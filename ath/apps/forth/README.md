This is a Forth 2012 CORE word-set interpreter written in !~ATH. It passes the Gerry Jackson Forth 2012 test suite's CORE tests.

## Building

```bash
tpc=../../transpiler-to-c
$tpc/athtoc-bin < build.~ATH > build.c
gcc -std=c89 build.c $(ls $tpc/runtime/*.c | grep -v test_runtime.c) \
    -I$tpc/runtime -ldl -lffi -o build

./build                                    # concatenate -> transpile -> compile
echo ': sq DUP * ; 7 sq .' | ./forth       # -> 49
```

## Running

The interpreter reads a Forth program from stdin and writes output to stdout.

```bash
./forth < program.fth
cat tester.fr core.fr | ./forth
```

## Testing

`run-tests.~ATH` is the CORE test runner.

Build and run it from this directory (`./forth` must already be built):

```bash
tpc=../../transpiler-to-c
$tpc/athtoc-bin < run-tests.~ATH > run-tests.c
gcc -std=c89 run-tests.c $(ls $tpc/runtime/*.c | grep -v test_runtime.c) \
    -I$tpc/runtime -ldl -lffi -o run-tests
./run-tests
```

(The suite path and the `./forth` location are constants at the top of `run-tests.~ATH`.)

## Design

- 32-bit cells. A double-cell (64 bits) then fits one native !~ATH `long`, so `M* UM* UM/MOD SM/REM FM/MOD */ */MOD` need no hand-rolled bignum.
- One byte-addressed data space (`G_MEM`, a `BUFFER`); Forth addresses are
  integer offsets. Reserved low regions: TIB, PAD, S"-scratch, WORD buffer, system variables (`BASE STATE >IN`), the HOLD buffer.
- Threaded-code VM. Colon words compile to arrays of instruction cells (`call`/`lit`/`branch`/`0branch`/`do`/`loop`/`slit`/`does`/...). The VM keeps its own explicit call stack so deep Forth nesting and long loops never grow the C stack. The scheduler steps one cell per tick inside a batched-recursion trampoline (one `~ATH` timer wait per batch).
- Dictionary = an array of entry records; xt = 1-based index; a name->xt map backs `FIND`/`'` (case-insensitive, newest-first, honoring `IMMEDIATE`).
- DOES> stores `{code, start}` into the defining word's code so branch targets stay valid; **EVALUATE** nests the input source and exposes the original address via `SOURCE`.

### Source layout

| file | contents |
|------|----------|
| `00_util.~ATH`     | 32-bit cell math, output sink (line-buffered) |
| `10_memory.~ATH`   | data-space BUFFER, cell/byte access, HERE/ALLOT, regions |
| `20_stack.~ATH`    | data & return stacks |
| `30_dict.~ATH`     | dictionary + name map |
| `40_prims.~ATH`    | primitive words + registration |
| `50_compiler.~ATH` | VM + colon compiler |
| `52_control.~ATH`  | control-flow words + loop/string/DOES> runtime |
| `56_numbers.~ATH`  | double-cell helpers, pictured output, >NUMBER, EVALUATE |
| `60_interp.~ATH`   | input source, tokenizer, number parsing, scheduler |
| `70_main.~ATH`     | entry point |

## Scope & limitations

- Targets the CORE word set. Core-Extension and optional word sets may be implemented later.
- `ACCEPT` is interactive; in batch mode it accepts 0 characters (the suite's ACCEPT test is a visual one and passes trivially).
- Cells are 32-bit (the test suite computes cell width at runtime, so this works), and division is floored (the suite adapts via `IFFLOORED`).
