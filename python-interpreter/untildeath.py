#!/usr/bin/env python3
# Copyright (C) 2026 Robin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
"""!~ATH interpreter - command line interface."""

import asyncio
import sys
import argparse
from pathlib import Path

from untildeath.lexer import Lexer
from untildeath.parser import Parser
from untildeath.interpreter import Interpreter
from untildeath.errors import TildeAthError, DebuggerQuitException
from untildeath.debugger import Debugger, DebuggerState


def run_file(filepath: str, debug: bool = False) -> int:
    """Run a !~ATH source file."""
    path = Path(filepath)

    if not path.exists():
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        return 1

    try:
        source = path.read_text(encoding='utf-8')
    except IOError as e:
        print(f"Error reading file: {e}", file=sys.stderr)
        return 1

    return run_source(source, filepath, debug)


def run_source(source: str, filename: str = "<stdin>", debug: bool = False) -> int:
    """Run !~ATH source code."""
    try:
        # Lexical analysis
        lexer = Lexer(source)
        tokens = lexer.tokenize()

        # Parsing
        parser = Parser(tokens)
        program = parser.parse()

        # Debugger initialization
        debugger = None
        if debug:
            debugger = Debugger(source)
            print(f"Debugger enabled for {filename}")

        # Interpretation
        interpreter = Interpreter(debugger)
        asyncio.run(interpreter.run(program))

        return 0

    except DebuggerQuitException:
        print("\nDebugger quit.", file=sys.stderr)
        return 0
    except TildeAthError as e:
        print(f"Error in {filename}: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        return 130


def run_repl():
    """Run an interactive REPL."""
    print("!~ATH interpreter v1.0.0")
    print("'quit' to exit.")
    print("':step' to toggle debugger for next execution.")
    print()

    # For REPL, we accumulate code until we see a complete program
    buffer = []
    debug_next = False

    while True:
        try:
            prompt = "(debug) >>> " if debug_next else ">>> "
            if buffer:
                prompt = "... "
            
            line = input(prompt)

            if line.strip().lower() == 'quit':
                break
            
            if line.strip() == ':step':
                debug_next = not debug_next
                print(f"Debugger {'enabled' if debug_next else 'disabled'} for next run.")
                continue

            buffer.append(line)

            # Try to parse - if incomplete, continue accumulating
            source = '\n'.join(buffer)

            try:
                lexer = Lexer(source)
                tokens = lexer.tokenize()
                parser = Parser(tokens)
                program = parser.parse()

                # Successfully parsed - execute
                debugger = None
                if debug_next:
                    debugger = Debugger(source)
                
                interpreter = Interpreter(debugger)
                asyncio.run(interpreter.run(program))
                
                buffer = []
                debug_next = False  # Reset after run

            except DebuggerQuitException:
                print("Debugger quit.")
                buffer = []
                debug_next = False
            except TildeAthError as e:
                # Check if it might be incomplete
                error_msg = str(e).lower()
                if 'unexpected token: eof' in error_msg or 'expected' in error_msg:
                    # Might be incomplete, continue accumulating
                    continue
                else:
                    # Real error
                    print(f"Error: {e}", file=sys.stderr)
                    buffer = []

        except EOFError:
            print()
            break
        except KeyboardInterrupt:
            print("\nInterrupted. Type 'quit' to exit.")
            buffer = []


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description="!~ATH Interpreter")
    parser.add_argument("file", nargs="?", help="Source file to run")
    parser.add_argument("--step", "-d", "--debug", action="store_true", help="Enable stepping debugger (CLI)")
    parser.add_argument("--tui", action="store_true", help="Enable TUI debugger (Textual)")
    
    args = parser.parse_args()
    
    if args.file:
        if args.tui:
            # Run TUI
            from untildeath.tui import AthDebuggerApp
            from untildeath.lexer import Lexer
            from untildeath.parser import Parser
            from untildeath.interpreter import Interpreter
            
            try:
                path = Path(args.file)
                if not path.exists():
                    print(f"Error: File not found: {args.file}", file=sys.stderr)
                    sys.exit(1)
                
                source = path.read_text(encoding='utf-8')
                
                # Parse first
                lexer = Lexer(source)
                tokens = lexer.tokenize()
                parser = Parser(tokens)
                program = parser.parse()
                
                # Run App
                app = AthDebuggerApp(source, program, Interpreter, filename=args.file)
                app.run()
                sys.exit(0)
            except Exception as e:
                print(f"Error starting TUI: {e}", file=sys.stderr)
                sys.exit(1)
        else:
            sys.exit(run_file(args.file, debug=args.step))
    else:
        run_repl()


if __name__ == '__main__':
    main()
