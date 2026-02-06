"""Textual-based TUI for the !~ATH debugger."""

import asyncio
import sys
import time
from typing import Any, Optional
from io import StringIO

from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, VerticalScroll
from textual.widgets import Header, Footer, Static, Button, DataTable, Tree, RichLog
from textual.binding import Binding
from rich.syntax import Syntax
from rich.text import Text

from .debugger import Debugger, DebuggerState, STATEMENT_TYPES
from .interpreter import Interpreter, Scope
from .ast_nodes import Statement


class TextualDebugger(Debugger):
    """Adapter to connect the Interpreter to the Textual UI."""

    def __init__(self, source_code: str, app: 'AthDebuggerApp'):
        super().__init__(source_code)
        self.app = app
        self._step_event = asyncio.Event()
        self.state = DebuggerState.STEPPING
        self._last_ui_update = 0.0
        self._latest_state = None

    async def step_hook(self, node: Any, scope: Scope, branch_context: str, interpreter: Interpreter) -> bool:
        """Called by the interpreter before executing a statement."""
        if not isinstance(node, STATEMENT_TYPES):
            return True

        step_info = self._create_step_info(node, branch_context)
        self._latest_state = (step_info, scope, interpreter)

        if self.state == DebuggerState.RUNNING:
            # Always log every step to program output
            self.app.log_step(step_info)
            # Throttle expensive panel updates (source, scope, entities)
            now = time.monotonic()
            if now - self._last_ui_update >= 0.05:
                self.app.update_panels(step_info, scope, interpreter)
                self._last_ui_update = now
            # Always yield so timers fire and keypresses are processed
            await asyncio.sleep(0)
            return self.state != DebuggerState.QUIT

        # Stepping mode: schedule update and wait for user input
        self.app.call_later(
            self.app.update_state,
            step_info,
            scope,
            interpreter
        )
        self._step_event.clear()
        await self._step_event.wait()

        return self.state != DebuggerState.QUIT

    def resume(self):
        """Resume execution (called by UI)."""
        self._step_event.set()


class StdoutRedirector:
    """Redirects sys.stdout to a Textual widget."""
    def __init__(self, widget: RichLog):
        self.widget = widget
        self.original_stdout = sys.stdout

    def write(self, text: str):
        if text:
            # We must schedule this because write() might be called from async context
            self.widget.app.call_later(self.widget.write, text)

    def flush(self):
        pass

    def __enter__(self):
        sys.stdout = self
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        sys.stdout = self.original_stdout


class StaticHeader(Header):
    """A Header that does not expand on click."""
    def on_click(self):
        pass


class AthDebuggerApp(App):
    """The Textual TUI Application."""

    TITLE = "!~ATH Single Stepper"

    CSS = """
    Screen {
        layout: grid;
        grid-size: 2 2;
        grid-columns: 2fr 1fr;
        grid-rows: 1fr 1fr;
    }

    #source-container {
        row-span: 2;
        column-span: 1;
        border: solid green;
        background: $surface;
        layout: vertical;
    }

    #source-scroll {
        height: 1fr;
    }

    #output-container {
        row-span: 1;
        column-span: 1;
        border: solid yellow;
        background: $surface;
    }

    #variables-container {
        row-span: 1;
        column-span: 1;
        border: solid blue;
        background: $surface;
    }

    .box-title {
        background: $accent;
        color: $text;
        padding: 0 1;
        text-align: center;
    }
    """

    BINDINGS = [
        Binding("s", "step", "Step"),
        Binding("c", "continue", "Continue"),
        Binding("r", "reset", "Reset"),
        Binding("q", "quit", "Quit"),
    ]

    def __init__(self, source_code: str, program_ast, interpreter_cls, filename: str = "Unknown"):
        super().__init__()
        self.source_code = source_code
        self.program_ast = program_ast
        self.interpreter_cls = interpreter_cls
        self.sub_title = filename
        self.debugger: Optional[TextualDebugger] = None
        self.interpreter_task: Optional[asyncio.Task] = None

    def compose(self) -> ComposeResult:
        yield StaticHeader(icon="♊")
        
        # Left column: Source Code
        yield Container(
            Static("Source Code", classes="box-title"),
            VerticalScroll(
                Static(id="source-view", expand=True),
                id="source-scroll",
            ),
            id="source-container"
        )

        # Top Right: Program Output
        yield Container(
            Static("Program Output", classes="box-title"),
            RichLog(id="program-output", highlight=True, markup=True),
            id="output-container"
        )

        # Bottom Right: Variables & Entities
        yield Container(
            Static("Variables & Entities", classes="box-title"),
            Tree("Scope", id="scope-tree"),
            DataTable(id="entities-table"),
            id="variables-container"
        )

        yield Footer()

    def on_mount(self) -> None:
        """Start the interpreter when the app mounts."""
        # Initialize widgets
        self.source_view = self.query_one("#source-view", Static)
        self.source_scroll = self.query_one("#source-scroll", VerticalScroll)
        self.scope_tree = self.query_one("#scope-tree", Tree)
        self.entities_table = self.query_one("#entities-table", DataTable)
        self.program_output = self.query_one("#program-output", RichLog)

        self.entities_table.add_columns("Entity", "State", "Type")

        # Create debugger linked to this app
        self.debugger = TextualDebugger(self.source_code, self)

        # Start the interpreter in a background task
        self.interpreter_task = asyncio.create_task(self.run_interpreter())

    async def run_interpreter(self):
        """Run the interpreter with stdout redirection."""
        interpreter = self.interpreter_cls(self.debugger)

        # Redirect stdout to our RichLog widget
        with StdoutRedirector(self.program_output):
            try:
                await interpreter.run(self.program_ast)
                # Final panel update so panels reflect end state
                if self.debugger._latest_state:
                    self.update_panels(*self.debugger._latest_state)
                self.program_output.write("[bold green]Program finished.[/]")
            except Exception as e:
                if self.debugger._latest_state:
                    self.update_panels(*self.debugger._latest_state)
                self.program_output.write(f"[bold red]Error: {e}[/]")

    def log_step(self, step_info):
        """Log a step to the program output."""
        self.program_output.write(
            f"[dim]Step: {step_info.node_type} at line {step_info.line} ({step_info.description})[/]"
        )

    def update_panels(self, step_info, scope, interpreter):
        """Update the source view, scope tree, and entities table."""
        # Update Source View with highlighting
        lines = self.source_code.splitlines()
        syntax = Syntax(self.source_code, "java", theme="monokai", line_numbers=True)
        if 0 < step_info.line <= len(lines):
            syntax.highlight_lines = {step_info.line}

        self.source_view.update(syntax)
        # Scroll the container so the current line is roughly centered
        visible_height = self.source_scroll.size.height
        target_y = max(0, step_info.line - 1 - visible_height // 2)
        self.source_scroll.scroll_to(y=target_y, animate=False)

        # Update Scope Tree
        self.scope_tree.clear()
        self.scope_tree.root.expand()

        # Walk up scope chain
        current = scope
        depth = 0
        while current:
            branch = self.scope_tree.root.add(f"Scope Level {depth}", expand=True)
            if not current.variables:
                branch.add("(empty)")
            for name, value in current.variables.items():
                val_str = str(value)
                if isinstance(value, str):
                    val_str = f'"{value}"'
                branch.add(f"{name} = {val_str}")
            current = current.parent
            depth += 1

        # Update Entities Table
        self.entities_table.clear()
        for name, entity in interpreter.entities.items():
            status = "ALIVE" if entity.is_alive else "DEAD"
            kind = type(entity).__name__
            status_styled = f"[green]{status}[/]" if status == "ALIVE" else f"[red]{status}[/]"
            self.entities_table.add_row(name, Text.from_markup(status_styled), kind)

    def update_state(self, step_info, scope, interpreter):
        """Update all UI: log the step and refresh panels."""
        self.log_step(step_info)
        self.update_panels(step_info, scope, interpreter)

    def action_step(self):
        """Handle Step action."""
        if self.debugger:
            self.debugger.state = DebuggerState.STEPPING
            self.debugger.resume()

    def action_continue(self):
        """Handle Continue action."""
        if self.debugger:
            self.debugger.state = DebuggerState.RUNNING
            self.debugger.resume()

    def action_reset(self):
        """Handle Reset action — restart the program from the beginning."""
        # Stop the current interpreter
        if self.debugger:
            self.debugger.state = DebuggerState.QUIT
            self.debugger.resume()
        if self.interpreter_task and not self.interpreter_task.done():
            self.interpreter_task.cancel()

        # Clear UI
        self.source_view.update("")
        self.source_scroll.scroll_to(y=0, animate=False)
        self.scope_tree.clear()
        self.scope_tree.root.expand()
        self.entities_table.clear()
        self.program_output.clear()
        self.program_output.write("[bold cyan]Program restarted.[/]")

        # Create a fresh debugger and start again
        self.debugger = TextualDebugger(self.source_code, self)
        self.interpreter_task = asyncio.create_task(self.run_interpreter())

    def action_quit(self):
        """Handle Quit action."""
        if self.debugger:
            self.debugger.state = DebuggerState.QUIT
            self.debugger.resume()
        self.exit()
