"""Tests for the !~ATH interpreter."""

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
from untildeath.errors import RuntimeError, CondemnError


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


def run_program_async(source: str):
    """Run a !~ATH program asynchronously."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    program = parser.parse()
    interpreter = Interpreter()
    return asyncio.run(interpreter.run(program))


class TestInterpreterBasics(unittest.TestCase):
    """Test basic interpreter functionality."""

    def test_hello_world(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Hello, world!"));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Hello, world!")

    def test_empty_program(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(VOID);
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "")


class TestInterpreterVariables(unittest.TestCase):
    """Test variable operations."""

    def test_birth_and_utter(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 42;
            UTTER(x);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_variable_reassignment(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            x = 10;
            UTTER(x);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "10")

    def test_entomb_constant(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ENTOMB PI WITH 3.14159;
            UTTER(PI);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "3.14159")

    def test_multiple_variables(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            BIRTH y WITH 10;
            BIRTH z WITH x + y;
            UTTER(z);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "15")


class TestInterpreterArithmetic(unittest.TestCase):
    """Test arithmetic operations."""

    def test_addition(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(5 + 3));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "8")

    def test_subtraction(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(10 - 4));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "6")

    def test_multiplication(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(7 * 6));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_division(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(20 / 4));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "5")

    def test_modulo(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(17 % 5));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "2")

    def test_negative_numbers(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(-5 + 3));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "-2")

    def test_float_arithmetic(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(3.5 + 1.5));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "5.0")

    def test_operator_precedence(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(2 + 3 * 4));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "14")

    def test_parentheses(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER((2 + 3) * 4));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "20")


class TestInterpreterComparison(unittest.TestCase):
    """Test comparison operations."""

    def test_equal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 == 5 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_not_equal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 != 3 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_less_than(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 3 < 5 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_greater_than(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 > 3 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_less_equal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 <= 5 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_greater_equal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 >= 5 { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")


class TestInterpreterBitwise(unittest.TestCase):
    """Test bitwise operations."""

    def test_bitwise_and(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 & 13));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "12")

    def test_bitwise_or(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 | 13));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "61")

    def test_bitwise_xor(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 ^ 13));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "49")

    def test_bitwise_not(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(~60));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "-61")

    def test_left_shift(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 << 2));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "240")

    def test_right_shift(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 >> 2));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "15")

    def test_bitwise_precedence(self):
        # 1 | 2 & 3 -> 1 | (2 & 3) = 1 | 2 = 3
        # 1 & 3 << 1 -> 1 & (3 << 1) = 1 & 6 = 0
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(1 | 2 & 3);
            UTTER(1 & 3 << 1);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["3", "0"])


class TestInterpreterLogical(unittest.TestCase):
    """Test logical operations."""

    def test_and_true(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE AND ALIVE { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_and_false(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE AND DEAD { UTTER("yes"); } LEST { UTTER("no"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "no")

    def test_or_true(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD OR ALIVE { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_or_false(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD OR DEAD { UTTER("yes"); } LEST { UTTER("no"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "no")

    def test_not(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD NOT DEAD { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")

    def test_short_circuit_and(self):
        """AND short-circuits when first operand is falsy."""
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH called WITH DEAD;
            RITE setTrue() {
                called = ALIVE;
                BEQUEATH ALIVE;
            }
            // DEAD AND setTrue() - setTrue should not be called
            BIRTH x WITH DEAD AND setTrue();
            SHOULD called {
                UTTER("called");
            } LEST {
                UTTER("not called");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "not called")

    def test_short_circuit_or(self):
        """OR short-circuits when first operand is truthy."""
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH called WITH DEAD;
            RITE setTrue() {
                called = ALIVE;
                BEQUEATH ALIVE;
            }
            // ALIVE OR setTrue() - setTrue should not be called
            BIRTH x WITH ALIVE OR setTrue();
            SHOULD called {
                UTTER("called");
            } LEST {
                UTTER("not called");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "not called")


class TestInterpreterStrings(unittest.TestCase):
    """Test string operations."""

    def test_string_concatenation(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Hello, " + "world!"));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Hello, world!")

    def test_string_number_concat(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Value: " + 42));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Value: 42")

    def test_string_comparison(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD "abc" == "abc" { UTTER("yes"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "yes")


class TestInterpreterArrays(unittest.TestCase):
    """Test array operations."""

    def test_array_literal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3];
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, 2, 3]")

    def test_array_index(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [10, 20, 30];
            UTTER(arr[1]);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "20")

    def test_array_index_assignment(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3];
            arr[1] = 99;
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, 99, 3]")

    def test_empty_array(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [];
            UTTER(LENGTH(arr));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "0")

    def test_mixed_array(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, "two", ALIVE];
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, two, ALIVE]")


class TestInterpreterMaps(unittest.TestCase):
    """Test map operations."""

    def test_map_literal(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {x: 1, y: 2};
            UTTER(m);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertIn("x: 1", output)
        self.assertIn("y: 2", output)

    def test_map_member_access(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {name: "Karkat"};
            UTTER(m.name);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Karkat")

    def test_map_index_access(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {name: "Karkat"};
            UTTER(m["name"]);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Karkat")

    def test_map_member_assignment(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {x: 1};
            m.x = 99;
            UTTER(m.x);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "99")

    def test_empty_map(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {};
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "0")


class TestInterpreterConditionals(unittest.TestCase):
    """Test conditional statements."""

    def test_should_true(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE {
                UTTER("true branch");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "true branch")

    def test_should_false(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD {
                UTTER("true branch");
            }
            UTTER("after");
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "after")

    def test_should_lest(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD {
                UTTER("true");
            } LEST {
                UTTER("false");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "false")

    def test_chained_should(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 2;
            SHOULD x == 1 {
                UTTER("one");
            } LEST SHOULD x == 2 {
                UTTER("two");
            } LEST {
                UTTER("other");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "two")

    def test_truthy_values(self):
        """Test various truthy/falsy values."""
        truthy_tests = [
            ("1", "truthy"),
            ('"hello"', "truthy"),
            ("[1]", "truthy"),
            ("{x: 1}", "truthy"),
        ]
        for value, expected in truthy_tests:
            with self.subTest(value=value):
                source = f'''
                import timer T(1ms);
                ~ATH(T) {{ }} EXECUTE(
                    SHOULD {value} {{ UTTER("truthy"); }} LEST {{ UTTER("falsy"); }}
                );
                THIS.DIE();
                '''
                output = run_program(source)
                self.assertEqual(output.strip(), expected)

    def test_falsy_values(self):
        """Test various falsy values."""
        falsy_tests = [
            ("0", "falsy"),
            ('""', "falsy"),
            ("[]", "falsy"),
            ("{}", "falsy"),
            ("VOID", "falsy"),
            ("DEAD", "falsy"),
        ]
        for value, expected in falsy_tests:
            with self.subTest(value=value):
                source = f'''
                import timer T(1ms);
                ~ATH(T) {{ }} EXECUTE(
                    SHOULD {value} {{ UTTER("truthy"); }} LEST {{ UTTER("falsy"); }}
                );
                THIS.DIE();
                '''
                output = run_program(source)
                self.assertEqual(output.strip(), expected)


class TestInterpreterRites(unittest.TestCase):
    """Test rite (function) definitions and calls."""

    def test_simple_rite(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE greet() {
                UTTER("Hello!");
            }
            greet();
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Hello!")

    def test_rite_with_params(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE add(a, b) {
                BEQUEATH a + b;
            }
            UTTER(add(3, 4));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "7")

    def test_rite_with_bequeath(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE double(x) {
                BEQUEATH x * 2;
            }
            BIRTH result WITH double(21);
            UTTER(result);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_recursive_rite(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE factorial(n) {
                SHOULD n <= 1 {
                    BEQUEATH 1;
                }
                BEQUEATH n * factorial(n - 1);
            }
            UTTER(factorial(5));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "120")

    def test_rite_no_return(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE noReturn() {
                BIRTH x WITH 5;
            }
            BIRTH result WITH noReturn();
            UTTER(TYPEOF(result));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "VOID")

    def test_rite_closure(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH multiplier WITH 10;
            RITE multiply(x) {
                BEQUEATH x * multiplier;
            }
            UTTER(multiply(5));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "50")


class TestInterpreterErrorHandling(unittest.TestCase):
    """Test error handling with ATTEMPT/SALVAGE/CONDEMN."""

    def test_condemn_caught(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                CONDEMN "test error";
            } SALVAGE err {
                UTTER("Caught: " + err);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Caught: test error")

    def test_runtime_error_caught(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                BIRTH x WITH PARSE_INT("not a number");
            } SALVAGE err {
                UTTER("Caught error");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Caught error")

    def test_no_error_attempt(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                UTTER("no error");
            } SALVAGE err {
                UTTER("caught: " + err);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "no error")

    def test_nested_attempt(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                ATTEMPT {
                    CONDEMN "inner";
                } SALVAGE e1 {
                    CONDEMN "outer";
                }
            } SALVAGE e2 {
                UTTER("Got: " + e2);
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "Got: outer")


class TestInterpreterTimers(unittest.TestCase):
    """Test timer entities."""

    def test_timer_chain(self):
        source = '''
        import timer T1(1ms);
        ~ATH(T1) { } EXECUTE(
            UTTER("first");
            import timer T2(1ms);
            ~ATH(T2) { } EXECUTE(
                UTTER("second");
            );
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["first", "second"])

    def test_timer_reuse_name(self):
        source = '''
        RITE count(n) {
            SHOULD n > 0 {
                UTTER(n);
                import timer T(1ms);
                ~ATH(T) { } EXECUTE(count(n - 1));
            }
        }
        count(3);
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["3", "2", "1"])


class TestInterpreterEntityCombinations(unittest.TestCase):
    """Test entity combination operators."""

    def test_entity_or(self):
        source = '''
        import timer T1(10ms);
        import timer T2(1ms);
        ~ATH(T1 || T2) { } EXECUTE(UTTER("done"));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "done")

    def test_entity_and(self):
        source = '''
        import timer T1(1ms);
        import timer T2(1ms);
        ~ATH(T1 && T2) { } EXECUTE(UTTER("both done"));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "both done")

    def test_entity_not(self):
        source = '''
        import timer T(1s);
        ~ATH(!T) { } EXECUTE(UTTER("timer exists"));
        T.DIE();
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "timer exists")


class TestInterpreterBifurcation(unittest.TestCase):
    """Test bifurcation and concurrent execution."""

    def test_simple_bifurcate(self):
        source = '''
        bifurcate THIS[LEFT, RIGHT];

        ~ATH(LEFT) {
            import timer T1(1ms);
            ~ATH(T1) { } EXECUTE(UTTER("left"));
        } EXECUTE(VOID);

        ~ATH(RIGHT) {
            import timer T2(1ms);
            ~ATH(T2) { } EXECUTE(UTTER("right"));
        } EXECUTE(VOID);

        [LEFT, RIGHT].DIE();
        '''
        output = run_program(source)
        lines = set(output.strip().split('\n'))
        self.assertEqual(lines, {"left", "right"})


class TestInterpreterBuiltins(unittest.TestCase):
    """Test built-in rites."""

    def test_utter_multiple_args(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(1, 2, 3));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "1 2 3")

    def test_typeof(self):
        tests = [
            ("42", "INTEGER"),
            ("3.14", "FLOAT"),
            ('"hello"', "STRING"),
            ("ALIVE", "BOOLEAN"),
            ("VOID", "VOID"),
            ("[1, 2]", "ARRAY"),
            ("{x: 1}", "MAP"),
        ]
        for value, expected in tests:
            with self.subTest(value=value):
                source = f'''
                import timer T(1ms);
                ~ATH(T) {{ }} EXECUTE(UTTER(TYPEOF({value})));
                THIS.DIE();
                '''
                output = run_program(source)
                self.assertEqual(output.strip(), expected)

    def test_length_string(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LENGTH("hello")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "5")

    def test_length_array(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LENGTH([1, 2, 3])));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "3")

    def test_parse_int(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(PARSE_INT("42")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_parse_float(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(PARSE_FLOAT("3.14")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "3.14")

    def test_string_conversion(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(STRING(42)));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42")

    def test_int_conversion(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(INT(3.7)));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "3")

    def test_float_conversion(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(FLOAT(42)));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "42.0")

    def test_append(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2];
            arr = APPEND(arr, 3);
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, 2, 3]")

    def test_prepend(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [2, 3];
            arr = PREPEND(arr, 1);
            UTTER(arr);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, 2, 3]")

    def test_slice(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3, 4, 5];
            UTTER(SLICE(arr, 1, 4));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[2, 3, 4]")

    def test_first(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(FIRST([10, 20, 30])));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "10")

    def test_last(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LAST([10, 20, 30])));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "30")

    def test_concat(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(CONCAT([1, 2], [3, 4])));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[1, 2, 3, 4]")

    def test_keys(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            UTTER(KEYS(m));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        # Keys order may vary
        self.assertIn("a", output)
        self.assertIn("b", output)

    def test_values(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            UTTER(VALUES(m));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertIn("1", output)
        self.assertIn("2", output)

    def test_has(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1};
            SHOULD HAS(m, "a") { UTTER("yes"); }
            SHOULD NOT HAS(m, "b") { UTTER("no b"); }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["yes", "no b"])

    def test_set(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1};
            m = SET(m, "b", 2);
            UTTER(m.b);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "2")

    def test_delete(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            m = DELETE(m, "a");
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "1")

    def test_split(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(SPLIT("a,b,c", ",")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "[a, b, c]")

    def test_join(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(JOIN(["a", "b", "c"], "-")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "a-b-c")

    def test_substring(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(SUBSTRING("hello", 1, 4)));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "ell")

    def test_uppercase(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(UPPERCASE("hello")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "HELLO")

    def test_lowercase(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LOWERCASE("HELLO")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "hello")

    def test_trim(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(TRIM("  hello  ")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "hello")

    def test_replace(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(REPLACE("hello", "l", "w")));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "hewwo")

    def test_random_range(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH r WITH RANDOM();
            SHOULD r >= 0 AND r < 1 {
                UTTER("valid");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "valid")

    def test_random_int(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH r WITH RANDOM_INT(1, 6);
            SHOULD r >= 1 AND r <= 6 {
                UTTER("valid");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "valid")

    def test_time(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH t WITH TIME();
            SHOULD t > 0 {
                UTTER("valid");
            }
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "valid")


class TestInterpreterScoping(unittest.TestCase):
    """Test variable scoping rules."""

    def test_global_scope(self):
        source = '''
        BIRTH x WITH 10;
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(x));
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "10")

    def test_local_scope(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            RITE test() {
                BIRTH x WITH 10;
                BEQUEATH x;
            }
            UTTER(test());
            UTTER(x);
        );
        THIS.DIE();
        '''
        output = run_program(source)
        lines = output.strip().split('\n')
        self.assertEqual(lines, ["10", "5"])

    def test_closure_scope(self):
        source = '''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            RITE inner() {
                BEQUEATH x;
            }
            UTTER(inner());
        );
        THIS.DIE();
        '''
        output = run_program(source)
        self.assertEqual(output.strip(), "5")


if __name__ == '__main__':
    unittest.main()
