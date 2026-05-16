"""Spec-driven black-box tests for the !~ATH-to-C89 transpiler.

Each test pins down a specific behaviour from athSpec.md (v1.3).
Tests transpile a small !~ATH program, gcc-compile, run, and check stdout.
"""

import sys
import os
import re

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


# =========================================================================
# Lexical structure
# =========================================================================


class TestLexical:
    """Spec section: Lexical Structure."""

    def test_single_line_comment_ignored(self):
        """Spec: "Single-line comments only: // This is a comment"."""
        out = run_ath_via_c(
            '// just a comment\n'
            'UTTER("ok"); // trailing\n'
            'THIS.DIE();'
        )
        assert out.strip() == "ok"

    def test_identifier_with_underscore_and_digits(self):
        """Spec: identifiers begin with letter/_ and continue with letters/digits/_ ."""
        out = run_ath_via_c(
            'BIRTH _private2 WITH 7; UTTER(_private2); THIS.DIE();'
        )
        assert out.strip() == "7"

    def test_case_sensitive_identifiers(self):
        """Spec: "Identifiers are case-sensitive"."""
        out = run_ath_via_c(
            'BIRTH x WITH 1; BIRTH X WITH 2; UTTER(x); UTTER(X); THIS.DIE();'
        )
        assert out.strip().split() == ["1", "2"]

    def test_integer_negative_literal(self):
        """Spec: "Integers: Decimal digits, optionally prefixed with -"."""
        out = run_ath_via_c('UTTER(-7); THIS.DIE();')
        assert out.strip() == "-7"

    def test_float_literal(self):
        """Spec: "Floats: Decimal digits with a decimal point"."""
        out = run_ath_via_c('UTTER(3.14); THIS.DIE();')
        # Allow trailing-zero variation; just make sure it starts with 3.14
        assert out.strip().startswith("3.14")

    def test_string_escape_sequences(self):
        """Spec: escape sequences \\\\, \\", \\n, \\t."""
        out = run_ath_via_c(
            'UTTER("a\\tb\\\\c\\"d"); THIS.DIE();'
        )
        assert out == 'a\tb\\c"d\n'

    def test_string_newline_escape(self):
        """Spec: \\n escape produces a newline."""
        out = run_ath_via_c('UTTER("x\\ny"); THIS.DIE();')
        assert out == "x\ny\n"

    def test_alive_dead_void_literals(self):
        """Spec: "ALIVE   // truthy", "DEAD    // falsy", "VOID    // absence"."""
        out = run_ath_via_c(
            'UTTER(ALIVE); UTTER(DEAD); UTTER(VOID); THIS.DIE();'
        )
        # Don't assume exact rendering; just split into 3 tokens.
        lines = out.strip().splitlines()
        assert len(lines) == 3

    def test_array_literal_mixed_types(self):
        """Spec: "Arrays: Square brackets, comma-separated", "[1, "mixed", ALIVE]"."""
        out = run_ath_via_c(
            'BIRTH a WITH [1, "two", ALIVE]; UTTER(LENGTH(a)); THIS.DIE();'
        )
        assert out.strip() == "3"

    def test_empty_array_and_map(self):
        """Spec: "[]" and "{}" are valid literals."""
        out = run_ath_via_c(
            'UTTER(LENGTH([])); UTTER(LENGTH(KEYS({}))); THIS.DIE();'
        )
        assert out.strip().split() == ["0", "0"]

    def test_map_keys_identifier_and_string_equivalent(self):
        """Spec: "Map keys are identifiers (unquoted) or strings (quoted). Both refer to string keys."."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 1, "b": 2}; '
            'UTTER(m["a"]); UTTER(m.b); '
            'THIS.DIE();'
        )
        assert out.strip().split() == ["1", "2"]

    def test_duration_units_all_convert(self):
        """Spec: 1ms, 1s, 1m, 1h durations all parse (we only test they don't deadlock fast)."""
        # 1ms timer wait: just confirms ms duration accepted and fires
        out = run_ath_via_c(
            'import timer T(1ms); '
            '~ATH(T) {} EXECUTE(UTTER("done")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "done"

    def test_duration_no_unit_is_milliseconds(self):
        """Spec: "100      // no unit = milliseconds (default)"."""
        out = run_ath_via_c(
            'import timer T(1); '
            '~ATH(T) {} EXECUTE(UTTER("ok")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "ok"


# =========================================================================
# Entity system
# =========================================================================


class TestEntities:

    def test_this_die_terminates(self):
        """Spec: "The program terminates when THIS.DIE() is called"."""
        out = run_ath_via_c('UTTER("before"); THIS.DIE();')
        assert out.strip() == "before"

    def test_die_idempotent(self):
        """Spec: "Calling .DIE() on an already-dead entity has no effect"."""
        out = run_ath_via_c(
            'import timer T(1ms); '
            'T.DIE(); T.DIE(); '
            'UTTER("ok"); THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "ok"

    def test_timer_dies_after_duration(self):
        """Spec: "When the duration elapses, the timer dies"."""
        out = run_ath_via_c(
            'import timer T(1ms); '
            '~ATH(T) {} EXECUTE(UTTER("fired")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "fired"

    def test_entity_and_combination(self):
        """Spec: "AND (&&): Dies when both entities have died"."""
        out = run_ath_via_c(
            'import timer T1(1ms); import timer T2(2ms); '
            '~ATH(T1 && T2) {} EXECUTE(UTTER("both")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "both"

    def test_entity_or_combination(self):
        """Spec: "OR (||): Dies when either entity has died"."""
        out = run_ath_via_c(
            'import timer T1(1ms); import timer T2(5s); '
            '~ATH(T1 || T2) {} EXECUTE(UTTER("either")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "either"

    def test_entity_not_fires_on_creation(self):
        """Spec: "NOT (!): Dies when the entity begins to exist (is imported)"."""
        out = run_ath_via_c(
            'import timer T(5s); '
            '~ATH(!T) {} EXECUTE(UTTER("created")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "created"

    def test_entity_nested_combination(self):
        """Spec: "Combinations can be nested: ~ATH((T1 && T2) || T3)"."""
        out = run_ath_via_c(
            'import timer T1(1ms); import timer T2(1ms); import timer T3(5s); '
            '~ATH((T1 && T2) || T3) {} EXECUTE(UTTER("done")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "done"


# =========================================================================
# ~ATH loop construct
# =========================================================================


class TestAthLoop:

    def test_wait_mode_executes_after_death(self):
        """Spec: wait-mode "When the entity dies, execute the EXECUTE clause"."""
        out = run_ath_via_c(
            'import timer T(1ms); '
            'UTTER("before"); '
            '~ATH(T) {} EXECUTE(UTTER("after")); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip().splitlines() == ["before", "after"]

    def test_execute_void_is_noop(self):
        """Spec: "Empty EXECUTE: Use VOID as the canonical no-op"."""
        out = run_ath_via_c(
            'import timer T(1ms); '
            '~ATH(T) {} EXECUTE(VOID); '
            'UTTER("ok"); THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "ok"

    def test_execute_multiple_statements(self):
        """Spec: "Multiple statements (semicolon-separated, final semicolon optional)"."""
        out = run_ath_via_c(
            'import timer T(1ms); '
            '~ATH(T) {} EXECUTE('
            '  BIRTH x WITH 5; '
            '  BIRTH y WITH x + 10; '
            '  UTTER(y)'
            '); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "15"

    def test_nested_ath_chaining(self):
        """Spec: "Nested ~ATH (for chaining)" -- imports and inner ~ATH inside EXECUTE.

        Note: per the EBNF, only the final bare *expression* may omit ';'; nested
        ath_loops are expr_statements that require ';'. Including the trailing ';'
        below to match the grammar; the spec prose ("final semicolon optional")
        does not draw this distinction explicitly.
        """
        out = run_ath_via_c(
            'import timer T1(1ms); '
            '~ATH(T1) {} EXECUTE('
            '  UTTER("outer"); '
            '  import timer T2(1ms); '
            '  ~ATH(T2) {} EXECUTE(UTTER("inner")); '
            '); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip().splitlines() == ["outer", "inner"]


# =========================================================================
# Bifurcation
# =========================================================================


class TestBifurcation:

    def test_simple_bifurcation_both_run(self):
        """Spec: "Both branches begin executing concurrently"."""
        out = run_ath_via_c(
            'bifurcate THIS[LEFT, RIGHT]; '
            '~ATH(LEFT) { } EXECUTE(UTTER("L")); '
            '~ATH(RIGHT) { } EXECUTE(UTTER("R")); '
            '[LEFT, RIGHT].DIE();',
            timeout=5.0,
        )
        assert set(out.strip().splitlines()) == {"L", "R"}

    def test_bifurcation_shared_variable(self):
        """Spec: "Variables are shared (lexical scoping applies across branches)"."""
        out = run_ath_via_c(
            'BIRTH x WITH 0; '
            'bifurcate THIS[A, B]; '
            '~ATH(A) { } EXECUTE(x = x + 1); '
            '~ATH(B) { } EXECUTE(x = x + 10); '
            '[A, B].DIE(); '
            'import timer T(5ms); ~ATH(T) {} EXECUTE(UTTER(x)); '
            'THIS.DIE();',
            timeout=5.0,
        )
        assert out.strip() == "11"

    def test_branch_dies_only_when_subtree_done(self):
        """Spec: "branch doesn't die until its entire subtree of execution is done"."""
        out = run_ath_via_c(
            'bifurcate THIS[A, B]; '
            '~ATH(A) { '
            '  import timer T(5ms); '
            '  ~ATH(T) {} EXECUTE(UTTER("A-inner")); '
            '} EXECUTE(UTTER("A-end")); '
            '~ATH(B) { } EXECUTE(UTTER("B-end")); '
            '[A, B].DIE();',
            timeout=5.0,
        )
        lines = out.strip().splitlines()
        assert "A-inner" in lines and "A-end" in lines and "B-end" in lines
        # A-inner must come before A-end (subtree completes before branch dies)
        assert lines.index("A-inner") < lines.index("A-end")

    def test_nested_bifurcation_recombination(self):
        """Spec: "[A, [B1, B2]].DIE();"."""
        out = run_ath_via_c(
            'bifurcate THIS[A, B]; '
            'bifurcate B[B1, B2]; '
            '~ATH(A)  { } EXECUTE(UTTER("A")); '
            '~ATH(B1) { } EXECUTE(UTTER("B1")); '
            '~ATH(B2) { } EXECUTE(UTTER("B2")); '
            '[A, [B1, B2]].DIE();',
            timeout=5.0,
        )
        assert set(out.strip().splitlines()) == {"A", "B1", "B2"}


# =========================================================================
# Expression language: literals, operators, precedence
# =========================================================================


class TestExpressionLanguage:

    def test_integer_arithmetic(self):
        """Spec: "+   Addition", "-   Subtraction", etc."""
        out = run_ath_via_c('UTTER(2 + 3 * 4); THIS.DIE();')
        assert out.strip() == "14"

    def test_parentheses_override_precedence(self):
        """Spec: "Parentheses override precedence"."""
        out = run_ath_via_c('UTTER((2 + 3) * 4); THIS.DIE();')
        assert out.strip() == "20"

    def test_integer_division_truncates(self):
        """Spec: "Division (integer division for INTEGER operands)"."""
        out = run_ath_via_c('UTTER(7 / 2); THIS.DIE();')
        assert out.strip() == "3"

    def test_modulo(self):
        """Spec: "%   Modulo"."""
        out = run_ath_via_c('UTTER(10 % 3); THIS.DIE();')
        assert out.strip() == "1"

    def test_bitwise_operators(self):
        """Spec: bitwise &, |, ^, ~, <<, >>."""
        out = run_ath_via_c(
            'UTTER(6 & 3); UTTER(6 | 1); UTTER(6 ^ 3); '
            'UTTER(1 << 3); UTTER(16 >> 2); '
            'THIS.DIE();'
        )
        assert out.strip().split() == ["2", "7", "5", "8", "4"]

    def test_comparison_returns_boolean(self):
        """Spec: comparison operators "(returns BOOLEAN)"."""
        out = run_ath_via_c(
            'UTTER(TYPEOF(1 == 1)); UTTER(TYPEOF(1 < 2)); THIS.DIE();'
        )
        for line in out.strip().splitlines():
            assert line == "BOOLEAN"

    def test_logical_and_short_circuit(self):
        """Spec: "AND   Logical and (short-circuit)"."""
        # If short-circuit works, second operand (1/0) is never evaluated.
        out = run_ath_via_c(
            'ATTEMPT { '
            '  BIRTH r WITH DEAD AND (1 / 0); UTTER("short"); '
            '} SALVAGE e { UTTER("crashed"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "short"

    def test_logical_or_short_circuit(self):
        """Spec: "OR    Logical or (short-circuit)"."""
        out = run_ath_via_c(
            'ATTEMPT { '
            '  BIRTH r WITH ALIVE OR (1 / 0); UTTER("short"); '
            '} SALVAGE e { UTTER("crashed"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "short"

    def test_string_concat_coerces_via_string(self):
        """Spec: "String concatenation with +: non-strings are converted via STRING()"."""
        out = run_ath_via_c('UTTER("n=" + 42); THIS.DIE();')
        assert out.strip() == "n=42"

    def test_precedence_unary_negation(self):
        """Spec: precedence puts unary - tighter than *."""
        out = run_ath_via_c('UTTER(-2 * 3); THIS.DIE();')
        assert out.strip() == "-6"

    def test_precedence_and_over_or(self):
        """Spec: AND has higher precedence than OR (11 vs 12)."""
        # DEAD OR ALIVE AND DEAD  ->  DEAD OR (ALIVE AND DEAD)  ->  DEAD OR DEAD  -> DEAD
        out = run_ath_via_c(
            'SHOULD DEAD OR ALIVE AND DEAD { UTTER("t"); } LEST { UTTER("f"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "f"

    def test_precedence_comparison_over_and(self):
        """Spec: comparison has higher precedence than AND."""
        out = run_ath_via_c(
            'SHOULD 1 < 2 AND 3 < 4 { UTTER("yes"); } LEST { UTTER("no"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "yes"

    def test_not_negation(self):
        """Spec: "NOT   Logical negation"."""
        out = run_ath_via_c(
            'SHOULD NOT DEAD { UTTER("a"); } '
            'SHOULD NOT ALIVE { UTTER("b"); } LEST { UTTER("c"); } '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == ["a", "c"]

    def test_truthiness_of_zero_empty(self):
        """Spec: "DEAD, VOID, 0, "", [], {} are falsy; all else truthy"."""
        out = run_ath_via_c(
            'SHOULD 0 { UTTER("a"); } LEST { UTTER("0f"); } '
            'SHOULD "" { UTTER("b"); } LEST { UTTER("strf"); } '
            'SHOULD [] { UTTER("c"); } LEST { UTTER("arrf"); } '
            'SHOULD VOID { UTTER("d"); } LEST { UTTER("voidf"); } '
            'SHOULD 1 { UTTER("nonzero"); } '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == ["0f", "strf", "arrf", "voidf", "nonzero"]

    def test_entomb_immutability(self):
        """Spec: "Attempting to reassign an entombed variable is a runtime error"."""
        out = run_ath_via_c(
            'ENTOMB PI WITH 3; '
            'ATTEMPT { PI = 4; UTTER("nope"); } '
            'SALVAGE e { UTTER("caught"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "caught"

    def test_array_indexing(self):
        """Spec: "arr[0]   // array index (0-based)"."""
        out = run_ath_via_c(
            'BIRTH a WITH [10, 20, 30]; UTTER(a[0]); UTTER(a[2]); THIS.DIE();'
        )
        assert out.strip().split() == ["10", "30"]

    def test_map_dot_access_equiv_bracket(self):
        """Spec: "map.key    // (equivalent to map["key"])"."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 11}; UTTER(m.a); UTTER(m["a"]); THIS.DIE();'
        )
        assert out.strip().splitlines() == ["11", "11"]


# =========================================================================
# Control flow & conditionals
# =========================================================================


class TestControlFlow:

    def test_should_lest_chain(self):
        """Spec: "Chained conditional: SHOULD ... LEST SHOULD ... LEST ..."."""
        out = run_ath_via_c(
            'BIRTH x WITH 2; '
            'SHOULD x == 1 { UTTER("one"); } '
            'LEST SHOULD x == 2 { UTTER("two"); } '
            'LEST { UTTER("other"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "two"


# =========================================================================
# Functions / Rites
# =========================================================================


class TestRites:

    def test_bequeath_returns_value(self):
        """Spec: "BEQUEATH value;  returns a value and exits the rite"."""
        out = run_ath_via_c(
            'RITE add(a, b) { BEQUEATH a + b; } '
            'UTTER(add(2, 3)); THIS.DIE();'
        )
        assert out.strip() == "5"

    def test_bequeath_no_value_is_void(self):
        """Spec: "BEQUEATH with no value returns VOID"."""
        out = run_ath_via_c(
            'RITE nothing() { BEQUEATH; } '
            'UTTER(TYPEOF(nothing())); THIS.DIE();'
        )
        assert out.strip() == "VOID"

    def test_no_bequeath_returns_void(self):
        """Spec: "If no BEQUEATH is reached, the rite returns VOID"."""
        out = run_ath_via_c(
            'RITE bare() { BIRTH x WITH 1; } '
            'UTTER(TYPEOF(bare())); THIS.DIE();'
        )
        assert out.strip() == "VOID"

    def test_recursive_factorial(self):
        """Spec: "Recursion is allowed"; example uses factorial."""
        out = run_ath_via_c(
            'RITE fact(n) { SHOULD n <= 1 { BEQUEATH 1; } BEQUEATH n * fact(n - 1); } '
            'UTTER(fact(5)); THIS.DIE();'
        )
        assert out.strip() == "120"

    def test_bequeath_early_exit(self):
        """Spec: BEQUEATH "exits the rite" immediately."""
        out = run_ath_via_c(
            'RITE early() { BEQUEATH 1; UTTER("unreached"); } '
            'UTTER(early()); THIS.DIE();'
        )
        assert out.strip() == "1"


# =========================================================================
# Error handling
# =========================================================================


class TestErrorHandling:

    def test_attempt_salvage_binds_error(self):
        """Spec: "The error variable in SALVAGE is a STRING describing the error"."""
        out = run_ath_via_c(
            'ATTEMPT { CONDEMN "boom"; } '
            'SALVAGE e { UTTER(TYPEOF(e)); UTTER(e); } '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "STRING"
        assert "boom" in lines[1]

    def test_condemn_propagates_through_rite_call(self):
        """Spec: "CONDEMN immediately exits to the nearest SALVAGE block"."""
        out = run_ath_via_c(
            'RITE bad() { CONDEMN "from-rite"; } '
            'ATTEMPT { bad(); UTTER("nope"); } '
            'SALVAGE e { UTTER("caught"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "caught"

    def test_condemn_takes_any_expression(self):
        """Spec: "CONDEMN expression;" -- any expression is allowed."""
        out = run_ath_via_c(
            'ATTEMPT { CONDEMN "x=" + 42; } '
            'SALVAGE e { UTTER(e); } '
            'THIS.DIE();'
        )
        assert "x=42" in out


# =========================================================================
# Built-in rites
# =========================================================================


class TestTypeOps:

    def test_typeof_every_type(self):
        """Spec: TYPEOF returns "INTEGER", "FLOAT", "STRING", "BOOLEAN", "VOID", "ARRAY", "MAP"."""
        out = run_ath_via_c(
            'UTTER(TYPEOF(42)); '
            'UTTER(TYPEOF(3.14)); '
            'UTTER(TYPEOF("x")); '
            'UTTER(TYPEOF(ALIVE)); '
            'UTTER(TYPEOF(VOID)); '
            'UTTER(TYPEOF([1])); '
            'UTTER(TYPEOF({a: 1})); '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == [
            "INTEGER", "FLOAT", "STRING", "BOOLEAN", "VOID", "ARRAY", "MAP",
        ]

    def test_length_string_and_array(self):
        """Spec: "LENGTH("hello") -> 5, LENGTH([1,2,3]) -> 3"."""
        out = run_ath_via_c('UTTER(LENGTH("hello")); UTTER(LENGTH([1,2,3])); THIS.DIE();')
        assert out.strip().split() == ["5", "3"]

    def test_parse_int_valid(self):
        """Spec: PARSE_INT("42") -> 42."""
        out = run_ath_via_c('UTTER(PARSE_INT("42")); THIS.DIE();')
        assert out.strip() == "42"

    def test_parse_int_bad_input_errors(self):
        """Spec: "PARSE_INT("abc") -> error"."""
        out = run_ath_via_c(
            'ATTEMPT { UTTER(PARSE_INT("abc")); } '
            'SALVAGE e { UTTER("err"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "err"

    def test_parse_float(self):
        """Spec: PARSE_FLOAT("42") -> 42.0; bad input errors."""
        out = run_ath_via_c(
            'UTTER(TYPEOF(PARSE_FLOAT("42"))); '
            'ATTEMPT { UTTER(PARSE_FLOAT("abc")); } '
            'SALVAGE e { UTTER("err"); } '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "FLOAT"
        assert lines[1] == "err"

    def test_string_of_array(self):
        """Spec: "STRING([1,2,3])      // "[1, 2, 3]" "."""
        out = run_ath_via_c('UTTER(STRING([1, 2, 3])); THIS.DIE();')
        assert out.strip() == "[1, 2, 3]"

    def test_int_truncates(self):
        """Spec: "INT(3.7) -> 3, INT(-2.9) -> -2"."""
        out = run_ath_via_c('UTTER(INT(3.7)); UTTER(INT(-2.9)); THIS.DIE();')
        assert out.strip().split() == ["3", "-2"]

    def test_float_conversion(self):
        """Spec: "FLOAT(42) -> 42.0"."""
        out = run_ath_via_c('UTTER(TYPEOF(FLOAT(42))); THIS.DIE();')
        assert out.strip() == "FLOAT"

    def test_char_and_code(self):
        """Spec: "CHAR(65) -> "A"", "CODE("A") -> 65"."""
        out = run_ath_via_c('UTTER(CHAR(65)); UTTER(CODE("A")); THIS.DIE();')
        assert out.strip().splitlines() == ["A", "65"]

    def test_bin_and_hex(self):
        """Spec: "BIN(10) -> "1010"", "HEX(255) -> "FF""."""
        out = run_ath_via_c('UTTER(BIN(10)); UTTER(HEX(255)); THIS.DIE();')
        assert out.strip().splitlines() == ["1010", "FF"]


class TestArrayOps:

    def test_append_returns_new(self):
        """Spec: "APPEND(array, value) -- Returns a new array (does not mutate)"."""
        out = run_ath_via_c(
            'BIRTH a WITH [1, 2]; '
            'BIRTH b WITH APPEND(a, 3); '
            'UTTER(LENGTH(a)); UTTER(LENGTH(b)); UTTER(b[2]); '
            'THIS.DIE();'
        )
        assert out.strip().split() == ["2", "3", "3"]

    def test_prepend(self):
        """Spec: "PREPEND([2,3], 1) -> [1,2,3]"."""
        out = run_ath_via_c(
            'BIRTH b WITH PREPEND([2, 3], 1); UTTER(b[0]); UTTER(LENGTH(b)); THIS.DIE();'
        )
        assert out.strip().split() == ["1", "3"]

    def test_slice_end_exclusive(self):
        """Spec: "SLICE([1,2,3,4,5],1,4) -> [2,3,4]. End is exclusive."""
        out = run_ath_via_c(
            'BIRTH s WITH SLICE([1,2,3,4,5], 1, 4); '
            'UTTER(LENGTH(s)); UTTER(s[0]); UTTER(s[2]); THIS.DIE();'
        )
        assert out.strip().split() == ["3", "2", "4"]

    def test_first_last(self):
        """Spec: "FIRST([1,2,3]) -> 1", "LAST([1,2,3]) -> 3"."""
        out = run_ath_via_c('UTTER(FIRST([1,2,3])); UTTER(LAST([1,2,3])); THIS.DIE();')
        assert out.strip().split() == ["1", "3"]

    def test_first_empty_errors(self):
        """Spec: "FIRST([]) -> error"."""
        out = run_ath_via_c(
            'ATTEMPT { UTTER(FIRST([])); } SALVAGE e { UTTER("err"); } '
            'THIS.DIE();'
        )
        assert out.strip() == "err"

    def test_concat(self):
        """Spec: "CONCAT([1,2],[3,4]) -> [1,2,3,4]"."""
        out = run_ath_via_c(
            'BIRTH c WITH CONCAT([1,2], [3,4]); '
            'UTTER(LENGTH(c)); UTTER(c[0]); UTTER(c[3]); THIS.DIE();'
        )
        assert out.strip().split() == ["4", "1", "4"]


class TestMapOps:

    def test_keys_values(self):
        """Spec: "KEYS({a:1,b:2}) -> ["a","b"]", "VALUES -> [1,2]"."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 1, b: 2}; '
            'UTTER(LENGTH(KEYS(m))); UTTER(LENGTH(VALUES(m))); THIS.DIE();'
        )
        assert out.strip().split() == ["2", "2"]

    def test_has_true_false(self):
        """Spec: "HAS({a:1},"a") -> ALIVE", "HAS({a:1},"b") -> DEAD"."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 1}; '
            'SHOULD HAS(m, "a") { UTTER("yes"); } '
            'SHOULD HAS(m, "b") { UTTER("yep"); } LEST { UTTER("no"); } '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == ["yes", "no"]

    def test_set_returns_new_map(self):
        """Spec: "SET ... Returns a new map (does not mutate)"."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 1}; '
            'BIRTH m2 WITH SET(m, "b", 2); '
            'SHOULD HAS(m, "b") { UTTER("mutated"); } LEST { UTTER("clean"); } '
            'UTTER(m2.b); '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "clean"
        assert lines[1] == "2"

    def test_delete_returns_new_map(self):
        """Spec: "DELETE(m, "a") -> {b:2}"."""
        out = run_ath_via_c(
            'BIRTH m WITH {a: 1, b: 2}; '
            'BIRTH m2 WITH DELETE(m, "a"); '
            'SHOULD HAS(m2, "a") { UTTER("kept"); } LEST { UTTER("gone"); } '
            'UTTER(m2.b); '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == ["gone", "2"]


class TestStringOps:

    def test_split(self):
        """Spec: "SPLIT("a,b,c",",") -> ["a","b","c"]"."""
        out = run_ath_via_c(
            'BIRTH p WITH SPLIT("a,b,c", ","); '
            'UTTER(LENGTH(p)); UTTER(p[1]); '
            'THIS.DIE();'
        )
        assert out.strip().split() == ["3", "b"]

    def test_split_empty_delimiter(self):
        """Spec: "SPLIT("hello","") -> ["h","e","l","l","o"]"."""
        out = run_ath_via_c(
            'BIRTH p WITH SPLIT("hello", ""); '
            'UTTER(LENGTH(p)); UTTER(p[0]); UTTER(p[4]); '
            'THIS.DIE();'
        )
        assert out.strip().split() == ["5", "h", "o"]

    def test_join(self):
        """Spec: "JOIN(["a","b","c"],",") -> "a,b,c""."""
        out = run_ath_via_c('UTTER(JOIN(["a","b","c"], ",")); THIS.DIE();')
        assert out.strip() == "a,b,c"

    def test_substring(self):
        """Spec: "SUBSTRING("hello",1,4) -> "ell""."""
        out = run_ath_via_c('UTTER(SUBSTRING("hello", 1, 4)); THIS.DIE();')
        assert out.strip() == "ell"

    def test_uppercase_lowercase(self):
        """Spec: "UPPERCASE("hello") -> "HELLO"", "LOWERCASE("HELLO") -> "hello""."""
        out = run_ath_via_c('UTTER(UPPERCASE("hi")); UTTER(LOWERCASE("HI")); THIS.DIE();')
        assert out.strip().splitlines() == ["HI", "hi"]

    def test_trim(self):
        """Spec: "TRIM("  hello  ") -> "hello""."""
        out = run_ath_via_c('UTTER("[" + TRIM("  hi  ") + "]"); THIS.DIE();')
        assert out.strip() == "[hi]"

    def test_replace(self):
        """Spec: "REPLACE("hello","l","w") -> "hewwo""."""
        out = run_ath_via_c('UTTER(REPLACE("hello", "l", "w")); THIS.DIE();')
        assert out.strip() == "hewwo"


class TestUtilityRites:

    def test_random_returns_float_in_range(self):
        """Spec: "RANDOM() -- Random float between 0 (inclusive) and 1 (exclusive)"."""
        out = run_ath_via_c(
            'BIRTH r WITH RANDOM(); '
            'UTTER(TYPEOF(r)); '
            'SHOULD r >= 0.0 AND r < 1.0 { UTTER("ok"); } LEST { UTTER("bad"); } '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "FLOAT"
        assert lines[1] == "ok"

    def test_random_int_in_range_inclusive(self):
        """Spec: "RANDOM_INT(min,max) -- Random integer in range (inclusive)"."""
        out = run_ath_via_c(
            'BIRTH r WITH RANDOM_INT(1, 6); '
            'UTTER(TYPEOF(r)); '
            'SHOULD r >= 1 AND r <= 6 { UTTER("ok"); } LEST { UTTER("bad"); } '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "INTEGER"
        assert lines[1] == "ok"

    def test_time_returns_integer(self):
        """Spec: "TIME() -- Current Unix timestamp in milliseconds"."""
        out = run_ath_via_c(
            'BIRTH t WITH TIME(); '
            'UTTER(TYPEOF(t)); '
            'SHOULD t > 0 { UTTER("ok"); } '
            'THIS.DIE();'
        )
        lines = out.strip().splitlines()
        assert lines[0] == "INTEGER"
        assert lines[1] == "ok"


# =========================================================================
# Scoping
# =========================================================================


class TestScoping:

    def test_rite_has_local_scope(self):
        """Spec: "Variables declared inside a RITE are local to that rite"."""
        out = run_ath_via_c(
            'BIRTH x WITH "outer"; '
            'RITE test() { BIRTH x WITH "inner"; UTTER(x); } '
            'test(); UTTER(x); '
            'THIS.DIE();'
        )
        assert out.strip().splitlines() == ["inner", "outer"]

    def test_rite_can_read_global(self):
        """Spec: "Variables declared at the top level are global ... can access global x"."""
        out = run_ath_via_c(
            'BIRTH g WITH 100; '
            'RITE r() { BEQUEATH g + 1; } '
            'UTTER(r()); THIS.DIE();'
        )
        assert out.strip() == "101"


# =========================================================================
# Modules (watcher on .~ATH path)
# =========================================================================


class TestModules:

    def test_module_exports_rite_and_var(self, tmp_path):
        """Spec: "All top-level BIRTH variables, ENTOMB constants, and RITE definitions become exports"."""
        lib = tmp_path / "lib.~ATH"
        lib.write_text(
            'BIRTH greeting WITH "hi"; '
            'RITE add(a, b) { BEQUEATH a + b; } '
            'THIS.DIE();'
        )
        out = run_ath_via_c(
            f'import watcher Lib("{lib}"); '
            'UTTER(Lib.greeting); UTTER(Lib.add(3, 4)); THIS.DIE();',
            timeout=10.0,
        )
        assert out.strip().splitlines() == ["hi", "7"]

    def test_typeof_module(self, tmp_path):
        """Spec: "TYPEOF(ModuleName) returns "MODULE""."""
        lib = tmp_path / "lib.~ATH"
        lib.write_text('BIRTH x WITH 1; THIS.DIE();')
        out = run_ath_via_c(
            f'import watcher M("{lib}"); UTTER(TYPEOF(M)); THIS.DIE();',
            timeout=10.0,
        )
        assert out.strip() == "MODULE"

    def test_module_entomb_export(self, tmp_path):
        """Spec: top-level ENTOMB constants become exports."""
        lib = tmp_path / "lib.~ATH"
        lib.write_text('ENTOMB PI WITH 3; THIS.DIE();')
        out = run_ath_via_c(
            f'import watcher L("{lib}"); UTTER(L.PI); THIS.DIE();',
            timeout=10.0,
        )
        assert out.strip() == "3"


# =========================================================================
# HEED (stdin)
# =========================================================================


class TestHeed:

    def test_heed_reads_line(self):
        """Spec: "HEED() -- Read line from stdin, blocks until line entered"."""
        out = run_ath_via_c(
            'BIRTH s WITH HEED(); UTTER("got:" + s); THIS.DIE();',
            stdin_input="hello\n",
        )
        assert out.strip() == "got:hello"
