"""Textual-based TUI for the !~ATH debugger."""

import asyncio
import sys
import time
from typing import Any, Optional
from textual.app import App, ComposeResult
from textual.containers import Container
from textual.widgets import Header, Footer, Static, DataTable, Tree, RichLog, TextArea
from textual.binding import Binding
from rich.text import Text

from .debugger import Debugger, DebuggerState, STATEMENT_TYPES
from .interpreter import Interpreter, Scope
from .ast_nodes import Statement
from .errors import TildeAthError


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

    #source-editor {
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
        Binding("e", "toggle_edit", "Edit"),
        Binding("ctrl+s", "save_and_run", "Save & Run", priority=True),
        Binding("escape", "exit_edit", "Exit Edit", show=False),
        Binding("q", "quit", "Quit"),
    ]

    def __init__(self, source_code: str, program_ast, interpreter_cls, filename: str = "Unknown"):
        super().__init__()
        self.source_code = source_code
        self.program_ast = program_ast
        self.interpreter_cls = interpreter_cls
        self.filename = filename
        self.sub_title = filename
        self.editing = False
        self.debugger: Optional[TextualDebugger] = None
        self.interpreter_task: Optional[asyncio.Task] = None

    def compose(self) -> ComposeResult:
        yield StaticHeader(icon="♊")
        
        # Left column: Source Code
        yield Container(
            Static("Source Code", classes="box-title", id="source-title"),
            TextArea(
                self.source_code,
                id="source-editor",
                language="java",
                theme="monokai",
                read_only=True,
                show_line_numbers=True,
                tab_behavior="indent",
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
        self.source_editor = self.query_one("#source-editor", TextArea)
        self.source_title = self.query_one("#source-title", Static)
        self.source_container = self.query_one("#source-container", Container)
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
        # Move TextArea cursor to current execution line (skip if user is editing)
        if not self.editing:
            target_row = max(0, step_info.line - 1)
            self.source_editor.move_cursor((target_row, 0))
            self.source_editor.scroll_cursor_visible()

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

    def _update_mode_indicator(self):
        """Update visual indicators based on editing state."""
        if self.editing:
            self.source_title.update("Source Code [EDITING]")
            self.source_container.styles.border = ("solid", "magenta")
        else:
            self.source_title.update("Source Code")
            self.source_container.styles.border = ("solid", "green")

    def action_toggle_edit(self):
        """Toggle between debug mode and edit mode."""
        if self.editing:
            # Exit edit mode without saving
            self.editing = False
            self.source_editor.read_only = True
            self.source_editor.text = self.source_code
            self._update_mode_indicator()
            self.set_focus(None)
        else:
            # Enter edit mode — pause interpreter if running
            if self.debugger and self.debugger.state == DebuggerState.RUNNING:
                self.debugger.state = DebuggerState.STEPPING
            self.editing = True
            self.source_editor.read_only = False
            self._update_mode_indicator()
            self.source_editor.focus()

    def action_exit_edit(self):
        """Exit edit mode on Escape without saving."""
        if self.editing:
            self.editing = False
            self.source_editor.read_only = True
            self.source_editor.text = self.source_code
            self._update_mode_indicator()
            self.set_focus(None)

    def action_save_and_run(self):
        """Save edited source and re-run the program."""
        if not self.editing:
            return

        from .lexer import Lexer
        from .parser import Parser

        new_source = self.source_editor.text

        # Try to lex + parse before committing
        try:
            lexer = Lexer(new_source)
            tokens = lexer.tokenize()
            parser = Parser(tokens)
            new_ast = parser.parse()
        except TildeAthError as e:
            self.program_output.write(f"[bold red]Parse error: {e}[/]")
            return

        # Save to disk if we have a real file path
        if self.filename and self.filename != "Unknown" and self.filename != "<stdin>":
            try:
                from pathlib import Path
                Path(self.filename).write_text(new_source, encoding='utf-8')
                self.program_output.write(f"[bold green]Saved to {self.filename}[/]")
            except IOError as e:
                self.program_output.write(f"[bold red]Save failed: {e}[/]")
                return

        # Update internal state
        self.source_code = new_source
        self.program_ast = new_ast

        # Exit edit mode
        self.editing = False
        self.source_editor.read_only = True
        self._update_mode_indicator()
        self.set_focus(None)

        # Reset and restart
        self._do_reset()

    def _do_reset(self):
        """Core reset logic: stop interpreter, clear UI, restart."""
        # Stop the current interpreter
        if self.debugger:
            self.debugger.state = DebuggerState.QUIT
            self.debugger.resume()
        if self.interpreter_task and not self.interpreter_task.done():
            self.interpreter_task.cancel()

        # Clear UI
        self.source_editor.move_cursor((0, 0))
        self.scope_tree.clear()
        self.scope_tree.root.expand()
        self.entities_table.clear()
        self.program_output.clear()
        self.program_output.write("[bold cyan]Program restarted.[/]")

        # Create a fresh debugger and start again
        self.debugger = TextualDebugger(self.source_code, self)
        self.interpreter_task = asyncio.create_task(self.run_interpreter())

    def action_step(self):
        """Handle Step action."""
        if self.editing:
            return
        if self.debugger:
            self.debugger.state = DebuggerState.STEPPING
            self.debugger.resume()

    def action_continue(self):
        """Handle Continue action."""
        if self.editing:
            return
        if self.debugger:
            self.debugger.state = DebuggerState.RUNNING
            self.debugger.resume()

    def action_reset(self):
        """Handle Reset action — restart the program from the beginning."""
        if self.editing:
            # Discard unsaved changes and exit edit mode
            self.editing = False
            self.source_editor.read_only = True
            self.source_editor.text = self.source_code
            self._update_mode_indicator()
            self.set_focus(None)
        self._do_reset()

    def action_quit(self):
        """Handle Quit action."""
        if self.debugger:
            self.debugger.state = DebuggerState.QUIT
            self.debugger.resume()
        self.exit()
