From `transpiler-to-c/`:

```bash
make test
```

or directly:

```bash
tests/run.sh
```

The harness prints a `pass`/`FAIL` line per case and a summary, and exits
non-zero if any case fails.

## How it works

```
run.sh        builds harness.~ATH with athtoc-stable, then runs it
  harness     reads manifest.txt; per case, spawns runcase.sh as a process
              entity, waits for it to die, compares output, tallies results
    runcase.sh   transpiles cases/<name>.~ATH with mainline athtoc-bin,
                 compiles it, runs it, writes work/<name>.actual
```

### Two transpilers

- `athtoc-stable`, a known-good copy of the transpiler, updated only
  ocasionally. The *harness* is built with this. Breaking the mainline 
  `../athtoc-bin` must not stop you from running tests, for obvious
  reasons.
-  `../athtoc-bin`, the thing under test. `runcase.sh` transpiles 
  every test case with this one.

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
3. If it reads stdin, add `cases/<name>.stdin` and set the stdin flag.
4. If it does `import watcher M("...~ATH")`, drop the module file in `cases/`
   and reference it by basename (the case is transpiled with cwd = `cases/`).
5. Append a line to `manifest.txt`. Use `sorted` mode for any `bifurcate` case.
