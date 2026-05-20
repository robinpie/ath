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

"""Shared fixtures for !~ATH to C89 transpiler tests."""

import subprocess
import os
import tempfile

TESTS_DIR  = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR   = os.path.dirname(TESTS_DIR)
ATHTOC     = os.environ.get("ATHTOC", os.path.join(ROOT_DIR, "athtoc"))
RUNTIME_DIR = os.path.join(ROOT_DIR, "runtime")

RUNTIME_SRCS = [
    os.path.join(RUNTIME_DIR, f)
    for f in os.listdir(RUNTIME_DIR)
    if f.endswith(".c") and f != "test_runtime.c"
]


def run_ath_via_c(source: str, stdin_input: str = "", timeout: float = 15.0) -> str:
    """Transpile !~ATH source to C89, compile with gcc, run, return stdout."""
    with tempfile.TemporaryDirectory() as tmp:
        c_path  = os.path.join(tmp, "prog.c")
        exe     = os.path.join(tmp, "prog")

        r = subprocess.run(
            [ATHTOC],
            input=source,
            capture_output=True,
            text=True,
            timeout=10.0,
        )
        if r.returncode != 0:
            raise AssertionError(f"Transpile failed:\n{r.stderr}")

        with open(c_path, "w") as f:
            f.write(r.stdout)

        r = subprocess.run(
            ["gcc", "-std=c89", "-pedantic", "-Wno-unused-variable",
             c_path, *RUNTIME_SRCS, f"-I{RUNTIME_DIR}", "-o", exe],
            capture_output=True,
            text=True,
            timeout=30.0,
        )
        if r.returncode != 0:
            raise AssertionError(f"Compile failed:\n{r.stderr}")

        r = subprocess.run(
            [exe],
            input=stdin_input,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return r.stdout
