"""End-to-end tests: transpile !~ATH to C89, compile, run.

Ported from python-interpreter/tests/test_interpreter.py and test_edge_cases.py.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


# ---------------------------------------------------------------------------
# Basics
# ---------------------------------------------------------------------------

def test_hello_world():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Hello, world!"));
        THIS.DIE();
    ''').strip() == "Hello, world!"


def test_empty_program():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(VOID);
        THIS.DIE();
    ''').strip() == ""


# ---------------------------------------------------------------------------
# Variables
# ---------------------------------------------------------------------------

def test_birth_and_utter():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 42;
            UTTER(x);
        );
        THIS.DIE();
    ''').strip() == "42"


def test_variable_reassignment():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            x = 10;
            UTTER(x);
        );
        THIS.DIE();
    ''').strip() == "10"


def test_entomb_constant():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ENTOMB PI WITH 3.14159;
            UTTER(PI);
        );
        THIS.DIE();
    ''').strip() == "3.14159"


def test_multiple_variables():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            BIRTH y WITH 10;
            BIRTH z WITH x + y;
            UTTER(z);
        );
        THIS.DIE();
    ''').strip() == "15"


def test_global_scope():
    assert run_ath_via_c('''
        BIRTH x WITH 10;
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(x));
        THIS.DIE();
    ''').strip() == "10"


# ---------------------------------------------------------------------------
# Arithmetic
# ---------------------------------------------------------------------------

def test_addition():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(5 + 3));
        THIS.DIE();
    ''').strip() == "8"


def test_subtraction():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(10 - 4));
        THIS.DIE();
    ''').strip() == "6"


def test_multiplication():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(7 * 6));
        THIS.DIE();
    ''').strip() == "42"


def test_division():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(20 / 4));
        THIS.DIE();
    ''').strip() == "5"


def test_modulo():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(17 % 5));
        THIS.DIE();
    ''').strip() == "2"


def test_negative_numbers():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(-5 + 3));
        THIS.DIE();
    ''').strip() == "-2"


def test_float_arithmetic():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(3.5 + 1.5));
        THIS.DIE();
    ''').strip() == "5.0"


def test_operator_precedence():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(2 + 3 * 4));
        THIS.DIE();
    ''').strip() == "14"


def test_parentheses():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER((2 + 3) * 4));
        THIS.DIE();
    ''').strip() == "20"


def test_complex_arithmetic():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(2 + 3 * 4 - 5 / 5);
        );
        THIS.DIE();
    ''').strip() == "13"


def test_negation_precedence():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(-2 * 3);
        );
        THIS.DIE();
    ''').strip() == "-6"


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------

def test_equal():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 == 5 { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_not_equal():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 != 3 { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_less_than():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 3 < 5 { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_greater_than():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 5 > 3 { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_comparison_in_logical():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 1 < 2 AND 3 > 2 {
                UTTER("yes");
            }
        );
        THIS.DIE();
    ''').strip() == "yes"


# ---------------------------------------------------------------------------
# Bitwise
# ---------------------------------------------------------------------------

def test_bitwise_and():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 & 13));
        THIS.DIE();
    ''').strip() == "12"


def test_bitwise_or():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 | 13));
        THIS.DIE();
    ''').strip() == "61"


def test_bitwise_xor():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 ^ 13));
        THIS.DIE();
    ''').strip() == "49"


def test_bitwise_not():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(~60));
        THIS.DIE();
    ''').strip() == "-61"


def test_left_shift():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 << 2));
        THIS.DIE();
    ''').strip() == "240"


def test_right_shift():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(60 >> 2));
        THIS.DIE();
    ''').strip() == "15"


def test_bitwise_precedence():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER(1 | 2 & 3);
            UTTER(1 & 3 << 1);
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["3", "0"]


# ---------------------------------------------------------------------------
# Logical
# ---------------------------------------------------------------------------

def test_and_true():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE AND ALIVE { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_and_false():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE AND DEAD { UTTER("yes"); } LEST { UTTER("no"); }
        );
        THIS.DIE();
    ''').strip() == "no"


def test_or_true():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD OR ALIVE { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_or_false():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD OR DEAD { UTTER("yes"); } LEST { UTTER("no"); }
        );
        THIS.DIE();
    ''').strip() == "no"


def test_not():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD NOT DEAD { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_short_circuit_and():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH called WITH DEAD;
            RITE setTrue() {
                called = ALIVE;
                BEQUEATH ALIVE;
            }
            BIRTH x WITH DEAD AND setTrue();
            SHOULD called {
                UTTER("called");
            } LEST {
                UTTER("not called");
            }
        );
        THIS.DIE();
    ''').strip() == "not called"


def test_short_circuit_or():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH called WITH DEAD;
            RITE setTrue() {
                called = ALIVE;
                BEQUEATH ALIVE;
            }
            BIRTH x WITH ALIVE OR setTrue();
            SHOULD called {
                UTTER("called");
            } LEST {
                UTTER("not called");
            }
        );
        THIS.DIE();
    ''').strip() == "not called"


# ---------------------------------------------------------------------------
# Strings
# ---------------------------------------------------------------------------

def test_string_concatenation():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Hello, " + "world!"));
        THIS.DIE();
    ''').strip() == "Hello, world!"


def test_string_number_concat():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("Value: " + 42));
        THIS.DIE();
    ''').strip() == "Value: 42"


def test_string_comparison():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD "abc" == "abc" { UTTER("yes"); }
        );
        THIS.DIE();
    ''').strip() == "yes"


def test_string_concat_with_numbers():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER("Value: " + 42 + " and " + 3.14);
        );
        THIS.DIE();
    ''').strip() == "Value: 42 and 3.14"


def test_string_concat_with_bool():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            UTTER("Is alive: " + ALIVE);
        );
        THIS.DIE();
    ''').strip() == "Is alive: ALIVE"


def test_string_index():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH s WITH "hello";
            UTTER(s[0]);
            UTTER(s[4]);
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["h", "o"]


# ---------------------------------------------------------------------------
# Arrays
# ---------------------------------------------------------------------------

def test_array_literal():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3];
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, 2, 3]"


def test_array_index():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [10, 20, 30];
            UTTER(arr[1]);
        );
        THIS.DIE();
    ''').strip() == "20"


def test_array_index_assignment():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3];
            arr[1] = 99;
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, 99, 3]"


def test_empty_array():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [];
            UTTER(LENGTH(arr));
        );
        THIS.DIE();
    ''').strip() == "0"


def test_mixed_array():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, "two", ALIVE];
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, two, ALIVE]"


def test_void_in_array():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, VOID, 3];
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, VOID, 3]"


def test_empty_array_operations():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [];
            UTTER(LENGTH(arr));
            arr = APPEND(arr, 1);
            UTTER(LENGTH(arr));
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["0", "1"]


def test_deeply_nested_array():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
            UTTER(arr[0][1][0]);
        );
        THIS.DIE();
    ''').strip() == "3"


def test_array_of_maps():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [{name: "a"}, {name: "b"}];
            UTTER(arr[1].name);
        );
        THIS.DIE();
    ''').strip() == "b"


def test_array_index_out_of_bounds():
    assert run_ath_via_c('''
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
    ''').strip() == "out of bounds"


# ---------------------------------------------------------------------------
# Maps
# ---------------------------------------------------------------------------

def test_map_literal():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {x: 1, y: 2};
            UTTER(m);
        );
        THIS.DIE();
    ''')
    assert "x: 1" in out
    assert "y: 2" in out


def test_map_member_access():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {name: "Karkat"};
            UTTER(m.name);
        );
        THIS.DIE();
    ''').strip() == "Karkat"


def test_map_index_access():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {name: "Karkat"};
            UTTER(m["name"]);
        );
        THIS.DIE();
    ''').strip() == "Karkat"


def test_map_member_assignment():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {x: 1};
            m.x = 99;
            UTTER(m.x);
        );
        THIS.DIE();
    ''').strip() == "99"


def test_empty_map():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {};
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
    ''').strip() == "0"


def test_void_in_map():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: VOID};
            UTTER(m);
        );
        THIS.DIE();
    ''')
    assert "a: VOID" in out


def test_nested_map_access():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {outer: {inner: {value: 42}}};
            UTTER(m.outer.inner.value);
        );
        THIS.DIE();
    ''').strip() == "42"


def test_map_with_array_values():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {nums: [1, 2, 3]};
            UTTER(m.nums[1]);
        );
        THIS.DIE();
    ''').strip() == "2"


def test_empty_map_operations():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {};
            UTTER(LENGTH(KEYS(m)));
            m = SET(m, "a", 1);
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["0", "1"]


# ---------------------------------------------------------------------------
# Conditionals
# ---------------------------------------------------------------------------

def test_should_true():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD ALIVE {
                UTTER("true branch");
            }
        );
        THIS.DIE();
    ''').strip() == "true branch"


def test_should_false():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD {
                UTTER("true branch");
            }
            UTTER("after");
        );
        THIS.DIE();
    ''').strip() == "after"


def test_should_lest():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD DEAD {
                UTTER("true");
            } LEST {
                UTTER("false");
            }
        );
        THIS.DIE();
    ''').strip() == "false"


def test_chained_should():
    assert run_ath_via_c('''
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
    ''').strip() == "two"


@pytest.mark.parametrize("value,expected", [
    ("1", "truthy"),
    ('"hello"', "truthy"),
    ("[1]", "truthy"),
    ("{x: 1}", "truthy"),
])
def test_truthy_values(value, expected):
    out = run_ath_via_c(f'''
        import timer T(1ms);
        ~ATH(T) {{ }} EXECUTE(
            SHOULD {value} {{ UTTER("truthy"); }} LEST {{ UTTER("falsy"); }}
        );
        THIS.DIE();
    ''')
    assert out.strip() == expected


@pytest.mark.parametrize("value,expected", [
    ("0", "falsy"),
    ('""', "falsy"),
    ("[]", "falsy"),
    ("{}", "falsy"),
    ("VOID", "falsy"),
    ("DEAD", "falsy"),
])
def test_falsy_values(value, expected):
    out = run_ath_via_c(f'''
        import timer T(1ms);
        ~ATH(T) {{ }} EXECUTE(
            SHOULD {value} {{ UTTER("truthy"); }} LEST {{ UTTER("falsy"); }}
        );
        THIS.DIE();
    ''')
    assert out.strip() == expected


def test_truthiness_of_numbers():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            SHOULD 1 { UTTER("1 is truthy"); }
            SHOULD 0 { UTTER("0 is truthy"); } LEST { UTTER("0 is falsy"); }
            SHOULD 0.0 { UTTER("0.0 is truthy"); } LEST { UTTER("0.0 is falsy"); }
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["1 is truthy", "0 is falsy", "0.0 is falsy"]


# ---------------------------------------------------------------------------
# Rites
# ---------------------------------------------------------------------------

def test_simple_rite():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE greet() {
                UTTER("Hello!");
            }
            greet();
        );
        THIS.DIE();
    ''').strip() == "Hello!"


def test_rite_with_params():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE add(a, b) {
                BEQUEATH a + b;
            }
            UTTER(add(3, 4));
        );
        THIS.DIE();
    ''').strip() == "7"


def test_rite_with_bequeath():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE double(x) {
                BEQUEATH x * 2;
            }
            BIRTH result WITH double(21);
            UTTER(result);
        );
        THIS.DIE();
    ''').strip() == "42"


def test_recursive_rite():
    assert run_ath_via_c('''
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
    ''').strip() == "120"


def test_rite_no_return():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE noReturn() {
                BIRTH x WITH 5;
            }
            BIRTH result WITH noReturn();
            UTTER(TYPEOF(result));
        );
        THIS.DIE();
    ''').strip() == "VOID"


def test_rite_closure():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH multiplier WITH 10;
            RITE multiply(x) {
                BEQUEATH x * multiplier;
            }
            UTTER(multiply(5));
        );
        THIS.DIE();
    ''').strip() == "50"


def test_mutual_recursion():
    out = run_ath_via_c('''
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
    ''').strip().split('\n')
    assert out == ["4 is even", "5 is odd"]


def test_fibonacci():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            RITE fib(n) {
                SHOULD n <= 1 { BEQUEATH n; }
                BEQUEATH fib(n - 1) + fib(n - 2);
            }
            UTTER(fib(10));
        );
        THIS.DIE();
    ''').strip() == "55"


def test_local_scope():
    out = run_ath_via_c('''
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
    ''').strip().split('\n')
    assert out == ["10", "5"]


def test_closure_scope():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH x WITH 5;
            RITE inner() {
                BEQUEATH x;
            }
            UTTER(inner());
        );
        THIS.DIE();
    ''').strip() == "5"


def test_shadowing():
    out = run_ath_via_c('''
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
    ''').strip().split('\n')
    assert out == ["2", "1"]


# ---------------------------------------------------------------------------
# Error handling
# ---------------------------------------------------------------------------

def test_condemn_caught():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                CONDEMN "test error";
            } SALVAGE err {
                UTTER("Caught: " + err);
            }
        );
        THIS.DIE();
    ''').strip() == "Caught: test error"


def test_runtime_error_caught():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                BIRTH x WITH PARSE_INT("not a number");
            } SALVAGE err {
                UTTER("Caught error");
            }
        );
        THIS.DIE();
    ''').strip() == "Caught error"


def test_no_error_attempt():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            ATTEMPT {
                UTTER("no error");
            } SALVAGE err {
                UTTER("caught: " + err);
            }
        );
        THIS.DIE();
    ''').strip() == "no error"


def test_nested_attempt():
    assert run_ath_via_c('''
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
    ''').strip() == "Got: outer"


def test_error_in_rite():
    assert run_ath_via_c('''
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
    ''').strip() == "Error: negative value"


def test_error_propagation():
    assert run_ath_via_c('''
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
    ''').strip() == "Caught: inner error"


def test_error_in_execute():
    assert run_ath_via_c('''
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
    ''').strip() == "Outer caught: nested error"


# ---------------------------------------------------------------------------
# Timers and chaining
# ---------------------------------------------------------------------------

def test_timer_chain():
    out = run_ath_via_c('''
        import timer T1(1ms);
        ~ATH(T1) { } EXECUTE(
            UTTER("first");
            import timer T2(1ms);
            ~ATH(T2) { } EXECUTE(
                UTTER("second");
            );
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["first", "second"]


def test_timer_reuse_name():
    out = run_ath_via_c('''
        RITE count(n) {
            SHOULD n > 0 {
                UTTER(n);
                import timer T(1ms);
                ~ATH(T) { } EXECUTE(count(n - 1));
            }
        }
        count(3);
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["3", "2", "1"]


def test_rapid_timer_chain():
    assert run_ath_via_c('''
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
    ''').strip() == "5"


def test_timer_in_conditional():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH flag WITH ALIVE;
            SHOULD flag {
                import timer T2(1ms);
                ~ATH(T2) { } EXECUTE(UTTER("conditional timer"));
            }
        );
        THIS.DIE();
    ''').strip() == "conditional timer"


# ---------------------------------------------------------------------------
# Entity combinations
# ---------------------------------------------------------------------------

def test_entity_or():
    assert run_ath_via_c('''
        import timer T1(10ms);
        import timer T2(1ms);
        ~ATH(T1 || T2) { } EXECUTE(UTTER("done"));
        THIS.DIE();
    ''').strip() == "done"


def test_entity_and():
    assert run_ath_via_c('''
        import timer T1(1ms);
        import timer T2(1ms);
        ~ATH(T1 && T2) { } EXECUTE(UTTER("both done"));
        THIS.DIE();
    ''').strip() == "both done"


def test_entity_not():
    assert run_ath_via_c('''
        import timer T(1s);
        ~ATH(!T) { } EXECUTE(UTTER("timer exists"));
        T.DIE();
        THIS.DIE();
    ''').strip() == "timer exists"


# ---------------------------------------------------------------------------
# Bifurcation
# ---------------------------------------------------------------------------

def test_simple_bifurcate():
    out = set(run_ath_via_c('''
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
    ''').strip().split('\n'))
    assert out == {"left", "right"}


def test_bifurcate_shared_variable():
    assert run_ath_via_c('''
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

        import timer wait(10ms);
        ~ATH(wait) { } EXECUTE(UTTER(counter));

        [LEFT, RIGHT].DIE();
    ''').strip() == "11"


# ---------------------------------------------------------------------------
# Builtins
# ---------------------------------------------------------------------------

def test_utter_multiple_args():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(1, 2, 3));
        THIS.DIE();
    ''').strip() == "1 2 3"


@pytest.mark.parametrize("value,expected", [
    ("42", "INTEGER"),
    ("3.14", "FLOAT"),
    ('"hello"', "STRING"),
    ("ALIVE", "BOOLEAN"),
    ("VOID", "VOID"),
    ("[1, 2]", "ARRAY"),
    ("{x: 1}", "MAP"),
])
def test_typeof(value, expected):
    out = run_ath_via_c(f'''
        import timer T(1ms);
        ~ATH(T) {{ }} EXECUTE(UTTER(TYPEOF({value})));
        THIS.DIE();
    ''')
    assert out.strip() == expected


def test_length_string():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LENGTH("hello")));
        THIS.DIE();
    ''').strip() == "5"


def test_length_array():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LENGTH([1, 2, 3])));
        THIS.DIE();
    ''').strip() == "3"


def test_parse_int():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(PARSE_INT("42")));
        THIS.DIE();
    ''').strip() == "42"


def test_parse_float():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(PARSE_FLOAT("3.14")));
        THIS.DIE();
    ''').strip() == "3.14"


def test_string_conversion():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(STRING(42)));
        THIS.DIE();
    ''').strip() == "42"


def test_int_conversion():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(INT(3.7)));
        THIS.DIE();
    ''').strip() == "3"


def test_float_conversion():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(FLOAT(42)));
        THIS.DIE();
    ''').strip() == "42.0"


def test_append():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2];
            arr = APPEND(arr, 3);
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, 2, 3]"


def test_prepend():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [2, 3];
            arr = PREPEND(arr, 1);
            UTTER(arr);
        );
        THIS.DIE();
    ''').strip() == "[1, 2, 3]"


def test_slice():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH arr WITH [1, 2, 3, 4, 5];
            UTTER(SLICE(arr, 1, 4));
        );
        THIS.DIE();
    ''').strip() == "[2, 3, 4]"


def test_first():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(FIRST([10, 20, 30])));
        THIS.DIE();
    ''').strip() == "10"


def test_last():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LAST([10, 20, 30])));
        THIS.DIE();
    ''').strip() == "30"


def test_concat():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(CONCAT([1, 2], [3, 4])));
        THIS.DIE();
    ''').strip() == "[1, 2, 3, 4]"


def test_keys():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            UTTER(KEYS(m));
        );
        THIS.DIE();
    ''')
    assert "a" in out
    assert "b" in out


def test_values():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            UTTER(VALUES(m));
        );
        THIS.DIE();
    ''')
    assert "1" in out
    assert "2" in out


def test_has():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1};
            SHOULD HAS(m, "a") { UTTER("yes"); }
            SHOULD NOT HAS(m, "b") { UTTER("no b"); }
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["yes", "no b"]


def test_set():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1};
            m = SET(m, "b", 2);
            UTTER(m.b);
        );
        THIS.DIE();
    ''').strip() == "2"


def test_delete():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH m WITH {a: 1, b: 2};
            m = DELETE(m, "a");
            UTTER(LENGTH(KEYS(m)));
        );
        THIS.DIE();
    ''').strip() == "1"


def test_split():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(SPLIT("a,b,c", ",")));
        THIS.DIE();
    ''').strip() == "[a, b, c]"


def test_join():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(JOIN(["a", "b", "c"], "-")));
        THIS.DIE();
    ''').strip() == "a-b-c"


def test_substring():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(SUBSTRING("hello", 1, 4)));
        THIS.DIE();
    ''').strip() == "ell"


def test_uppercase():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(UPPERCASE("hello")));
        THIS.DIE();
    ''').strip() == "HELLO"


def test_lowercase():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(LOWERCASE("HELLO")));
        THIS.DIE();
    ''').strip() == "hello"


def test_trim():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(TRIM("  hello  ")));
        THIS.DIE();
    ''').strip() == "hello"


def test_replace():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(REPLACE("hello", "l", "w")));
        THIS.DIE();
    ''').strip() == "hewwo"


def test_random_range():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH r WITH RANDOM();
            SHOULD r >= 0 AND r < 1 {
                UTTER("valid");
            }
        );
        THIS.DIE();
    ''').strip() == "valid"


def test_random_int():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH r WITH RANDOM_INT(1, 6);
            SHOULD r >= 1 AND r <= 6 {
                UTTER("valid");
            }
        );
        THIS.DIE();
    ''').strip() == "valid"


def test_time_positive():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH t WITH TIME();
            SHOULD t > 0 {
                UTTER("valid");
            }
        );
        THIS.DIE();
    ''').strip() == "valid"


def test_char():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(CHAR(65)));
        THIS.DIE();
    ''').strip() == "A"


def test_code():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(CODE("A")));
        THIS.DIE();
    ''').strip() == "65"


def test_bin():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(BIN(10)));
        THIS.DIE();
    ''').strip() == "1010"


def test_hex():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(HEX(255)));
        THIS.DIE();
    ''').strip() == "FF"


def test_empty_string_operations():
    out = run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(
            BIRTH s WITH "";
            UTTER(LENGTH(s));
            s = s + "hello";
            UTTER(LENGTH(s));
        );
        THIS.DIE();
    ''').strip().split('\n')
    assert out == ["0", "5"]


# ---------------------------------------------------------------------------
# Entity management / misc
# ---------------------------------------------------------------------------

def test_die_already_dead():
    assert run_ath_via_c('''
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER("timer died"));
        T.DIE();
        THIS.DIE();
    ''').strip() == "timer died"


def test_kill_this_early():
    assert run_ath_via_c('''
        THIS.DIE();
        ~ATH(THIS) { } EXECUTE(UTTER("THIS died"));
    ''').strip() == "THIS died"


def test_comment_after_code():
    assert run_ath_via_c('''
        import timer T(1ms); // This is a comment
        ~ATH(T) { } EXECUTE(UTTER("hello")); // Another comment
        THIS.DIE(); // Final comment
    ''').strip() == "hello"


def test_comment_with_keywords():
    assert run_ath_via_c('''
        // import timer FAKE(1ms);
        import timer T(1ms);
        // ~ATH(FAKE) { } EXECUTE(UTTER("fake"));
        ~ATH(T) { } EXECUTE(UTTER("real"));
        THIS.DIE();
    ''').strip() == "real"


def test_fizzbuzz():
    """FizzBuzz via async rite chaining."""
    out = run_ath_via_c('''
        RITE fizzbuzz(n, max) {
            SHOULD n <= max {
                SHOULD n % 15 == 0 {
                    UTTER("FizzBuzz");
                } LEST SHOULD n % 3 == 0 {
                    UTTER("Fizz");
                } LEST SHOULD n % 5 == 0 {
                    UTTER("Buzz");
                } LEST {
                    UTTER(n);
                }
                import timer T(1ms);
                ~ATH(T) { } EXECUTE(fizzbuzz(n + 1, max));
            }
        }
        fizzbuzz(1, 15);
        THIS.DIE();
    ''').strip().split('\n')
    expected = [
        "1","2","Fizz","4","Buzz","Fizz","7","8","Fizz","Buzz",
        "11","Fizz","13","14","FizzBuzz",
    ]
    assert out == expected
