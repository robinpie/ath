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

"""E2E tests for !~ATH module imports.

Ported from python-interpreter/tests/test_module_import.py.
Module imports (import watcher Lib("path.~ATH")) work by reading the module
source at transpile time and inlining it into the generated C.
"""

import pytest
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from conftest import run_ath_via_c


def write_module(tmpdir, filename, content):
    path = os.path.join(tmpdir, filename)
    with open(path, "w") as f:
        f.write(content)
    return path


def test_basic_rite_import():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "mathlib.~ATH", """
            RITE add(a, b) {
                BEQUEATH a + b;
            }
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/mathlib.~ATH");
            BIRTH result WITH W.add(3, 4);
            UTTER(result);
            THIS.DIE();
        """)
        assert out.strip() == "7"


def test_variable_export():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "config.~ATH", """
            BIRTH greeting WITH "Hello from module";
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/config.~ATH");
            UTTER(W.greeting);
            THIS.DIE();
        """)
        assert out.strip() == "Hello from module"


def test_constant_export():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "constants.~ATH", """
            ENTOMB PI WITH 3;
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/constants.~ATH");
            UTTER(W.PI);
            THIS.DIE();
        """)
        assert out.strip() == "3"


def test_rite_with_closure():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "greeter.~ATH", """
            BIRTH prefix WITH "Hello, ";
            RITE greet(name) {
                BEQUEATH prefix + name;
            }
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/greeter.~ATH");
            BIRTH msg WITH W.greet("World");
            UTTER(msg);
            THIS.DIE();
        """)
        assert out.strip() == "Hello, World"


def test_multiple_rites():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "multi.~ATH", """
            RITE double(x) { BEQUEATH x * 2; }
            RITE triple(x) { BEQUEATH x * 3; }
            RITE negate(x) { BEQUEATH 0 - x; }
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher M("{tmpdir}/multi.~ATH");
            UTTER(M.double(5));
            UTTER(M.triple(5));
            UTTER(M.negate(5));
            THIS.DIE();
        """).strip().split('\n')
        assert out == ["10", "15", "-5"]


def test_reimport_module():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "counter.~ATH", """
            BIRTH val WITH 1;
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/counter.~ATH");
            UTTER(W.val);
            import watcher W("{tmpdir}/counter.~ATH");
            UTTER(W.val);
            THIS.DIE();
        """).strip().split('\n')
        assert out[0] == "1"
        assert out[1] == "1"


def test_module_with_timer():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "timermod.~ATH", """
            BIRTH result WITH 0;
            import timer T(1ms);
            ~ATH(T) { } EXECUTE(result = 42;);
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/timermod.~ATH");
            UTTER(W.result);
            THIS.DIE();
        """)
        assert out.strip() == "42"


def test_module_typeof():
    with tempfile.TemporaryDirectory() as tmpdir:
        write_module(tmpdir, "mod.~ATH", """
            THIS.DIE();
        """)
        out = run_ath_via_c(f"""
            import watcher W("{tmpdir}/mod.~ATH");
            UTTER(TYPEOF(W));
            THIS.DIE();
        """)
        assert out.strip() == "MODULE"
