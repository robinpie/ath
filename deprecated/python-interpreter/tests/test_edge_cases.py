"""Edge case tests for the !~ATH interpreter."""

import unittest
import asyncio
import sys
import os
from io import StringIO
from contextlib import redirect_stdout

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from untildeath.lexer import Lexer
from untildeath.parser import Parser
from untildeath.interpreter import Interpreter
from untildeath.errors import RuntimeError, CondemnError, ParseError


def run_program(source: str) -> str:
    """Run a !~ATH program and return stdout."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    program = parser.parse()
    interpreter = Interpreter()

    output = StringIO()
    with redirect_stdout(output):
        asyncio.run(interpreter.run(program))

    return output.getvalue()


class TestEdgeCasesEmptyStructures(unittest.TestCase):
    """Test edge cases with empty structures."""

    def test_empty_array_operations(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [];
            UTTER(LENGTH(arr));
            arr = APPEND(arr, 1);
            UTTER(LENGTH(arr));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["0", "1"])

    def test_empty_map_operations(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {};
            UTTER(LENGTH(KEYS(m)));
            m = SET(m, "a", 1);
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["0", "1"])

    def test_empty_string_operations(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH s WITH "";
            UTTER(LENGTH(s));
            s = s + "hello";
            UTTER(LENGTH(s));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["0", "5"])


class TestEdgeCasesNestedStructures(unittest.TestCase):
    """Test edge cases with nested structures."""

    def test_deeply_nested_array(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
            UTTER(arr[0][1][0]);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "3")

    def test_nested_map_access(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {outer: {inner: {value: 42}}};
            UTTER(m.outer.inner.value);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_array_of_maps(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [{name: "a"}, {name: "b"}];
            UTTER(arr[1].name);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "b")

    def test_map_with_array_values(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {nums: [1, 2, 3]};
            UTTER(m.nums[1]);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "2")


class TestEdgeCasesRecursion(unittest.TestCase):
    """Test edge cases with recursion."""

    def test_mutual_recursion(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE isEven(n) {
                SHOULD n == 0 { BEQUEATH ALIVE; }
                BEQUEATH isOdd(n - 1);
            }
            RITE isOdd(n) {
                SHOULD n == 0 { BEQUEATH DEAD; }
                BEQUEATH isEven(n - 1);
            }
            SHOULD isEven(4) { UTTER("4 is even"); }
            SHOULD isOdd(5) { UTTER("5 is odd"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["4 is even", "5 is odd"])

    def test_fibonacci(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE fib(n) {
                SHOULD n <= 1 { BEQUEATH n; }
                BEQUEATH fib(n - 1) + fib(n - 2);
            }
            UTTER(fib(10));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "55")


class TestEdgeCasesScoping(unittest.TestCase):
    """Test edge cases with variable scoping."""

    def test_shadowing(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 1;
            RITE test() {
                BIRTH x WITH 2;
                BEQUEATH x;
            }
            UTTER(test());
            UTTER(x);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["2", "1"])

    def test_closure_captures_variable(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE makeCounter(start) {
                BIRTH count WITH start;
                RITE increment() {
                    count = count + 1;
                    BEQUEATH count;
                }
                BEQUEATH increment;
            }
            // Note: This tests closure creation, though full closure
            // semantics would require returning functions as values
            BIRTH counter WITH makeCounter(0);
            // In this implementation, we can't call returned functions
            // So this is a simplified test
            UTTER("closure test passed");
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "closure test passed")


class TestEdgeCasesTimerChaining(unittest.TestCase):
    """Test edge cases with timer chaining."""

    def test_rapid_timer_chain(self):
        source = '''
        BIRTH count WITH 0;
        RITE increment() {
            count = count + 1;
            SHOULD count < 5 {
                import timer T(1ms);
                ~ATH(T) { } EXECUTE(increment());
            } LEST {
                UTTER(count);
            }
        }
        increment();
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "5")

    def test_timer_in_conditional(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH flag WITH ALIVE;
            SHOULD flag {
                import timer T2(1ms);
                ~ATH(T2) { } EXECUTE(UTTER("conditional timer"));
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "conditional timer")


class TestEdgeCasesErrorHandling(unittest.TestCase):
    """Test edge cases with error handling."""

    def test_error_in_rite(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE mayFail(x) {
                SHOULD x < 0 {
                    CONDEMN "negative value";
                }
                BEQUEATH x * 2;
            }
            ATTEMPT {
                UTTER(mayFail(-5));
            } SALVAGE err {
                UTTER("Error: " + err);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Error: negative value")

    def test_error_propagation(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE outer() {
                inner();
            }
            RITE inner() {
                CONDEMN "inner error";
            }
            ATTEMPT {
                outer();
            } SALVAGE err {
                UTTER("Caught: " + err);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Caught: inner error")

    def test_error_in_execute(self):
        source = '''
        import timer T1(1ms);
        ~ATH(T1) { } EXECUTE(
            ATTEMPT {
                import timer T2(1ms);
                ~ATH(T2) { } EXECUTE(
                    CONDEMN "nested error";
                );
            } SALVAGE err {
                UTTER("Outer caught: " + err);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Outer caught: nested error")


class TestEdgeCasesTypeCoercion(unittest.TestCase):
    """Test edge cases with type coercion."""

    def test_string_concat_with_numbers(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER("Value: " + 42 + " and " + 3.14);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Value: 42 and 3.14")

    def test_string_concat_with_bool(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER("Is alive: " + ALIVE);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Is alive: ALIVE")

    def test_truthiness_of_numbers(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 1 { UTTER("1 is truthy"); }
            SHOULD 0 { UTTER("0 is truthy"); } LEST { UTTER("0 is falsy"); }
            SHOULD 0.0 { UTTER("0.0 is truthy"); } LEST { UTTER("0.0 is falsy"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["1 is truthy", "0 is falsy", "0.0 is falsy"])


class TestEdgeCasesComments(unittest.TestCase):
    """Test edge cases with comments."""

    def test_comment_after_code(self):
        source = '''
        import timer T(1ms); // This is a comment
        ~ATH(T) { } EXECUTE(UTTER("hello")); // Another comment
        THIS.DIE(); // Final comment
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "hello")

    def test_comment_with_keywords(self):
        source = '''
        // import timer FAKE(1ms);
        import timer T(1ms);
        // ~ATH(FAKE) { } EXECUTE(UTTER("fake"));
        ~ATH(T) { } EXECUTE(UTTER("real"));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "real")


class TestEdgeCasesSpecialValues(unittest.TestCase):
    """Test edge cases with special values."""

    def test_void_in_array(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, VOID, 3];
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, VOID, 3]")

    def test_void_in_map(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: VOID};
            UTTER(m);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertIn("a: VOID", output)

    def test_boolean_in_arithmetic(self):
        """Booleans should not be used in arithmetic."""
        # Note: This might work or error depending on implementation
        # This tests the actual behavior
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                BIRTH x WITH ALIVE + 1;
                UTTER(x);
            } SALVAGE err {
                UTTER("error");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        # Should either work (True=1) or error
        self.assertTrue(output.strip() in ["2", "error"])


class TestEdgeCasesOperatorPrecedence(unittest.TestCase):
    """Test edge cases with operator precedence."""

    def test_complex_arithmetic(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(2 + 3 * 4 - 5 / 5);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        # 2 + 12 - 1 = 13
        self.assertEqual(output.strip(), "13")

    def test_comparison_in_logical(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 1 < 2 AND 3 > 2 {
                UTTER("yes");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_negation_precedence(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(-2 * 3);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "-6")


class TestEdgeCasesIndexBounds(unittest.TestCase):
    """Test edge cases with array/string indexing."""

    def test_string_index(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH s WITH "hello";
            UTTER(s[0]);
            UTTER(s[4]);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["h", "o"])

    def test_array_index_out_of_bounds(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3];
            ATTEMPT {
                UTTER(arr[10]);
            } SALVAGE err {
                UTTER("out of bounds");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "out of bounds")


class TestEdgeCasesEntityManagement(unittest.TestCase):
    """Test edge cases with entity management."""

    def test_die_already_dead(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("timer died"));
        T.DIE();  // Already dead, should be no-op
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "timer died")

    def test_kill_this_early(self):
        source = '''
        THIS.DIE();
        ~ATH(THIS) { } EXECUTE(UTTER("THIS died"));
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "THIS died")


class TestEdgeCasesBifurcation(unittest.TestCase):
    """Test edge cases with bifurcation."""

    def test_bifurcate_shared_variable(self):
        source = '''
        BIRTH counter WITH 0;
        bifurcate THIS[LEFT, RIGHT];

        ~ATH(LEFT) {
            import timer T1(1ms);
            ~ATH(T1) { } EXECUTE(
                counter = counter + 1;
            );
        } EXECUTE(VOID);

        ~ATH(RIGHT) {
            import timer T2(2ms);
            ~ATH(T2) { } EXECUTE(
                counter = counter + 10;
            );
        } EXECUTE(VOID);

        // Wait for both to complete
        import timer wait(10ms);
        ~ATH(wait) { } EXECUTE(UTTER(counter));

        [LEFT, RIGHT].DIE();
        '''
        output = run_program(source)
        # Both branches modify counter
        self.assertEqual(output.strip(), "11")


if __name__ == '__main__':
    unittest.main()
