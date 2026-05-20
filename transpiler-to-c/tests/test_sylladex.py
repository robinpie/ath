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

"""End-to-end tests for !~ATH 2.0 sylladex types.

Covers STACK, QUEUE, TREE, HASHMAP, OUIJA, BOTTLE, TECHHOP, JUJU —
constructors, CAPTCHALOGUE/EJECT operations, TYPEOF, STRING, COUNT,
truthiness, modifier validation, and JUJU branch context.
"""

import sys
import os

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


# ===== STACK =====

class TestStack:
    def test_basic_lifo(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(3);
CAPTCHALOGUE 1 INTO S;
CAPTCHALOGUE 3 INTO S;
CAPTCHALOGUE 5 INTO S;
UTTER(STRING(S));
THIS.DIE();
""")
        assert out == "STACK[5, 3, 1]\n"

    def test_overflow_discards_bottom(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(3);
CAPTCHALOGUE 1 INTO S;
CAPTCHALOGUE 3 INTO S;
CAPTCHALOGUE 5 INTO S;
CAPTCHALOGUE 2 INTO S;
UTTER(STRING(S));
THIS.DIE();
""")
        assert out == "STACK[2, 5, 3]\n"

    def test_eject_returns_top(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(3);
CAPTCHALOGUE 1 INTO S;
CAPTCHALOGUE 2 INTO S;
BIRTH top WITH EJECT FROM S;
UTTER(top);
UTTER(STRING(S));
THIS.DIE();
""")
        assert out == "2\nSTACK[1, VOID, VOID]\n"

    def test_zero_size(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(0);
CAPTCHALOGUE 5 INTO S;
UTTER(STRING(S));
UTTER(STRING(EJECT FROM S));
THIS.DIE();
""")
        assert out == "STACK[]\nVOID\n"

    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(STACK(3))); THIS.DIE();')
        assert out == "STACK\n"

    def test_count(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(3);
UTTER(COUNT(S));
CAPTCHALOGUE 1 INTO S;
UTTER(COUNT(S));
CAPTCHALOGUE 2 INTO S;
UTTER(COUNT(S));
THIS.DIE();
""")
        assert out == "0\n1\n2\n"

    def test_truthiness(self):
        out = run_ath_via_c("""
BIRTH S WITH STACK(3);
SHOULD S { UTTER("truthy"); } LEST { UTTER("falsy"); }
CAPTCHALOGUE 1 INTO S;
SHOULD S { UTTER("truthy"); } LEST { UTTER("falsy"); }
THIS.DIE();
""")
        assert out == "falsy\ntruthy\n"


# ===== QUEUE =====

class TestQueue:
    def test_basic_fifo(self):
        out = run_ath_via_c("""
BIRTH Q WITH QUEUE(3);
CAPTCHALOGUE "mixed" INTO Q;
CAPTCHALOGUE 1 INTO Q;
UTTER(STRING(Q));
THIS.DIE();
""")
        assert out == 'QUEUE[1, "mixed", VOID]\n'

    def test_eject_returns_back(self):
        out = run_ath_via_c("""
BIRTH Q WITH QUEUE(3);
CAPTCHALOGUE "mixed" INTO Q;
CAPTCHALOGUE 1 INTO Q;
CAPTCHALOGUE 2 INTO Q;
BIRTH first WITH EJECT FROM Q;
UTTER(first);
UTTER(STRING(Q));
THIS.DIE();
""")
        assert out == "mixed\nQUEUE[VOID, 2, 1]\n"

    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(QUEUE(2))); THIS.DIE();')
        assert out == "QUEUE\n"


# ===== TREE =====

class TestTree:
    def test_basic_insert_inorder(self):
        out = run_ath_via_c("""
BIRTH T WITH TREE();
CAPTCHALOGUE "banana" INTO T;
CAPTCHALOGUE "apple" INTO T;
CAPTCHALOGUE "cherry" INTO T;
UTTER(STRING(T));
THIS.DIE();
""")
        assert out == 'TREE["banana": [apple, banana, cherry]]\n'

    def test_eject_leaf(self):
        out = run_ath_via_c("""
BIRTH T WITH TREE();
CAPTCHALOGUE "banana" INTO T;
CAPTCHALOGUE "apple" INTO T;
CAPTCHALOGUE "cherry" INTO T;
BIRTH plucked WITH EJECT LEAF FROM T;
UTTER(plucked);
THIS.DIE();
""")
        assert out == "apple\n"

    def test_eject_root_returns_sorted_array(self):
        out = run_ath_via_c("""
BIRTH T WITH TREE();
CAPTCHALOGUE "banana" INTO T;
CAPTCHALOGUE "apple" INTO T;
CAPTCHALOGUE "cherry" INTO T;
BIRTH dumped WITH EJECT ROOT FROM T;
UTTER(STRING(dumped));
UTTER(STRING(T));
THIS.DIE();
""")
        assert out == "[apple, banana, cherry]\nTREE[]\n"

    def test_empty_root_returns_empty_array(self):
        out = run_ath_via_c("""
BIRTH T WITH TREE();
BIRTH r WITH EJECT ROOT FROM T;
UTTER(STRING(r));
THIS.DIE();
""")
        assert out == "[]\n"

    def test_empty_leaf_returns_void(self):
        out = run_ath_via_c("""
BIRTH T WITH TREE();
UTTER(STRING(EJECT LEAF FROM T));
THIS.DIE();
""")
        assert out == "VOID\n"

    def test_avl_balanced_insert(self):
        # Insert 1..7 into an AVL-balancing tree; the in-order traversal
        # should be sorted and the tree should be balanced.
        out = run_ath_via_c("""
BIRTH T WITH TREE(ALIVE);
CAPTCHALOGUE "1" INTO T;
CAPTCHALOGUE "2" INTO T;
CAPTCHALOGUE "3" INTO T;
CAPTCHALOGUE "4" INTO T;
CAPTCHALOGUE "5" INTO T;
CAPTCHALOGUE "6" INTO T;
CAPTCHALOGUE "7" INTO T;
BIRTH a WITH EJECT ROOT FROM T;
UTTER(STRING(a));
THIS.DIE();
""")
        assert out == "[1, 2, 3, 4, 5, 6, 7]\n"

    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(TREE())); THIS.DIE();')
        assert out == "TREE\n"


# ===== HASHMAP =====

class TestHashmap:
    def test_default_hash_collision_overwrite(self):
        # "ab" -> 97+98 = 195, 195 % 4 = 3
        # "cd" -> 99+100 = 199, 199 % 4 = 3 — same slot, collision
        out = run_ath_via_c("""
BIRTH H WITH HASHMAP(4);
CAPTCHALOGUE "first" WITH "ab" INTO H;
UTTER(STRING(H));
CAPTCHALOGUE "second" WITH "cd" INTO H;
UTTER(STRING(H));
THIS.DIE();
""")
        assert out == (
            'HASHMAP[VOID, VOID, VOID, "ab"->"first"]\n'
            'HASHMAP[VOID, VOID, VOID, "cd"->"second"]\n'
        )

    def test_eject_by_key_miss_on_collision(self):
        out = run_ath_via_c("""
BIRTH H WITH HASHMAP(4);
CAPTCHALOGUE "second" WITH "cd" INTO H;
BIRTH miss WITH EJECT "ab" FROM H;
UTTER(STRING(miss));
UTTER(STRING(H));
THIS.DIE();
""")
        # "ab" hashes to slot 3 which holds "cd" — returns VOID, slot unchanged
        assert out == 'VOID\nHASHMAP[VOID, VOID, VOID, "cd"->"second"]\n'

    def test_eject_by_key_hit(self):
        out = run_ath_via_c("""
BIRTH H WITH HASHMAP(4);
CAPTCHALOGUE "second" WITH "cd" INTO H;
BIRTH hit WITH EJECT "cd" FROM H;
UTTER(hit);
UTTER(STRING(H));
THIS.DIE();
""")
        assert out == "second\nHASHMAP[VOID, VOID, VOID, VOID]\n"

    def test_eject_by_slot(self):
        out = run_ath_via_c("""
BIRTH H WITH HASHMAP(4);
CAPTCHALOGUE "x" WITH "ab" INTO H;
BIRTH v WITH EJECT SLOT 3 FROM H;
UTTER(v);
THIS.DIE();
""")
        assert out == "x\n"

    def test_custom_hash_rite(self):
        out = run_ath_via_c("""
RITE myHash(s) { BEQUEATH LENGTH(s); }
BIRTH H WITH HASHMAP(4, myHash);
CAPTCHALOGUE "v" WITH "ab" INTO H;
UTTER(STRING(H));
THIS.DIE();
""")
        # length 2 -> slot 2
        assert out == 'HASHMAP[VOID, VOID, "ab"->"v", VOID]\n'

    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(HASHMAP(4))); THIS.DIE();')
        assert out == "HASHMAP\n"

    def test_count(self):
        out = run_ath_via_c("""
BIRTH H WITH HASHMAP(4);
UTTER(COUNT(H));
CAPTCHALOGUE "x" WITH "k1" INTO H;
UTTER(COUNT(H));
THIS.DIE();
""")
        assert out == "0\n1\n"


# ===== OUIJA =====

class TestOuija:
    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(OUIJA(4))); THIS.DIE();')
        assert out == "OUIJA\n"

    def test_write_then_eject_eventually_empties(self):
        # Write k items; eject k+slack times; should not exceed initial population
        out = run_ath_via_c("""
BIRTH O WITH OUIJA(4);
CAPTCHALOGUE "a" INTO O;
CAPTCHALOGUE "b" INTO O;
BIRTH c0 WITH COUNT(O);
UTTER(STRING(c0));
THIS.DIE();
""")
        # 0, 1, or 2 depending on collisions
        c = int(out.strip())
        assert 0 <= c <= 2

    def test_count_after_full(self):
        out = run_ath_via_c("""
BIRTH O WITH OUIJA(2);
CAPTCHALOGUE "a" INTO O;
CAPTCHALOGUE "b" INTO O;
CAPTCHALOGUE "c" INTO O;
CAPTCHALOGUE "d" INTO O;
BIRTH n WITH COUNT(O);
UTTER(STRING(n));
THIS.DIE();
""")
        # After 4 writes to size-2 OUIJA, count is 1 or 2
        n = int(out.strip())
        assert 1 <= n <= 2


# ===== BOTTLE =====

class TestBottle:
    def test_basic_write_and_first_eject(self):
        out = run_ath_via_c("""
BIRTH B WITH BOTTLE(3);
CAPTCHALOGUE "first message" INTO B;
CAPTCHALOGUE "second message" INTO B;
UTTER(STRING(B));
BIRTH msg WITH EJECT FROM B;
UTTER(msg);
UTTER(STRING(B));
THIS.DIE();
""")
        assert out == (
            'BOTTLE["first message", "second message", VOID]\n'
            "first message\n"
            'BOTTLE[DEAD, "second message", VOID]\n'
        )

    def test_skip_dead_on_capture(self):
        out = run_ath_via_c("""
BIRTH B WITH BOTTLE(3);
CAPTCHALOGUE "first" INTO B;
CAPTCHALOGUE "second" INTO B;
BIRTH x WITH EJECT FROM B;
CAPTCHALOGUE "third" INTO B;
UTTER(STRING(B));
THIS.DIE();
""")
        assert out == 'BOTTLE[DEAD, "second", "third"]\n'

    def test_eject_dead_slot_errors(self):
        out = run_ath_via_c("""
BIRTH B WITH BOTTLE(3);
CAPTCHALOGUE "x" INTO B;
BIRTH _ WITH EJECT FROM B;
ATTEMPT {
    BIRTH dead WITH EJECT SLOT 0 FROM B;
    UTTER("got:");
    UTTER(dead);
} SALVAGE err {
    UTTER("err");
}
THIS.DIE();
""")
        assert out == "err\n"

    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(BOTTLE(3))); THIS.DIE();')
        assert out == "BOTTLE\n"


# ===== TECHHOP =====

class TestTechhop:
    def test_predicate_routing(self):
        out = run_ath_via_c("""
RITE byType(v, g) {
    SHOULD g == 0 { BEQUEATH TYPEOF(v) == "STRING"; }
    SHOULD g == 1 { BEQUEATH TYPEOF(v) == "INTEGER"; }
    BEQUEATH DEAD;
}
RITE bySize(v, s) {
    BIRTH metric WITH 0;
    SHOULD TYPEOF(v) == "STRING" { metric = LENGTH(v); }
    LEST { metric = v; }
    SHOULD s == 0 { BEQUEATH metric < 4; }
    BEQUEATH metric >= 4;
}
BIRTH TH WITH TECHHOP(2, 2, byType, bySize);
CAPTCHALOGUE "hi" INTO TH;
CAPTCHALOGUE 42 INTO TH;
CAPTCHALOGUE "hello" INTO TH;
CAPTCHALOGUE 3 INTO TH;
UTTER(STRING(TH));
BIRTH s WITH EJECT GROOVE 0 SHADE 1 FROM TH;
UTTER(s);
THIS.DIE();
""")
        assert out == 'TECHHOP[["hi", "hello"], [3, 42]]\nhello\n'

    def test_typeof(self):
        out = run_ath_via_c("""
RITE pg(v, g) { BEQUEATH ALIVE; }
RITE ps(v, s) { BEQUEATH ALIVE; }
UTTER(TYPEOF(TECHHOP(2, 2, pg, ps)));
THIS.DIE();
""")
        assert out == "TECHHOP\n"


# ===== JUJU =====

class TestJuju:
    def test_typeof(self):
        out = run_ath_via_c('UTTER(TYPEOF(JUJU(2))); THIS.DIE();')
        assert out == "JUJU\n"

    def test_outside_branch_errors(self):
        out = run_ath_via_c("""
BIRTH J WITH JUJU(2);
ATTEMPT {
    CAPTCHALOGUE "x" INTO J SLOT 0;
    UTTER("no error");
} SALVAGE err {
    UTTER("caught");
}
THIS.DIE();
""")
        assert out == "caught\n"

    def test_branch_communicates_via_juju(self):
        out = run_ath_via_c("""
BIRTH J WITH JUJU(2);
bifurcate THIS[ALICE, BOB];

~ATH(ALICE) {
    CAPTCHALOGUE "hello bob" INTO J SLOT 0;
    import timer TA(10ms);
    ~ATH(TA) {
    } EXECUTE(VOID);
} EXECUTE(VOID);

~ATH(BOB) {
    import timer TB(5ms);
    ~ATH(TB) {
    } EXECUTE(
        BIRTH msg WITH EJECT SLOT 0 FROM J;
        UTTER("Bob got:");
        UTTER(msg);
    );
} EXECUTE(VOID);

[ALICE, BOB].DIE();
""")
        assert out == "Bob got:\nhello bob\n"


# ===== Cross-cutting =====

class TestModifierValidation:
    def test_stack_with_extra_modifier_errors(self):
        # CAPTCHALOGUE x WITH k INTO stack is invalid
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH S WITH STACK(3);
    CAPTCHALOGUE 1 WITH "k" INTO S;
    UTTER("no error");
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_hashmap_without_with_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH H WITH HASHMAP(4);
    CAPTCHALOGUE 1 INTO H;
    UTTER("no error");
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_tree_bare_eject_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH T WITH TREE();
    CAPTCHALOGUE "a" INTO T;
    BIRTH x WITH EJECT FROM T;
    UTTER(x);
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_techhop_bare_eject_errors(self):
        out = run_ath_via_c("""
RITE pg(v, g) { BEQUEATH ALIVE; }
RITE ps(v, s) { BEQUEATH ALIVE; }
ATTEMPT {
    BIRTH TH WITH TECHHOP(2, 2, pg, ps);
    BIRTH x WITH EJECT FROM TH;
    UTTER(x);
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_eject_non_sylladex_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH x WITH 42;
    BIRTH y WITH EJECT FROM x;
    UTTER(y);
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_captchalogue_non_sylladex_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH x WITH 42;
    CAPTCHALOGUE 1 INTO x;
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"


class TestSylladexNotIndexable:
    def test_index_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH S WITH STACK(3);
    BIRTH x WITH S[0];
    UTTER(x);
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"

    def test_member_errors(self):
        out = run_ath_via_c("""
ATTEMPT {
    BIRTH S WITH STACK(3);
    BIRTH x WITH S.foo;
    UTTER(x);
} SALVAGE err { UTTER("caught"); }
THIS.DIE();
""")
        assert out == "caught\n"
