From `transpiler-to-c/`:

```bash
make test
```

The harness prints a `pass`/`FAIL` line per case and a summary, and exits
non-zero if any case fails. Incremental: only rebuilds artifacts that are
out of date.

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

## Manifest format

Each line of `manifest.txt` is `name|mode|stdin`:

|  |  |
| --- | --- |
| `name` | The case basename under `./cases/`. |
| `mode` | `exact` (stdout must match `<name>.expected` byte-for-byte) or `sorted` (compare line-sorted; used for bifurcated programs whose branches interleave nondeterministically). |
| `stdin` | `1` if a `<name>.stdin` file should be fed to the program, else `0`. |


## Adding a test

1. Write `cases/<name>.~ATH`.
2. Put its expected stdout into `cases/<name>.expected`.
3. If it reads stdin, add `cases/<name>.stdin`
4. If it does `import watcher M("...~ATH")`, drop the module file in `cases/`
   and reference it by basename (the case is transpiled with cwd = `cases/`).
5. Append a line to `manifest.txt`. Use `sorted` mode for any `bifurcate` case.
