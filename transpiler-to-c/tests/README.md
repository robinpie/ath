<!-- SPDX-License-Identifier: GPL-2.0-only -->
From `transpiler-to-c/`:

```bash
make test          # every target: Linux (native), Windows (wine), WASM (wasmtime)
make test-linux    # Linux only
make test-win64    # Windows only, via wine + mingw (requires wine + mingw-w64-gcc)
make test-wasm     # WebAssembly/WASI only, via wasmtime + wasi-sdk clang
```

The harness prints a `pass`/`FAIL`/`skip` line per case and a summary, and exits
non-zero if any case fails. Incremental: only rebuilds artifacts that are
out of date.

Each run targets one platform. Cases are filtered to the current target by their
manifest platform field (see *Manifest format* below), so the summary also
reports a `skipped` count for cases that don't apply to that target.

## How it works

```
make test     delegates to tests/Makefile, which builds libruncase.so and
              harness (via athtoc-stable), then runs the harness binary
  harness     reads manifest.txt; per case, calls libruncase.run_case() via
              FFI session; compares work/<name>.actual vs cases/<name>.expected
    libruncase.so   transpiles cases/<name>.~ATH with mainline athtoc-bin,
                    compiles it, runs it, writes work/<name>.actual
```

### Two transpilers

- `athtoc-stable`, a known-good copy of the transpiler, updated only
  occasionally. The *harness* is built with this. Breaking the mainline
  `../athtoc-bin` must not stop you from running tests, for obvious
  reasons.
- `../athtoc-bin`, the thing under test. `libruncase.so` transpiles
  every test case with this one.

### FFI-based runner

`libruncase.c` is a small C shared library that does the
transpile → compile → run pipeline for each case using `fork`/`exec`/`waitpid`
with proper I/O redirection. The harness opens it once as an `import session`
entity and calls `RC.run_case(name, athtoc)` synchronously for each case.
This replaces the old `sh runcase.sh` subprocess per case.

On any toolchain failure (transpile or compile) the actual-output file is
filled with a sentinel (`TRANSPILE_FAIL` / `COMPILE_FAIL`) so the harness
reports a normal FAIL rather than hanging.

To override the transpiler path, set the `ATHTOC` environment variable before
running the harness (falls back to `../athtoc-bin`).

### Platform scope

The harness binary, `libruncase.so`, and `athtoc-stable` are always Linux ELFs;
the runner uses `fork`/`exec`/`waitpid` internally. What changes per target is
the per-case toolchain, selected by the `ATH_TARGET` env var that the `make`
targets set (and which `libruncase` reads and reports back to the harness via
`ath_target()`, since !~ATH has no `getenv`):

- native (`make test-linux`): transpiles each case with `../athtoc-bin`,
  compiles with `gcc`, runs the ELF directly.
- win64 (`make test-win64`): transpiles with `../athtoc-bin-win64.exe` under
  wine, cross-compiles with `x86_64-w64-mingw32-gcc` against the vendored
  libffi and `libath_runtime_win64.a` (built by `make lib-win64`), and run the
  `.exe` under wine. Requires `wine` and `mingw-w64-gcc`. The Windows runtime
  writes stdout in text mode (CRLF), so `libruncase` normalises the captured
  output to LF before comparing.
- wasm (`make test-wasm`): transpiles with `../athtoc.wasm` under `wasmtime`,
  compiles with the wasi-sdk clang against `libath_runtime_wasm.a` (built by
  `make lib-wasm`), and runs the `.wasm` under `wasmtime`. Both wasmtime
  invocations get a `--dir` grant on `cases/` (and on `work/` for the run step)
  so `SCRY`/`INSCRIBE` and module-import watchers can reach the filesystem.
  Tool paths come from the env (`WASMTIME`, `WASI_CLANG`, `WASI_SYSROOT`,
  `WASM_STACK`), defaulted by the parent Makefile.

`make test-win64` and `make test-wasm` are much slower than `test-linux` (every
case spawns the emulator/runtime twice plus a cross-compile).

## Manifest format

Each line of `manifest.txt` is `name|mode|stdin` and optionally `platforms`:

|  |  |
| --- | --- |
| `name` | The case basename under `./cases/`. |
| `mode` | `exact` (stdout must match `<name>.expected` byte-for-byte) or `sorted` (compare line-sorted; used for bifurcated programs whose branches interleave nondeterministically). |
| `stdin` | `1` if a `<name>.stdin` file should be fed to the program, else `0`. |
| `platforms` | Optional comma-separated list of targets the case applies to (`linux`, `win64`, `wasm`). Omitted/empty means **all** targets. A case not applicable to the current target is reported as `skip`. |

Use `linux` for cases that cannot run under wine (e.g. FFI sessions that
`dlopen` a Linux `.so`, processes that spawn unix commands) and for cases whose
output legitimately differs on Windows' 32-bit `long`. For the latter, add a
parallel `win64`-tagged case asserting the Windows-correct output (see
`*_large_integer_fits_in_long` / `*_large_integer_wraps_on_win64`).

WASM has no FFI/sessions (those are already `linux`-only), and no `process` or
`connection` entities; those last cases are tagged `linux,win64` so they skip on
wasm. Everything else -- pure language, sylladices, buffers, timers, and
module-import watchers -- runs on wasm.

Blank lines and `#` comment lines in `manifest.txt` (e.g. the SPDX header) are
skipped by the harness.


## Adding a test

1. Write `cases/<name>.~ATH`.
2. Put its expected stdout into `cases/<name>.expected`.
3. If it reads stdin, add `cases/<name>.stdin`
4. If it does `import watcher M("...~ATH")`, drop the module file in `cases/`
   and reference it by basename (the case is transpiled with cwd = `cases/`).
5. Append a line to `manifest.txt`. Use `sorted` mode for any `bifurcate` case.
   Add a `platforms` field if the case is platform-specific (e.g. `|linux` for an
   FFI/`.so` or unix-process case).
