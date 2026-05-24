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

# run.sh -- entry point for the !~ATH-based test harness.
#
# The harness itself (harness.~ATH) is built with athtoc-stable, a frozen
# known-good copy of the transpiler. This is deliberate: breaking the
# mainline athtoc-bin must not stop you from running the tests that would
# tell you it is broken. The individual test cases, on the other hand, are
# transpiled by the mainline athtoc-bin (see runcase.sh) -- that is the
# transpiler under test.

set -e
cd "$(dirname "$0")"            # transpiler-to-c/tests/

RUNTIME="../runtime"

# The harness and every case link against the runtime static library
# (../libath_runtime.a, produced by `make lib`).
if [ ! -f ../libath_runtime.a ]; then
    echo "Building runtime library..."
    ( cd .. && make lib )
fi

if [ ! -x ./athtoc-stable ]; then
    echo "ERROR: ./athtoc-stable not found." >&2
    echo "       It is the frozen transpiler used to build the harness." >&2
    exit 1
fi

mkdir -p work

echo "Building harness with athtoc-stable..."
./athtoc-stable < harness.~ATH > work/harness.c
gcc -std=c89 -pedantic -Wno-unused-variable -Wno-declaration-after-statement \
    work/harness.c -I"$RUNTIME" -L.. -lath_runtime -ldl -lffi -o work/harness

# Hand off to the harness. Its exit status (non-zero on any test failure)
# becomes this script's exit status.
exec ./work/harness
