# Copyright (C) 2026 robinpie
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

"""C-specific behaviour tests for the !~ATH to C89 transpiler.

These tests document intentional differences between the C transpiler and
the Python reference interpreter.  They should all PASS; the expected values
reflect C semantics, not Python semantics.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


# ---------------------------------------------------------------------------
# Integer arithmetic
# ---------------------------------------------------------------------------

class TestIntegerArithmetic:
    """C uses long (64-bit on LP64), not Python's unbounded integers."""

    def test_basic_arithmetic(self):
        out = run_ath_via_c("""
            BIRTH a WITH 100;
            BIRTH b WITH 200;
            UTTER(a + b);
            THIS.DIE();
        """)
        assert out.strip() == "300"

    def test_integer_division_truncates(self):
        """Integer / integer → truncated integer, same as Python's //."""
        out = run_ath_via_c("""
            UTTER(7 / 2);
            THIS.DIE();
        """)
        assert out.strip() == "3"

    def test_negative_division_truncates_toward_zero(self):
        """C truncates toward zero; Python floors. Document the difference."""
        out = run_ath_via_c("""
            UTTER(-7 / 2);
            THIS.DIE();
        """)
        # C: -3 (truncate toward zero); Python: -4 (floor).
        # We document the C behaviour.
        assert out.strip() == "-3"

    def test_large_integer_fits_in_long(self):
        """Values within 64-bit signed range work correctly."""
        out = run_ath_via_c("""
            BIRTH x WITH 1000000000;
            UTTER(x * 1000);
            THIS.DIE();
        """)
        assert out.strip() == "1000000000000"

    def test_modulo_positive(self):
        out = run_ath_via_c("""
            UTTER(10 % 3);
            THIS.DIE();
        """)
        assert out.strip() == "1"

    def test_modulo_negative_dividend(self):
        """C modulo: sign follows dividend."""
        out = run_ath_via_c("""
            UTTER(-10 % 3);
            THIS.DIE();
        """)
        assert out.strip() == "-1"


# ---------------------------------------------------------------------------
# String / UTF-8 byte semantics
# ---------------------------------------------------------------------------

class TestStringBytes:
    """The C runtime treats strings as byte arrays, not Unicode codepoints."""

    def test_ascii_length(self):
        out = run_ath_via_c("""
            UTTER(LENGTH("hello"));
            THIS.DIE();
        """)
        assert out.strip() == "5"

    def test_utf8_length_returns_bytes(self):
        """LENGTH("☺") is 3 (UTF-8 bytes), not 1 (codepoint)."""
        out = run_ath_via_c("""
            UTTER(LENGTH("☺"));
            THIS.DIE();
        """)
        assert out.strip() == "3"

    def test_substring_bytes(self):
        """SUBSTRING operates on byte offsets."""
        out = run_ath_via_c("""
            UTTER(SUBSTRING("hello", 1, 4));
            THIS.DIE();
        """)
        assert out.strip() == "ell"

    def test_string_concat(self):
        out = run_ath_via_c("""
            BIRTH s WITH "Hello" + ", " + "world!";
            UTTER(s);
            THIS.DIE();
        """)
        assert out.strip() == "Hello, world!"

    def test_code_returns_byte_value(self):
        """CODE("A") returns 65 (ASCII/UTF-8 byte value of 'A')."""
        out = run_ath_via_c("""
            UTTER(CODE("A"));
            THIS.DIE();
        """)
        assert out.strip() == "65"

    def test_char_from_code(self):
        out = run_ath_via_c("""
            UTTER(CHAR(65));
            THIS.DIE();
        """)
        assert out.strip() == "A"


# ---------------------------------------------------------------------------
# Recursion depth
# ---------------------------------------------------------------------------

class TestRecursionDepth:
    """Sync rites recurse on the C call stack.  Shallow recursion works fine."""

    def test_recursion_depth_100(self):
        out = run_ath_via_c("""
            RITE count(n) {
                SHOULD n <= 0 { BEQUEATH 0; }
                BEQUEATH 1 + count(n - 1);
            }
            UTTER(count(100));
            THIS.DIE();
        """)
        assert out.strip() == "100"

    def test_fibonacci_10(self):
        out = run_ath_via_c("""
            RITE fib(n) {
                SHOULD n <= 1 { BEQUEATH n; }
                BEQUEATH fib(n-1) + fib(n-2);
            }
            UTTER(fib(10));
            THIS.DIE();
        """)
        assert out.strip() == "55"

    def test_tail_recursive_sum(self):
        """Large iteration via timer-chained CPS works without stack growth."""
        out = run_ath_via_c("""
            BIRTH total WITH 0;
            BIRTH i WITH 0;

            RITE step() {
                SHOULD i < 100 {
                    total = total + i;
                    i = i + 1;
                    import timer T(1ms);
                    ~ATH(T) {} EXECUTE(step());
                }
            }
            step();
            THIS.DIE();
        """, timeout=10.0)
        assert out.strip() == ""   # no output — just verifying no crash/timeout

    def test_deep_map_nesting(self):
        out = run_ath_via_c("""
            BIRTH m WITH {a: {b: {c: 42}}};
            UTTER(m.a.b.c);
            THIS.DIE();
        """)
        assert out.strip() == "42"


# ---------------------------------------------------------------------------
# Timer precision
# ---------------------------------------------------------------------------

class TestTimerPrecision:
    """Timers may fire slightly late; tests check output, not timing."""

    def test_1ms_timer_fires(self):
        out = run_ath_via_c("""
            import timer T(1ms);
            ~ATH(T) {} EXECUTE(UTTER("done"));
            THIS.DIE();
        """)
        assert out.strip() == "done"

    def test_chained_timers(self):
        out = run_ath_via_c("""
            import timer T1(1ms);
            ~ATH(T1) {} EXECUTE(
                import timer T2(1ms);
                ~ATH(T2) {} EXECUTE(UTTER("chained"));
            );
            THIS.DIE();
        """)
        assert out.strip() == "chained"

    def test_multiple_timers_order(self):
        """A 1ms timer fires before a 100ms timer."""
        out = run_ath_via_c("""
            import timer TFast(1ms);
            import timer TSlow(100ms);
            ~ATH(TFast) {} EXECUTE(UTTER("fast"));
            ~ATH(TSlow) {} EXECUTE(UTTER("slow"));
            THIS.DIE();
        """, timeout=5.0)
        lines = [l for l in out.strip().splitlines() if l]
        assert lines[0] == "fast"
        assert lines[1] == "slow"


# ---------------------------------------------------------------------------
# Float behaviour
# ---------------------------------------------------------------------------

class TestFloatBehaviour:
    """IEEE 754 double, same representation as Python's float."""

    def test_float_addition(self):
        out = run_ath_via_c("""
            UTTER(1.5 + 2.5);
            THIS.DIE();
        """)
        assert out.strip() == "4.0"

    def test_int_to_float_conversion(self):
        out = run_ath_via_c("""
            BIRTH f WITH FLOAT(7);
            UTTER(f);
            THIS.DIE();
        """)
        assert out.strip() == "7.0"

    def test_float_to_int_truncation(self):
        out = run_ath_via_c("""
            UTTER(INT(3.9));
            THIS.DIE();
        """)
        assert out.strip() == "3"

    def test_mixed_arithmetic(self):
        out = run_ath_via_c("""
            UTTER(3 + 0.5);
            THIS.DIE();
        """)
        assert out.strip() == "3.5"


# ---------------------------------------------------------------------------
# Type system
# ---------------------------------------------------------------------------

class TestTypeSystem:
    """TYPEOF behaviour matches the Python interpreter exactly."""

    def test_typeof_void(self):
        out = run_ath_via_c("""UTTER(TYPEOF(VOID)); THIS.DIE();""")
        assert out.strip() == "VOID"

    def test_typeof_boolean(self):
        out = run_ath_via_c("""UTTER(TYPEOF(ALIVE)); THIS.DIE();""")
        assert out.strip() == "BOOLEAN"

    def test_typeof_integer(self):
        out = run_ath_via_c("""UTTER(TYPEOF(42)); THIS.DIE();""")
        assert out.strip() == "INTEGER"

    def test_typeof_float(self):
        out = run_ath_via_c("""UTTER(TYPEOF(3.14)); THIS.DIE();""")
        assert out.strip() == "FLOAT"

    def test_typeof_string(self):
        out = run_ath_via_c("""UTTER(TYPEOF("hi")); THIS.DIE();""")
        assert out.strip() == "STRING"

    def test_typeof_array(self):
        out = run_ath_via_c("""UTTER(TYPEOF([1,2])); THIS.DIE();""")
        assert out.strip() == "ARRAY"

    def test_typeof_map(self):
        out = run_ath_via_c("""UTTER(TYPEOF({a:1})); THIS.DIE();""")
        assert out.strip() == "MAP"


# ---------------------------------------------------------------------------
# Bitwise operations
# ---------------------------------------------------------------------------

class TestBitwiseOps:
    def test_and(self):
        out = run_ath_via_c("""UTTER(12 & 10); THIS.DIE();""")
        assert out.strip() == "8"

    def test_or(self):
        out = run_ath_via_c("""UTTER(12 | 10); THIS.DIE();""")
        assert out.strip() == "14"

    def test_xor(self):
        out = run_ath_via_c("""UTTER(12 ^ 10); THIS.DIE();""")
        assert out.strip() == "6"

    def test_not(self):
        out = run_ath_via_c("""UTTER(~0); THIS.DIE();""")
        assert out.strip() == "-1"

    def test_lshift(self):
        out = run_ath_via_c("""UTTER(1 << 4); THIS.DIE();""")
        assert out.strip() == "16"

    def test_rshift(self):
        out = run_ath_via_c("""UTTER(256 >> 3); THIS.DIE();""")
        assert out.strip() == "32"
