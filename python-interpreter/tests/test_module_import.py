"""Tests for !~ATH module imports via watcher entity."""

import unittest
import asyncio
import sys
import os
import tempfile
from io import StringIO
from contextlib import redirect_stdout, redirect_stderr

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from untildeath.lexer import Lexer
from untildeath.parser import Parser
from untildeath.interpreter import Interpreter
from untildeath.errors import RuntimeError as AthRuntimeError, TildeAthError


def run_program(source: str, source_file: str = None) -> str:
    """Run a !~ATH program and return stdout."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    program = parser.parse()
    interpreter = Interpreter(source_file=source_file)

    output = StringIO()
    with redirect_stdout(output), redirect_stderr(StringIO()):
        asyncio.run(interpreter.run(program))

    return output.getvalue()


def ath_path(path):
    """Convert OS path to forward slashes for embedding in !~ATH strings."""
    return path.replace('\\', '/')


def write_module(tmpdir, filename, content):
    """Write a module file and return its path."""
    path = os.path.join(tmpdir, filename)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    return path


class TestModuleImport(unittest.TestCase):
    """Test module import functionality."""

    def test_basic_rite_import(self):
        """Module defines a rite, caller invokes it via W.rite()."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "mathlib.~ATH", '''
                RITE add(a, b) {
                    BEQUEATH a + b;
                }
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "mathlib.~ATH"))}");
                BIRTH result WITH W.add(3, 4);
                UTTER(result);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "7")

    def test_variable_export(self):
        """Access a top-level BIRTH variable from module."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "config.~ATH", '''
                BIRTH greeting WITH "Hello from module";
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "config.~ATH"))}");
                UTTER(W.greeting);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "Hello from module")

    def test_constant_export(self):
        """Access an ENTOMB constant from module."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "constants.~ATH", '''
                ENTOMB PI WITH 3;
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "constants.~ATH"))}");
                UTTER(W.PI);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "3")

    def test_nonexistent_export_error(self):
        """W.nope raises RuntimeError."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "lib.~ATH", '''
                BIRTH x WITH 1;
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "lib.~ATH"))}");
                UTTER(W.nope);
                THIS.DIE();
            '''
            with self.assertRaises(TildeAthError) as ctx:
                run_program(source, source_file=main_path)
            self.assertIn("no export", str(ctx.exception))

    def test_rite_with_closure(self):
        """Module rite references a module-local variable."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "greeter.~ATH", '''
                BIRTH prefix WITH "Hello, ";
                RITE greet(name) {
                    BEQUEATH prefix + name;
                }
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "greeter.~ATH"))}");
                BIRTH msg WITH W.greet("World");
                UTTER(msg);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "Hello, World")

    def test_non_ath_watcher_no_exports(self):
        """A .txt watcher has no module behavior."""
        with tempfile.TemporaryDirectory() as tmpdir:
            txt_path = write_module(tmpdir, "data.txt", "some data")
            main_path = write_module(tmpdir, "main.~ATH", '')

            # Non-.~ATH watcher should not be accessible as a value
            source = f'''
                import watcher W("{ath_path(txt_path)}");
                UTTER(W);
                THIS.DIE();
            '''
            with self.assertRaises(TildeAthError):
                run_program(source, source_file=main_path)

    def test_circular_import_detection(self):
        """A imports B imports A -> RuntimeError."""
        with tempfile.TemporaryDirectory() as tmpdir:
            a_path = os.path.join(tmpdir, "a.~ATH")
            b_path = os.path.join(tmpdir, "b.~ATH")

            with open(a_path, 'w') as f:
                f.write(f'''
                    import watcher B("{ath_path(b_path)}");
                    THIS.DIE();
                ''')

            with open(b_path, 'w') as f:
                f.write(f'''
                    import watcher A("{ath_path(a_path)}");
                    THIS.DIE();
                ''')

            source = f'''
                import watcher A("{ath_path(a_path)}");
                THIS.DIE();
            '''
            main_path = write_module(tmpdir, "main.~ATH", '')
            with self.assertRaises(TildeAthError) as ctx:
                run_program(source, source_file=main_path)
            self.assertIn("Circular import", str(ctx.exception))

    def test_module_syntax_error(self):
        """Module with bad syntax raises clear error."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "bad.~ATH", '''
                BIRTH x WITH ;
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "bad.~ATH"))}");
                THIS.DIE();
            '''
            with self.assertRaises(TildeAthError) as ctx:
                run_program(source, source_file=main_path)
            self.assertIn("Error in module", str(ctx.exception))

    def test_reimport_module(self):
        """Re-importing reloads exports from disk."""
        with tempfile.TemporaryDirectory() as tmpdir:
            mod_path = os.path.join(tmpdir, "counter.~ATH")
            with open(mod_path, 'w') as f:
                f.write('''
                    BIRTH val WITH 1;
                    THIS.DIE();
                ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(mod_path)}");
                UTTER(W.val);
                import watcher W("{ath_path(mod_path)}");
                UTTER(W.val);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            lines = output.strip().split('\n')
            self.assertEqual(lines[0], "1")
            self.assertEqual(lines[1], "1")

    def test_module_with_timer(self):
        """Module uses timers internally at load time."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "timermod.~ATH", '''
                BIRTH result WITH 0;
                import timer T(1ms);
                ~ATH(T) { } EXECUTE(result = 42;);
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "timermod.~ATH"))}");
                UTTER(W.result);
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "42")

    def test_multiple_rites(self):
        """Module exports several rites, all callable."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "multi.~ATH", '''
                RITE double(x) { BEQUEATH x * 2; }
                RITE triple(x) { BEQUEATH x * 3; }
                RITE negate(x) { BEQUEATH 0 - x; }
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher M("{ath_path(os.path.join(tmpdir, "multi.~ATH"))}");
                UTTER(M.double(5));
                UTTER(M.triple(5));
                UTTER(M.negate(5));
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            lines = output.strip().split('\n')
            self.assertEqual(lines[0], "10")
            self.assertEqual(lines[1], "15")
            self.assertEqual(lines[2], "-5")

    def test_module_typeof(self):
        """TYPEOF(W) returns 'MODULE'."""
        with tempfile.TemporaryDirectory() as tmpdir:
            write_module(tmpdir, "mod.~ATH", '''
                THIS.DIE();
            ''')

            main_path = write_module(tmpdir, "main.~ATH", '')
            source = f'''
                import watcher W("{ath_path(os.path.join(tmpdir, "mod.~ATH"))}");
                UTTER(TYPEOF(W));
                THIS.DIE();
            '''
            output = run_program(source, source_file=main_path)
            self.assertEqual(output.strip(), "MODULE")


if __name__ == '__main__':
    unittest.main()
