"""Regression tests for bugs found while bringing the C transpiler up.

These tests pin down behaviour the codebase has lost before and may lose again.
Each test class corresponds to a category of bug, and each test names the
specific failure mode it guards against.
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


# ---------------------------------------------------------------------------
# Parser: long chains of associative operators
# ---------------------------------------------------------------------------
# Background: parseOr/parseAnd/parseEq/parseCmp/parseBitOr/etc. originally
# handled only ONE infix operator per call, so `a OR b OR c` would parse `a OR b`
# and then leave `OR c` unconsumed, producing "expected ; got OR".

class TestChainedOperators:
    def test_three_ors(self):
        out = run_ath_via_c("""
            BIRTH r WITH DEAD OR DEAD OR ALIVE;
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"

    def test_three_ands(self):
        out = run_ath_via_c("""
            BIRTH r WITH ALIVE AND ALIVE AND DEAD;
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "DEAD"

    def test_ten_chained_ors(self):
        out = run_ath_via_c("""
            BIRTH r WITH DEAD OR DEAD OR DEAD OR DEAD OR DEAD
                       OR DEAD OR DEAD OR DEAD OR DEAD OR ALIVE;
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"

    def test_mixed_and_or_with_precedence(self):
        # AND binds tighter than OR: parsed as DEAD OR (ALIVE AND ALIVE) OR DEAD
        out = run_ath_via_c("""
            BIRTH r WITH DEAD OR ALIVE AND ALIVE OR DEAD;
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"

    def test_three_chained_eq(self):
        # Even (a == b) == (c == d) — equality chain triggers the same loop bug
        out = run_ath_via_c("""
            BIRTH r WITH (1 == 1) == (2 == 2);
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"

    def test_chained_bitwise_or(self):
        out = run_ath_via_c("""
            UTTER(1 | 2 | 4 | 8);
            THIS.DIE();
        """)
        assert out.strip() == "15"

    def test_chained_bitwise_and(self):
        out = run_ath_via_c("""
            UTTER(15 & 14 & 12 & 8);
            THIS.DIE();
        """)
        assert out.strip() == "8"

    def test_long_compound_condition(self):
        out = run_ath_via_c("""
            BIRTH c WITH 65;
            BIRTH ch WITH "A";
            BIRTH r WITH (c >= 65 AND c <= 90)
                      OR (c >= 97 AND c <= 122)
                      OR ch == "_";
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"


# ---------------------------------------------------------------------------
# CPS: top-level statement count beyond the old unroll limit
# ---------------------------------------------------------------------------
# Background: stmtsAsync / analyze unrolled their loops only ~20 times,
# so any program with more than 20 top-level statements failed to detect
# async rites declared past that point. A recursive async rite past stmt 20
# would silently be generated as sync, and its ~ATH waits would be no-ops.

class TestManyTopLevelStatements:
    def test_30_birth_statements_then_async_rite(self):
        # 30 BIRTHs to push the async rite past the old 20-stmt cutoff.
        births = "\n".join(f"BIRTH v{i} WITH {i};" for i in range(30))
        out = run_ath_via_c(births + """
            BIRTH counter WITH 0;
            RITE tick(n) {
                SHOULD n <= 0 { BEQUEATH VOID; }
                counter = counter + 1;
                import timer T(1ms);
                ~ATH(T) {} EXECUTE(tick(n - 1));
            }
            tick(3);
            THIS.DIE();
        """, timeout=5.0)
        # Three timer-driven iterations means counter ends at 3.
        # If `tick` was incorrectly classified as sync, ~ATH(T) becomes a no-op,
        # tick recurses once, and counter would end at 1.
        # We can't UTTER counter here (timing), so we instead verify that the
        # rite completes by making it print a marker after counter increments.
        # Re-run with a marker test:
        out = run_ath_via_c(births + """
            BIRTH counter WITH 0;
            RITE tick(n) {
                SHOULD n <= 0 {
                    UTTER("done counter=" + STRING(counter));
                    BEQUEATH VOID;
                }
                counter = counter + 1;
                import timer T(1ms);
                ~ATH(T) {} EXECUTE(tick(n - 1));
            }
            tick(3);
        """, timeout=5.0)
        assert out.strip() == "done counter=3"

    def test_many_top_level_stmts_with_should(self):
        # SHOULD is processed by stmtAsync too — make sure the loop covers it.
        prelude = "\n".join(f"BIRTH x{i} WITH {i};" for i in range(25))
        out = run_ath_via_c(prelude + """
            SHOULD x10 == 10 {
                UTTER("ok");
            }
            THIS.DIE();
        """)
        assert out.strip() == "ok"


# ---------------------------------------------------------------------------
# Entity NOT: ~ATH(!T) must yield to the event loop
# ---------------------------------------------------------------------------
# Background: `EntityNot` was treated as sync (no actual wait), so
# `~ATH(!T) {} EXECUTE(recurse())` would recurse synchronously on the C
# stack, blowing it up immediately and producing one iteration of output
# (or a stack overflow on deeper recursion).

class TestNotEntityAsync:
    def test_not_entity_immediate_chain(self):
        out = run_ath_via_c("""
            RITE step(n) {
                SHOULD n > 5 { BEQUEATH VOID; }
                UTTER(n);
                import timer T(1ms);
                ~ATH(!T) {} EXECUTE(step(n + 1));
            }
            step(1);
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "1\n2\n3\n4\n5"

    def test_not_entity_deep_chain(self):
        # 200 iterations would stack-overflow with synchronous recursion;
        # event-loop scheduling unwinds the C stack between iterations.
        out = run_ath_via_c("""
            BIRTH count WITH 0;
            RITE step(n) {
                SHOULD n <= 0 {
                    UTTER("done at " + STRING(count));
                    BEQUEATH VOID;
                }
                count = count + 1;
                import timer T(1ms);
                ~ATH(!T) {} EXECUTE(step(n - 1));
            }
            step(200);
        """, timeout=10.0)
        assert out.strip() == "done at 200"


# ---------------------------------------------------------------------------
# Codegen: async rites declared inside branch bodies
# ---------------------------------------------------------------------------
# Background: when a `RiteDef` for an async rite appeared inside a branch's
# `~ATH(BRANCH) {…}` body, its struct + helper functions were emitted INLINE
# in the branch's CPS segment (i.e. function definitions nested inside another
# function, which is invalid C).

class TestRiteInsideBranch:
    def test_async_rite_inside_branch_body(self):
        out = run_ath_via_c("""
            BIRTH counter WITH 0;
            bifurcate THIS[A, B];

            ~ATH(A) {
                RITE tick_a() {
                    SHOULD counter >= 3 { BEQUEATH VOID; }
                    counter = counter + 1;
                    UTTER("A " + STRING(counter));
                    import timer t(1ms);
                    ~ATH(t) {} EXECUTE(tick_a());
                }
                tick_a();
            } EXECUTE(UTTER("A done"));

            ~ATH(B) {
                UTTER("B ran");
            } EXECUTE(VOID);

            [A, B].DIE();
        """, timeout=5.0)
        # We don't pin precise interleaving (branches race) but we DO require
        # the program to compile and run to completion. Both A's three ticks
        # and the "A done" marker must appear, and B must run.
        lines = out.strip().splitlines()
        assert "A 1" in lines
        assert "A 2" in lines
        assert "A 3" in lines
        assert "A done" in lines
        assert "B ran" in lines

    def test_sync_rite_inside_branch_body(self):
        out = run_ath_via_c("""
            bifurcate THIS[A, B];

            ~ATH(A) {
                RITE add(x, y) { BEQUEATH x + y; }
                UTTER(add(3, 4));
            } EXECUTE(VOID);

            ~ATH(B) {
                UTTER("b");
            } EXECUTE(VOID);

            [A, B].DIE();
        """)
        lines = sorted(out.strip().splitlines())
        assert lines == ["7", "b"]


# ---------------------------------------------------------------------------
# Codegen: BEQUEATH must respect sync vs async context
# ---------------------------------------------------------------------------
# Background: BEQUEATH always emitted `_ret=...; goto _done;` regardless of
# context. In an async rite that has no `_done` label, this is a compile
# error. The fix: in async context, emit
# `ath_cont_resume(f->base.next, value); frame_free(f); return;`.

class TestBequeathContext:
    def test_async_rite_bequeath_returns_to_caller(self):
        # The recursive call's BEQUEATH should pop one level of the async
        # call chain — without the fix the resulting program either fails
        # to compile or behaves incorrectly.
        out = run_ath_via_c("""
            BIRTH total WITH 0;
            RITE add_all(n) {
                SHOULD n <= 0 { BEQUEATH VOID; }
                total = total + n;
                import timer t(1ms);
                ~ATH(t) {} EXECUTE(add_all(n - 1));
            }
            add_all(5);
            import timer wait(50ms);
            ~ATH(wait) {} EXECUTE(UTTER(total));
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "15"

    def test_sync_rite_bequeath_returns_value(self):
        out = run_ath_via_c("""
            RITE id(x) { BEQUEATH x; }
            UTTER(id(42));
            THIS.DIE();
        """)
        assert out.strip() == "42"

    def test_sync_rite_bequeath_void(self):
        out = run_ath_via_c("""
            RITE log_it(x) {
                UTTER("logging");
                BEQUEATH;
            }
            BIRTH r WITH log_it(99);
            UTTER(TYPEOF(r));
            THIS.DIE();
        """)
        assert out.strip() == "logging\nVOID"


# ---------------------------------------------------------------------------
# Codegen: temp slot reset and stack discipline
# ---------------------------------------------------------------------------
# Background: a single huge sync rite (e.g. the lexer's tokenizeAt) used
# >1000 temp slots if the codegen never reset the temp counter between
# statements. The fix resets `tc` between statements so a single statement's
# expressions reuse slots `_t[0..N]`. The hard-coded `_t[]` size is now small
# (128) and the array is heap-allocated to keep the C stack frame thin.

class TestLargeFunctions:
    def test_function_with_many_statements(self):
        # 30 BIRTH+UTTER pairs inside one rite → 60 sequential statements.
        # If `tc` weren't reset, the temp index would exceed 128 and either
        # smash the stack or fail to compile.
        body = "\n".join(
            f'BIRTH v{i} WITH {i}; UTTER("v" + STRING(v{i}));'
            for i in range(30)
        )
        out = run_ath_via_c(f"""
            RITE big() {{
                {body}
            }}
            big();
            THIS.DIE();
        """)
        expected = "\n".join(f"v{i}" for i in range(30))
        assert out.strip() == expected

    def test_deeply_nested_expression(self):
        # 25 +1 operators in one expression — within a single statement,
        # temp count grows but should still fit in _t[128].
        chain = "+1" * 25
        out = run_ath_via_c(f"""
            UTTER(0{chain});
            THIS.DIE();
        """)
        assert out.strip() == "25"


# ---------------------------------------------------------------------------
# Entity expressions: nesting and combinations
# ---------------------------------------------------------------------------
# Spec coverage that the existing test_e2e.py doesn't fully exercise.

class TestEntityCombinations:
    def test_entity_and_both_die(self):
        out = run_ath_via_c("""
            import timer T1(1ms);
            import timer T2(2ms);
            ~ATH(T1 && T2) {} EXECUTE(UTTER("both"));
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "both"

    def test_entity_or_first_wins(self):
        out = run_ath_via_c("""
            import timer Fast(1ms);
            import timer Slow(100ms);
            ~ATH(Fast || Slow) {} EXECUTE(UTTER("one"));
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "one"

    def test_not_entity_fires_immediately(self):
        out = run_ath_via_c("""
            import timer T(1h);
            ~ATH(!T) {} EXECUTE(UTTER("now"));
            THIS.DIE();
        """, timeout=2.0)
        assert out.strip() == "now"

    def test_nested_entity_expression(self):
        out = run_ath_via_c("""
            import timer T1(1ms);
            import timer T2(2ms);
            import timer T3(50ms);
            ~ATH((T1 && T2) || T3) {} EXECUTE(UTTER("first group"));
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "first group"


# ---------------------------------------------------------------------------
# Bifurcation patterns
# ---------------------------------------------------------------------------

class TestBifurcation:
    def test_branches_share_variables(self):
        out = run_ath_via_c("""
            BIRTH counter WITH 0;
            bifurcate THIS[A, B];

            ~ATH(A) {
                counter = counter + 10;
            } EXECUTE(VOID);

            ~ATH(B) {
                counter = counter + 100;
            } EXECUTE(VOID);

            [A, B].DIE();
            import timer W(20ms);
            ~ATH(W) {} EXECUTE(UTTER(counter));
            THIS.DIE();
        """, timeout=5.0)
        # Branches run cooperatively, no true parallelism — both updates
        # land before the timer-gated UTTER.
        assert out.strip() == "110"

    def test_branch_runs_full_subtree_before_dying(self):
        # A branch is alive until all its nested ~ATH waits complete.
        out = run_ath_via_c("""
            bifurcate THIS[A, B];

            ~ATH(A) {
                import timer T(20ms);
                ~ATH(T) {} EXECUTE(UTTER("A inner done"));
            } EXECUTE(UTTER("A branch done"));

            ~ATH(B) {
                UTTER("B ran");
            } EXECUTE(VOID);

            [A, B].DIE();
        """, timeout=5.0)
        lines = out.strip().splitlines()
        # B finishes first (no waits), then A's inner timer fires, then
        # A's EXECUTE clause runs.
        assert lines[0] == "B ran"
        assert "A inner done" in lines
        assert "A branch done" in lines
        assert lines.index("A inner done") < lines.index("A branch done")


# ---------------------------------------------------------------------------
# Error handling
# ---------------------------------------------------------------------------

class TestErrorHandling:
    def test_salvage_binds_error_message(self):
        out = run_ath_via_c("""
            ATTEMPT {
                CONDEMN "boom";
            } SALVAGE e {
                UTTER("caught: " + e);
            }
            THIS.DIE();
        """)
        assert out.strip() == "caught: boom"

    def test_nested_attempt(self):
        out = run_ath_via_c("""
            ATTEMPT {
                ATTEMPT {
                    CONDEMN "inner";
                } SALVAGE e {
                    UTTER("inner caught: " + e);
                    CONDEMN "outer";
                }
            } SALVAGE e {
                UTTER("outer caught: " + e);
            }
            THIS.DIE();
        """)
        assert out.strip() == "inner caught: inner\nouter caught: outer"

    def test_attempt_no_error(self):
        out = run_ath_via_c("""
            ATTEMPT {
                UTTER("ok");
            } SALVAGE e {
                UTTER("should not run");
            }
            THIS.DIE();
        """)
        assert out.strip() == "ok"

    def test_condemn_with_computed_message(self):
        out = run_ath_via_c("""
            ATTEMPT {
                BIRTH n WITH 5;
                CONDEMN "value=" + STRING(n);
            } SALVAGE e {
                UTTER(e);
            }
            THIS.DIE();
        """)
        assert out.strip() == "value=5"


# ---------------------------------------------------------------------------
# Operator precedence — full table per spec §"Operator Precedence"
# ---------------------------------------------------------------------------

class TestOperatorPrecedence:
    def test_unary_binds_tighter_than_multiplication(self):
        out = run_ath_via_c("""
            UTTER(-3 * 2);
            THIS.DIE();
        """)
        assert out.strip() == "-6"

    def test_multiplication_binds_tighter_than_addition(self):
        out = run_ath_via_c("""
            UTTER(2 + 3 * 4);
            THIS.DIE();
        """)
        assert out.strip() == "14"

    def test_shift_binds_tighter_than_bitwise_and(self):
        # 1 << 2 & 5 should be (1 << 2) & 5 = 4 & 5 = 4
        out = run_ath_via_c("""
            UTTER(1 << 2 & 5);
            THIS.DIE();
        """)
        assert out.strip() == "4"

    def test_bitwise_and_binds_tighter_than_xor(self):
        # 6 & 3 ^ 5 should be (6 & 3) ^ 5 = 2 ^ 5 = 7
        out = run_ath_via_c("""
            UTTER(6 & 3 ^ 5);
            THIS.DIE();
        """)
        assert out.strip() == "7"

    def test_xor_binds_tighter_than_bitwise_or(self):
        # 1 ^ 2 | 4 should be (1 ^ 2) | 4 = 3 | 4 = 7
        out = run_ath_via_c("""
            UTTER(1 ^ 2 | 4);
            THIS.DIE();
        """)
        assert out.strip() == "7"

    def test_comparison_binds_tighter_than_and(self):
        out = run_ath_via_c("""
            UTTER(1 < 2 AND 3 < 4);
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE"

    def test_and_binds_tighter_than_or(self):
        # DEAD OR ALIVE AND DEAD → DEAD OR (ALIVE AND DEAD) → DEAD OR DEAD → DEAD
        out = run_ath_via_c("""
            UTTER(DEAD OR ALIVE AND DEAD);
            THIS.DIE();
        """)
        assert out.strip() == "DEAD"

    def test_parens_override_precedence(self):
        out = run_ath_via_c("""
            UTTER((2 + 3) * 4);
            THIS.DIE();
        """)
        assert out.strip() == "20"


# ---------------------------------------------------------------------------
# Spec built-ins — corner cases
# ---------------------------------------------------------------------------

class TestBuiltins:
    def test_bin_zero(self):
        out = run_ath_via_c("""UTTER(BIN(0)); THIS.DIE();""")
        assert out.strip() == "0"

    def test_bin_one(self):
        out = run_ath_via_c("""UTTER(BIN(1)); THIS.DIE();""")
        assert out.strip() == "1"

    def test_hex_zero(self):
        out = run_ath_via_c("""UTTER(HEX(0)); THIS.DIE();""")
        assert out.strip() == "0"

    def test_hex_large(self):
        out = run_ath_via_c("""UTTER(HEX(65535)); THIS.DIE();""")
        # Spec example shows uppercase
        assert out.strip().upper() == "FFFF"

    def test_split_to_chars(self):
        out = run_ath_via_c("""
            BIRTH parts WITH SPLIT("abc", "");
            UTTER(STRING(parts));
            UTTER(LENGTH(parts));
            THIS.DIE();
        """)
        lines = out.strip().splitlines()
        assert lines[0] == "[a, b, c]"
        assert lines[1] == "3"

    def test_join_empty_array(self):
        out = run_ath_via_c("""
            UTTER("[" + JOIN([], ",") + "]");
            THIS.DIE();
        """)
        assert out.strip() == "[]"

    def test_join_single_element(self):
        out = run_ath_via_c("""
            UTTER(JOIN(["only"], ","));
            THIS.DIE();
        """)
        assert out.strip() == "only"

    def test_replace_no_match(self):
        out = run_ath_via_c("""
            UTTER(REPLACE("hello", "x", "y"));
            THIS.DIE();
        """)
        assert out.strip() == "hello"

    def test_trim_only_whitespace(self):
        out = run_ath_via_c('''
            BIRTH s WITH TRIM("   \\t  ");
            UTTER("[" + s + "]");
            UTTER(LENGTH(s));
            THIS.DIE();
        ''')
        lines = out.strip().splitlines()
        assert lines[0] == "[]"
        assert lines[1] == "0"

    def test_uppercase_mixed(self):
        out = run_ath_via_c("""
            UTTER(UPPERCASE("AbC123"));
            THIS.DIE();
        """)
        assert out.strip() == "ABC123"

    def test_has_returns_boolean(self):
        out = run_ath_via_c("""
            BIRTH m WITH {a: 1};
            UTTER(TYPEOF(HAS(m, "a")));
            UTTER(TYPEOF(HAS(m, "b")));
            THIS.DIE();
        """)
        assert out.strip() == "BOOLEAN\nBOOLEAN"

    def test_set_returns_new_map(self):
        # SET must not mutate the original (immutable semantics per spec).
        out = run_ath_via_c("""
            BIRTH a WITH {x: 1};
            BIRTH b WITH SET(a, "y", 2);
            UTTER(HAS(a, "y"));
            UTTER(HAS(b, "y"));
            THIS.DIE();
        """)
        assert out.strip() == "DEAD\nALIVE"

    def test_append_returns_new_array(self):
        out = run_ath_via_c("""
            BIRTH a WITH [1, 2];
            BIRTH b WITH APPEND(a, 3);
            UTTER(LENGTH(a));
            UTTER(LENGTH(b));
            THIS.DIE();
        """)
        assert out.strip() == "2\n3"

    def test_random_in_unit_range(self):
        out = run_ath_via_c("""
            BIRTH r WITH RANDOM();
            UTTER(r >= 0.0 AND r < 1.0);
            UTTER(TYPEOF(r));
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE\nFLOAT"

    def test_random_int_inclusive_range(self):
        out = run_ath_via_c("""
            BIRTH lo WITH 5;
            BIRTH hi WITH 5;
            BIRTH r WITH RANDOM_INT(lo, hi);
            UTTER(r);
            UTTER(TYPEOF(r));
            THIS.DIE();
        """)
        assert out.strip() == "5\nINTEGER"

    def test_time_is_positive_integer(self):
        out = run_ath_via_c("""
            BIRTH t WITH TIME();
            UTTER(TYPEOF(t));
            UTTER(t > 0);
            THIS.DIE();
        """)
        assert out.strip() == "INTEGER\nALIVE"

    def test_char_code_roundtrip(self):
        out = run_ath_via_c("""
            UTTER(CHAR(CODE("Z")));
            UTTER(CODE(CHAR(48)));
            THIS.DIE();
        """)
        assert out.strip() == "Z\n48"


# ---------------------------------------------------------------------------
# String escapes per spec §"Strings"
# ---------------------------------------------------------------------------

class TestStringEscapes:
    def test_newline_escape(self):
        out = run_ath_via_c(r'''
            UTTER("a\nb");
            THIS.DIE();
        ''')
        assert out == "a\nb\n"

    def test_tab_escape(self):
        out = run_ath_via_c(r'''
            UTTER("x\ty");
            THIS.DIE();
        ''')
        assert out == "x\ty\n"

    def test_quote_escape(self):
        out = run_ath_via_c(r'''
            UTTER("say \"hi\"");
            THIS.DIE();
        ''')
        assert out == 'say "hi"\n'

    def test_backslash_escape(self):
        out = run_ath_via_c(r'''
            UTTER("c:\\path");
            THIS.DIE();
        ''')
        assert out == "c:\\path\n"


# ---------------------------------------------------------------------------
# Control flow: SHOULD / LEST chains per spec §"Control Flow"
# ---------------------------------------------------------------------------

class TestControlFlow:
    def test_lest_should_chain(self):
        out = run_ath_via_c("""
            RITE classify(n) {
                SHOULD n < 0 { BEQUEATH "neg"; }
                LEST SHOULD n == 0 { BEQUEATH "zero"; }
                LEST SHOULD n < 10 { BEQUEATH "small"; }
                LEST { BEQUEATH "big"; }
            }
            UTTER(classify(-5));
            UTTER(classify(0));
            UTTER(classify(7));
            UTTER(classify(100));
            THIS.DIE();
        """)
        assert out.strip() == "neg\nzero\nsmall\nbig"

    def test_should_with_no_lest(self):
        out = run_ath_via_c("""
            BIRTH r WITH "untouched";
            SHOULD ALIVE { r = "yes"; }
            UTTER(r);
            r = "x";
            SHOULD DEAD { r = "no"; }
            UTTER(r);
            THIS.DIE();
        """)
        assert out.strip() == "yes\nx"

    def test_truthiness_of_collections(self):
        # Spec: empty array, empty map, empty string, VOID, DEAD, 0 are falsy
        out = run_ath_via_c("""
            BIRTH falsy_count WITH 0;
            SHOULD NOT [] { falsy_count = falsy_count + 1; }
            SHOULD NOT {} { falsy_count = falsy_count + 1; }
            SHOULD NOT "" { falsy_count = falsy_count + 1; }
            SHOULD NOT VOID { falsy_count = falsy_count + 1; }
            SHOULD NOT DEAD { falsy_count = falsy_count + 1; }
            SHOULD NOT 0 { falsy_count = falsy_count + 1; }
            UTTER(falsy_count);
            THIS.DIE();
        """)
        assert out.strip() == "6"

    def test_truthiness_of_nonempty(self):
        out = run_ath_via_c("""
            BIRTH truthy_count WITH 0;
            SHOULD [0] { truthy_count = truthy_count + 1; }
            SHOULD {a:0} { truthy_count = truthy_count + 1; }
            SHOULD "x" { truthy_count = truthy_count + 1; }
            SHOULD ALIVE { truthy_count = truthy_count + 1; }
            SHOULD 1 { truthy_count = truthy_count + 1; }
            SHOULD 0.5 { truthy_count = truthy_count + 1; }
            UTTER(truthy_count);
            THIS.DIE();
        """)
        assert out.strip() == "6"


# ---------------------------------------------------------------------------
# Rites: recursion and closures
# ---------------------------------------------------------------------------

class TestRiteCorrectness:
    def test_mutual_recursion(self):
        out = run_ath_via_c("""
            RITE is_even(n) {
                SHOULD n == 0 { BEQUEATH ALIVE; }
                BEQUEATH is_odd(n - 1);
            }
            RITE is_odd(n) {
                SHOULD n == 0 { BEQUEATH DEAD; }
                BEQUEATH is_even(n - 1);
            }
            UTTER(is_even(10));
            UTTER(is_odd(7));
            UTTER(is_even(1));
            THIS.DIE();
        """)
        assert out.strip() == "ALIVE\nALIVE\nDEAD"

    def test_rite_captures_outer_variable(self):
        out = run_ath_via_c("""
            BIRTH base WITH 100;
            RITE add_base(x) { BEQUEATH x + base; }
            UTTER(add_base(5));
            base = 200;
            UTTER(add_base(5));
            THIS.DIE();
        """)
        assert out.strip() == "105\n205"

    def test_rite_local_shadows_outer(self):
        out = run_ath_via_c("""
            BIRTH x WITH 1;
            RITE shadow() {
                BIRTH x WITH 99;
                BEQUEATH x;
            }
            UTTER(shadow());
            UTTER(x);
            THIS.DIE();
        """)
        assert out.strip() == "99\n1"

    def test_rite_returning_void_implicitly(self):
        out = run_ath_via_c("""
            RITE nop() { UTTER("hi"); }
            BIRTH r WITH nop();
            UTTER(TYPEOF(r));
            THIS.DIE();
        """)
        assert out.strip() == "hi\nVOID"


# ---------------------------------------------------------------------------
# Composition: timer-chained iteration over many items
# ---------------------------------------------------------------------------

class TestAsyncIteration:
    def test_sum_via_timer_chain(self):
        out = run_ath_via_c("""
            BIRTH total WITH 0;
            RITE sum_to(n) {
                SHOULD n <= 0 {
                    UTTER(total);
                    BEQUEATH VOID;
                }
                total = total + n;
                import timer t(1ms);
                ~ATH(t) {} EXECUTE(sum_to(n - 1));
            }
            sum_to(10);
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "55"

    def test_collect_via_timer_chain(self):
        out = run_ath_via_c("""
            BIRTH result WITH [];
            RITE collect(n, max) {
                SHOULD n > max {
                    UTTER(STRING(result));
                    BEQUEATH VOID;
                }
                result = APPEND(result, n * 2);
                import timer t(1ms);
                ~ATH(t) {} EXECUTE(collect(n + 1, max));
            }
            collect(1, 5);
            THIS.DIE();
        """, timeout=5.0)
        assert out.strip() == "[2, 4, 6, 8, 10]"
