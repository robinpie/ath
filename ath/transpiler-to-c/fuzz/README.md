<!-- SPDX-License-Identifier: GPL-2.0-only -->
# Fuzzing and sanitizers

From `ath/transpiler-to-c/`. Requires clang with libFuzzer (any recent LLVM).

## Sanitized test suite

```bash
make test-san      # native suite, cases built with ASan+UBSan against libath_runtime_san.a
```

`test-san` lists cases with sanitizer reports afterwards (also saved to `tests/work/san-hits.txt`).

## libFuzzer harnesses

| harness | target | build / run |
|---------|--------|-------------|
| `fuzz_cake.c` | `.^CAKE` parser, layout, codes, BAKE/SPRINKLE/SCOOP/PLATE/UNPLATE | `make fuzz-cake`, `make fuzz-cake-run` |
| AFL++ on `athtoc` | the transpiler itself (lexer/parser/CPS/codegen written in !~ATH, running on the runtime), fed through stdin | `make afl-athtoc`, `make afl-athtoc-run`, `make afl-check` |
| `fuzz_ops.c` | runtime value API: built-in rites, operators, indexing, sylladices (with misbehaving hash/predicate rites), BANISH, and the !^CAKE builtins with arbitrary recipes, paths and buffer lengths | `make fuzz-ops`, `make fuzz-ops-run` |

```bash
make fuzz-cake-run                                   # until the first crash
make fuzz-cake-run FUZZ_ARGS="-max_total_time=600"   # time-boxed
make fuzz-cake-run FUZZ_ARGS="-jobs=4 -workers=4"    # parallel
```

Everything generated lives in `fuzz/work/` (gitignored). Replay a crash with `ASAN_OPTIONS=detect_leaks=0 ./fuzz/work/fuzz_cake <crash-file>`.

`fuzz_cake` aborts on any layout invariant violation (misaligned or overlapping fields, `size % align != 0`, unsorted union arms, a malformed captchalogue code), on a RAW error from a path it derived from the recipe itself, and on any SPRINKLE/SCOOP round-trip mismatch.

`fuzz_ops` reads its input as a bytecode program over 16 value registers and calls the runtime exactly as generated C does (same +1 ownership convention, every operation inside an error frame, the value sink flushed after each step). It aborts when a value's structure is broken (refcount <= 0, length past capacity, a string without its NUL, a buffer whose bytes and length disagree), when the error-frame stack isn't unwound, and when a round-trip identity fails (`CODE(CHAR(cp))`, `BUFFER_TO_STRING(STRING_TO_BUFFER(s))`, `JOIN(SPLIT(s, d), d)`). It never builds reference cycles (they are legal but stringify forever) and it drops values past a size budget, because a program that keeps stringifying a map into itself grows exponentially without that being a runtime bug.

AFL++ (`pacman -S afl++`): `make afl-athtoc-run` builds an ASan+UBSan main instance and uninstrumented-sanitizer secondaries (`AFL_SECONDARIES`, default 5) from the current transpiler source, seeds them from the test cases and apps (comments stripped, `afl-cmin`-minimized) with `fuzz/athtoc.dict`, and runs for `AFL_TIME` seconds (default 3600). AFL only sees crashes and hangs, so `make afl-check` (`fuzz/afl_check_outputs.sh`) adds the second oracle: every queued input athtoc *accepts* must produce C that passes `gcc -std=c89 -fsyntax-only` with implicit declarations as errors; failures are copied to `fuzz/work/afl/out/uncompilable/`.

`make clean` removes only the fuzz binaries; `make fuzz-clean` removes corpora, crashes and AFL state too.

Leak detection is off. `ath_cake_load_source` now frees its token array and parse tree when a schema error is raised, but a few partly-built nodes (alchemy nodes, the recipe being parsed) still leak on error paths. That's a few hundred bytes per rejected input, compared with several KB before, and is tracked in the leak-audit item.

## Findings so far

- `ck_build_struct`: heap use-after-free formatting the duplicate-ingredient error (regression: `cake__TestErrors__test_duplicate_ingredient`).
- `ck_resolve_path`: path segments were truncated to 127 bytes, so a longer ingredient name was unreachable, or resolved to a *different* field sharing the prefix, silently reading and writing the wrong bytes (regression: `cake__TestField__test_long_ingredient_names`).
- `ck_resolve_recipe`: the PUNCHED/STALE message was `sprintf`-ed into a 160-byte stack buffer, so a long recipe name overflowed the stack. Found by reading while chasing fuzzer OOMs (regression:
  `cake__TestErrors__test_punched_long_name`).
- `fuzz_ops`: `ath_stringify` and `INT()` converted NaN/infinite/out-of-range FLOATs to `long` (undefined behaviour); stringify now range-checks first, and `INT()` raises `INT: FLOAT is out of INTEGER range`.
- Found by reading while writing `fuzz_ops` (all confirmed with ASan/UBSan against the old runtime; regressions `regression__overflow_ub_in_builtins`, `regression__int_index_no_truncation`):
  - HASHMAP with a hash rite returning LONG_MIN: `-h` overflowed and the negative slot index wrote before the slot array (heap-buffer-overflow).
  - `RANDOM_INT` over a span of 2^32 truncated `(int)range` to 0 and divided by zero (SIGFPE); the full range overflowed `max - min + 1`.
  - `RECKON(buf, off, len)` computed `off + len` before bounds-checking it (signed overflow, then an out-of-bounds read).
  - `TECHHOP(65536, 65536, ...)` overflowed `grooves * shades` and under-allocated (heap-buffer-overflow on CAPTCHALOGUE). Slot arrays are now capped so their byte size can't overflow `size_t` on 32-bit hosts either.
  - `(int)` casts of 64-bit INTEGERs silently wrapped: `arr[4294967296]` read `arr[0]`, `SLICE`/`SUBSTRING` bounds wrapped, `STACK(4294967298)` was a `STACK(2)`, `BUFFER(4294967296)` was empty. Indices and bounds now saturate (so they fail or clamp correctly), and sizes above INT_MAX raise `<KIND> size too large`.
  - SCOOP of a whole union arm allocated its result before the bounds check that can raise (leak).
- `fuzz_ops` round-trip oracle `JOIN(SPLIT(s, d), d) == s`: strings are byte arrays and may hold NULs, but `JOIN`, `STRING(s)`, `"x" + s` and `INSCRIBE` went through C strings and silently truncated at the first NUL, and `PARSE_INT`/`PARSE_FLOAT` accepted `"12<NUL>junk"` (regression: `regression__string_builtins_keep_nul_bytes`). Map keys, and strings nested inside a stringified array/map/sylladex, are still C strings (a limitation of the `char *` stringify API).
- While fixing JOIN: string lengths summed in `int`. `JOIN([s, s, s], "")` with a 1 GiB `s` wrapped the total and segfaulted in an under-sized buffer (confirmed natively); the same pattern was in `stringify_array`/`stringify_map`, `REPLACE`'s doubling buffer (unchecked `realloc`), string concatenation, and the sylladex `str_append` (where `off + n + 1` wrapping negative skipped the grow and wrote past the buffer). All now use `size_t` and raise a catchable `... too large` error when a result can't be an AthString. No suite regression for these: they need several GB of RAM.
- Leak audit (`make test-san SAN_ASAN_OPTIONS=allow_user_segv_handler=1:detect_leaks=1`): generated code never freed `AthErrorFrame.error_msg` after SALVAGE copied it into the error variable, so every caught error leaked its message (fixed in codegen, all four SALVAGE/module-import sites); `.^CAKE` `MEASURE` declarations leaked their name on every load (fixed). What remains is the builtins scope/rite reference cycle held until exit, and partial nodes on `.^CAKE` error paths.
- AFL++ on athtoc, first 30-minute run (6 instances, ~3M execs): no crashes; the 34 "hangs" all terminate (72 ms to 7 s natively; large invalid inputs over AFL's timeout). The compile oracle flagged 42 accepted inputs whose C did not compile, which reduced to four transpiler bugs:
  - float literals were emitted via `STRING(v)` (6 significant digits), so `3.14159265358979` compiled as `3.14159`, and an out-of-range literal as the token `inf` -- codegen now emits the literal's source digits (regression: `regression__float_literal_precision`);
  - a definition generated while another function was still being emitted (a branch-mode `~ATH` inside a wait-mode EXECUTE; an async rite defined in an EXECUTE that defines a rite) was spliced into the middle of it -- whole definitions are now generated into their own buffer and hoisted (regression: `regression__nested_definitions_hoisted`);
  - a module rite and a program rite with the same name were both `_rite_<name>` -- module rites now get a per-module C prefix and bind calls against the module's own rites (regression: `regression__module_rite_name_matches_program_rite`);
  - two RITEs with the same name in one program (redefinition, or an inner rite shadowing an outer one) produced a C redefinition, a nested function, or a call statically bound to the wrong rite (one reproducer looped forever). Proper lexical resolution of rite names is a bigger change; until then a repeated rite name is a transpile error naming both lines (regression: `regression__duplicate_rite_name_rejected`). No existing test or app reuses a rite name.
- Probed by hand: nesting is not a crash, but it is super-linear in codegen (1000 nested `SHOULD` blocks take ~32 s, 2000 take ~250 s; a 10,000-term `1 + 1 + ...` takes ~10 s), since generated text is re-concatenated at every level.
- Parser leaked its whole state on every schema error, so fuzzing sessions hit libFuzzer's 2 GB RSS limit after ~350k inputs. Mostly fixed (see above).
