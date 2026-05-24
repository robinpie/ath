#!/bin/sh
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

# runcase.sh <casename>
#
# Per-case executor for the !~ATH test harness. Transpiles cases/<name>.~ATH
# with the *mainline* transpiler (the one under test), compiles the result,
# runs it, and writes the program's stdout to work/<name>.actual.
#
# Must be invoked with cwd = transpiler-to-c/tests/. The harness (harness.~ATH)
# spawns this script as a process entity, one per test case.
#
# On any toolchain failure the actual-output file is filled with a sentinel
# (TRANSPILE_FAIL / COMPILE_FAIL) so the harness reports a normal FAIL rather
# than hanging. Diagnostics land in work/<name>.err.

NAME="$1"
ATHTOC="${ATHTOC:-../athtoc-bin}"
RUNTIME="../runtime"

mkdir -p work

# Transpile. cwd = cases/ so that `import watcher` module fixtures resolve.
( cd cases && "../$ATHTOC" < "$NAME.~ATH" ) > "work/$NAME.c" 2> "work/$NAME.err"
if [ $? -ne 0 ]; then
    printf 'TRANSPILE_FAIL\n' > "work/$NAME.actual"
    exit 0
fi

gcc -std=c89 -pedantic -Wno-unused-variable "work/$NAME.c" \
    -I"$RUNTIME" -L.. -lath_runtime -ldl -lffi -o "work/$NAME.bin" \
    2>> "work/$NAME.err"
if [ $? -ne 0 ]; then
    printf 'COMPILE_FAIL\n' > "work/$NAME.actual"
    exit 0
fi

# Run. cwd = cases/ so module fixtures resolve at runtime as well.
if [ -f "cases/$NAME.stdin" ]; then
    ( cd cases && "../work/$NAME.bin" < "$NAME.stdin" ) \
        > "work/$NAME.actual" 2>> "work/$NAME.err"
else
    ( cd cases && "../work/$NAME.bin" < /dev/null ) \
        > "work/$NAME.actual" 2>> "work/$NAME.err"
fi

exit 0
