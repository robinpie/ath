#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# afl_check_outputs.sh -- the "accepted input must yield compilable C" oracle
# for AFL++ runs against athtoc (see fuzz/README.md).
#
# AFL only notices crashes and hangs. This sweeps an AFL output directory and,
# for every queued input the transpiler ACCEPTS (exit 0), compiles the emitted C
# with `cc -std=c89 -fsyntax-only` (implicit declarations promoted to errors, since
# a mis-emitted runtime call would otherwise only fail at link time). An input athtoc accepts but whose output does
# not compile is a transpiler bug; it is copied to <afl-out>/uncompilable/ along
# with the generated C and the compiler errors.
#
# usage (from ath/transpiler-to-c): fuzz/afl_check_outputs.sh fuzz/work/afl/out [athtoc-binary]
set -u
OUT=${1:?usage: afl_check_outputs.sh <afl-out-dir> [athtoc]}
ATHTOC=${2:-./athtoc-bin}
CC=${CC:-gcc}
BAD="$OUT/uncompilable"
mkdir -p "$BAD"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

checked=0 accepted=0 failed=0
for f in "$OUT"/*/queue/id:* "$OUT"/*/crashes/id:*; do
    [ -f "$f" ] || continue
    checked=$((checked + 1))
    if ! timeout 10 "$ATHTOC" < "$f" > "$tmp/out.c" 2>/dev/null; then
        continue   # rejected (or crashed: AFL already records those)
    fi
    accepted=$((accepted + 1))
    if ! "$CC" -std=c89 -fsyntax-only -Werror=implicit-function-declaration -Werror=implicit-int -Iruntime "$tmp/out.c" > "$tmp/err.txt" 2>&1; then
        failed=$((failed + 1))
        dir=$(dirname "$f")
        name="$(basename "$(dirname "$dir")")-$(basename "$dir")-$(basename "$f" | cut -d, -f1 | tr ':' '-')"
        cp "$f" "$BAD/$name.~ATH"
        cp "$tmp/out.c" "$BAD/$name.c"
        cp "$tmp/err.txt" "$BAD/$name.err"
    fi
done
echo "checked $checked inputs, athtoc accepted $accepted, output failed to compile for $failed (see $BAD)"
