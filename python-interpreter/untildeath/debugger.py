"""Stepping debugger for the !~ATH interpreter."""

import asyncio
import concurrent.futures
import sys
from dataclasses import dataclass
from enum import Enum, auto
from typing import Any, Dict, List, Optional

from .ast_nodes import (
    Statement, ImportStmt, BifurcateStmt, AthLoop, DieStmt,
    VarDecl, ConstDecl, Assignment, RiteDef, Conditional,
    AttemptSalvage, CondemnStmt, BequeathStmt, ExprStmt
)
from .entities import Entity, BranchEntity


STATEMENT_TYPES = (
    ImportStmt, BifurcateStmt, AthLoop, DieStmt,
    VarDecl, ConstDecl, Assignment, RiteDef, Conditional,
    AttemptSalvage, CondemnStmt, BequeathStmt, ExprStmt
)


class DebuggerState(Enum):
    """State of the debugger."""
    RUNNING = auto()   # Running freely
    STEPPING = auto()  # Pausing at each step
    PAUSED = auto()    # Currently paused and waiting for input
    QUIT = auto()      # User requested to quit


@dataclass
class StepInfo:
    """Information about the current execution step."""
    line: int
    column: int
    node_type: str
    description: str
    branch: str = "MAIN"
    source_line: str = ""


class AsyncInputHandler:
    """Handles user input without blocking the asyncio event loop."""

    def __init__(self):
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)

    async def get_input(self, prompt: str) -> str:
        """Get input from stdin asynchronously."""
        loop = asyncio.get_running_loop()
        return await loop.run_in_executor(self._executor, input, prompt)


class Debugger:
    """Interactive stepping debugger."""

    def __init__(self, source_code: str):
        self.state = DebuggerState.STEPPING  # Default to stepping mode if debugger is active
        self.source_lines = source_code.splitlines()
        self.input_handler = AsyncInputHandler()
        self.last_command = "step"  # Default command for empty input

    async def step_hook(self, node: Any, scope: 'Scope', branch_context: str, interpreter: 'Interpreter') -> bool:
        """
        Called before executing a statement.
        Returns False if execution should abort (QUIT), True otherwise.
        """
        # Only pause on statements, not expressions (unless they are ExprStmt)
        if not isinstance(node, STATEMENT_TYPES):
            return True

        # If we are just running, don't pause
        if self.state == DebuggerState.RUNNING:
            return True

        # If we are stepping, pause and show info
        if self.state == DebuggerState.STEPPING:
            self.state = DebuggerState.PAUSED
            
            step_info = self._create_step_info(node, branch_context)
            self._display_step(step_info, scope, interpreter)

            while self.state == DebuggerState.PAUSED:
                try:
                    cmd_input = await self.input_handler.get_input("(step) ")
                    cmd_input = cmd_input.strip()
                    
                    if not cmd_input:
                        cmd = self.last_command
                    else:
                        cmd = cmd_input.lower()
                        self.last_command = cmd

                    await self.process_command(cmd, scope, interpreter)
                except EOFError:
                    self.state = DebuggerState.QUIT
                    return False
        
        return self.state != DebuggerState.QUIT

    def _create_step_info(self, node: Any, branch_context: str) -> StepInfo:
        """Create StepInfo from a node."""
        line = node.line
        column = node.column
        
        # Get source line context
        source_line = ""
        if 0 < line <= len(self.source_lines):
            source_line = self.source_lines[line - 1]

        node_type = type(node).__name__
        description = self._describe_node(node)

        return StepInfo(
            line=line,
            column=column,
            node_type=node_type,
            description=description,
            branch=branch_context,
            source_line=source_line
        )

    def _describe_node(self, node: Any) -> str:
        """Generate a human-readable description of the node."""
        if isinstance(node, ImportStmt):
            return f"Importing {node.entity_type} entity '{node.name}'"
        if isinstance(node, BifurcateStmt):
            return f"Bifurcating '{node.entity}' into '{node.branch1}' and '{node.branch2}'"
        if isinstance(node, AthLoop):
            if isinstance(node.entity_expr, str): # Branch mode logic might pass string
                 return f"Executing branch '{node.entity_expr}'"
            # Attempt to stringify entity expr roughly
            return f"~ATH loop waiting on entity"
        if isinstance(node, DieStmt):
            return "Invoking .DIE()"
        if isinstance(node, VarDecl):
            return f"Declaring variable '{node.name}'"
        if isinstance(node, ConstDecl):
            return f"Declaring constant '{node.name}'"
        if isinstance(node, Assignment):
            return "Assignment"
        if isinstance(node, RiteDef):
            return f"Defining rite '{node.name}'"
        if isinstance(node, Conditional):
            return "Conditional check (SHOULD)"
        if isinstance(node, AttemptSalvage):
            return "Entering ATTEMPT block"
        if isinstance(node, CondemnStmt):
            return "Throwing error (CONDEMN)"
        if isinstance(node, BequeathStmt):
            return "Returning value (BEQUEATH)"
        if isinstance(node, ExprStmt):
            return "Expression statement"
        return str(node)

    def _display_step(self, info: StepInfo, scope: 'Scope', interpreter: 'Interpreter'):
        """Print the step display."""
        print("=" * 80)
        print(f"Step | Branch: {info.branch} | Line {info.line}, Col {info.column}")
        print("-" * 80)
        
        if info.source_line:
            print("SOURCE:")
            print(f"   {info.line} | {info.source_line}")
            # Identify indentation to place marker correctly
            indent = len(info.source_line) - len(info.source_line.lstrip())
            marker_indent = max(0, info.column - 1)
            # Adjust if column is absolute vs relative to code
            # But usually column from parser is 1-based index in line
            print(" " * (6 + marker_indent) + "^^^^^")
        
        print(f"\nSTATEMENT: {info.node_type}")
        print(f"  {info.description}")
        
        # Show top variables in current scope
        print("\nSCOPE VARIABLES:")
        count = 0
        for name, value in scope.variables.items():
            type_name = type(value).__name__
            # Helper to show value nicely
            val_str = str(value)
            if isinstance(value, str):
                val_str = f'"{value}"'
            print(f"  {name} = {val_str} ({type_name})")
            count += 1
            if count >= 5:
                print("  ... (use 'v' to see all)")
                break
        if count == 0:
            print("  (empty)")

        # Show pending tasks summary
        print(f"\nPENDING TASKS: {len(interpreter._pending_tasks)}")
        
        print("=" * 80)
        print("Commands: [Enter]=step  [c]=continue  [v]=variables  [e]=entities  [q]=quit")

    async def process_command(self, cmd: str, scope: 'Scope', interpreter: 'Interpreter'):
        """Process a debugger command."""
        if cmd in ('s', 'step'):
            self.state = DebuggerState.STEPPING
            # Implicitly leaves PAUSED state effectively (caller handles loop break)
            # Actually, to exit the loop in step_hook, we need to change state from PAUSED
            # If we set it to STEPPING, the loop condition (state == PAUSED) becomes false.
            pass
            
        elif cmd in ('c', 'continue'):
            self.state = DebuggerState.RUNNING
            
        elif cmd in ('q', 'quit'):
            self.state = DebuggerState.QUIT
            
        elif cmd in ('v', 'vars', 'variables'):
            print("\n--- ALL VARIABLES ---")
            # Walk up the scope chain
            current = scope
            depth = 0
            while current:
                print(f"Scope Level {depth}:")
                if not current.variables:
                    print("  (empty)")
                for name, value in current.variables.items():
                    val_str = str(value)
                    if isinstance(value, str):
                        val_str = f'"{value}"'
                    print(f"  {name} = {val_str}")
                current = current.parent
                depth += 1
            print("---------------------")
            # Stay paused
            self.state = DebuggerState.PAUSED
            
        elif cmd in ('e', 'entities'):
            print("\n--- ENTITIES ---")
            for name, entity in interpreter.entities.items():
                status = "ALIVE" if entity.is_alive else "DEAD"
                kind = type(entity).__name__
                print(f"  {name:<15} : {status:<5} ({kind})")
            print("----------------")
            self.state = DebuggerState.PAUSED
            
        elif cmd in ('t', 'tasks'):
             print("\n--- PENDING TASKS ---")
             for i, task in enumerate(interpreter._pending_tasks):
                 print(f"  Task {i}: {task}")
             print("---------------------")
             self.state = DebuggerState.PAUSED
             
        elif cmd in ('h', 'help', '?'):
            print("\n--- DEBUGGER HELP ---")
            print("  (Enter) / s / step   : Execute next statement")
            print("  c / continue         : Resume execution until next breakpoint or end")
            print("  v / variables        : Show all variables in current scope chain")
            print("  e / entities         : Show all entities and their status")
            print("  t / tasks            : Show pending async tasks")
            print("  q / quit             : Stop execution")
            print("---------------------")
            self.state = DebuggerState.PAUSED
            
        else:
            print(f"Unknown command: {cmd}")
            self.state = DebuggerState.PAUSED
