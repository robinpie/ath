# Try the old Web interpreter at [https://robinpie.neocities.org/ath/](https://robinpie.neocities.org/ath/?code=Ly8gRml6ekJ1enogdXNpbmcgcmVjdXJzaW9uIHdpdGggdGltZXIgY2hhaW5zClJJVEUgZml6emJ1enoobiwgbWF4KSB7CiAgICBTSE9VTEQgbiA8PSBtYXggewogICAgICAgIFNIT1VMRCBuICUgMTUgPT0gMCB7CiAgICAgICAgICAgIFVUVEVSKCJGaXp6QnV6eiIpOwogICAgICAgIH0gTEVTVCBTSE9VTEQgbiAlIDMgPT0gMCB7CiAgICAgICAgICAgIFVUVEVSKCJGaXp6Iik7CiAgICAgICAgfSBMRVNUIFNIT1VMRCBuICUgNSA9PSAwIHsKICAgICAgICAgICAgVVRURVIoIkJ1enoiKTsKICAgICAgICB9IExFU1QgewogICAgICAgICAgICBVVFRFUihuKTsKICAgICAgICB9CgogICAgICAgIGltcG9ydCB0aW1lciBUKDFtcyk7CiAgICAgICAgfkFUSCghVCkgewogICAgICAgIH0gRVhFQ1VURShmaXp6YnV6eihuICsgMSwgbWF4KSk7CiAgICB9Cn0KCmZpenpidXp6KDEsIDUwKTsKVEhJUy5ESUUoKTs%3D)

---

**!~ATH** (pronounced "until death") is an esoteric programming language where all control flow is predicated on waiting for things to die. Inspired by the fictional ~ATH language from Homestuck. Everything is about death. Loops wait for entities to die. Computation happens in death callbacks. The language is deliberately inconvenient. 

## Implementation

The primary implementation of !~ATH is a transpiler to C89 that compiles via CPS transform. It is self-hosting, the transpiler itself is written in !~ATH.

There is also a deprecated Python interpreter for !~ATH 1.3, and a partial deprecated JavaScript interpreter for !~ATH 1.3.

The JavaScript interpreter powers [a Web playground for !~ATH 1.3](https://robinpie.neocities.org/ath/).

## Usage

All commands run from `transpiler-to-c/`:

```bash
# Transpile stdin → C89 stdout
./athtoc-bin < program.~ATH > program.c

# Compile and run
gcc -std=c89 program.c runtime/*.c -Iruntime -o program && ./program

# Or: build the runtime library and link against it
make lib
gcc -std=c89 program.c -L. -lath_runtime -Iruntime -o program && ./program
```

Works with any C89-compatible compiler (gcc, clang, etc.). The repo ships a pre-built x86_64 Linux `athtoc-bin` as the bootstrap seed.

```bash
# Rebuild athtoc-bin from source
make
```

If you're on a non-x86_64-Linux platform, you'll need a working `athtoc-bin` from another machine to bootstrap.

Current limitations of the implementation (may be worked around in the future):

- Integers are C `long` (64-bit on LP64 systems), not unbounded
- Strings are byte arrays; `LENGTH` and `SUBSTRING` operate on bytes, not Unicode codepoints
- `ProcessEntity`, `ConnectionEntity`, `WatcherEntity` require POSIX (Linux/macOS)
- Sync rites recurse on the C call stack; deep recursion will stack-overflow

**Test suite:**

```bash
cd transpiler-to-c
make test     # runs the !~ATH test harness over all 397 cases
make smoke    # quick hello-world sanity check
```

The test harness is itself an !~ATH program (`tests/harness.~ATH`); see
`transpiler-to-c/tests/README.md`.

## Debugging !~ATH programs

No specialized tools beyond existing C debugging tools exist for !~ATH.

The deprecated Python interpreter at `deprecated/python-interpreter/` includes a stepping debugger (`--step`), TUI debugger (`--tui`), and non-interactive JSON trace mode (`--trace`). These are useful for debugging !~ATH 1.3 logic. Run from that directory with `python3 untildeath.py --help`.


## Spec and Reference

The full !~ATH spec is located at ./athSpec.md, but a quick reference is located below.

### Quick Reference

#### ENTITIES
Entities are mortal things that can be waited upon. Each entity is either `ALIVE` or `DEAD`. Create entities with `import`:

```ath
import timer T(1s);        // dies after 1 second
import timer T2(500ms);    // dies after 500 milliseconds
import process P("cmd");   // dies when process exits
import connection C("host", 80); // dies when connection closes
import watcher W("file.txt");    // dies when file is deleted
import watcher Lib("lib.~ATH");  // loads .~ATH file as module
```

`THIS` is an implicit entity representing the program itself. Kill entities manually with `.DIE()`. The program ends when `THIS.DIE();` is called.

#### ~ATH LOOPS
The fundamental control structure. Waits for an entity to die, then runs the `EXECUTE` clause:

```ath
import timer T(1s);
~ATH(T) {
} EXECUTE(UTTER("Timer died!"));
THIS.DIE();
```

Combine entities with `&&` (both must die), `||` (either dies), or `!` (dies immediately when created):

```ath
~ATH(T1 && T2) { } EXECUTE(...);  // wait for both
~ATH(T1 || T2) { } EXECUTE(...);  // wait for either
~ATH(!T) { } EXECUTE(...);        // runs immediately
```

#### BIFURCATION
Split execution into concurrent branches:

```ath
bifurcate THIS[LEFT, RIGHT];

~ATH(LEFT) {
    // code for left branch
} EXECUTE(VOID);

~ATH(RIGHT) {
    // code for right branch
} EXECUTE(VOID);

[LEFT, RIGHT].DIE();
```

#### VARIABLES

```ath
BIRTH x WITH 5;           // mutable variable
ENTOMB PI WITH 3.14159;   // constant (immutable)
x = x + 1;                // reassignment
```

#### DATA TYPES

```ath
42, -7                    // INTEGER
3.14, -0.5                // FLOAT
"hello\nworld"            // STRING (escapes: \\ \" \n \t)
ALIVE, DEAD               // BOOLEAN (truthy/falsy)
VOID                      // absence of value
[1, 2, 3]                 // ARRAY
{name: "Karkat", age: 6}  // MAP
STACK(n)                  // fixed-size LIFO sylladex
QUEUE(n)                  // fixed-size FIFO sylladex
TREE()                    // unbounded BST sylladex (TREE(ALIVE) for AVL)
HASHMAP(n)                // fixed-size key-indexed sylladex
OUIJA(n)                  // random-slot sylladex
BOTTLE(n)                 // single-use-slot sylladex
TECHHOP(g, s, gp, sp)     // 2D predicate-routed sylladex
JUJU(n)                   // cross-branch sylladex
```

#### SYLLADICES

Sylladices are mutable structured collections where reads consume values. Write with `CAPTCHALOGUE`, read with `EJECT`. No random access or iteration — to traverse, eject repeatedly.

**Write:**

```ath
CAPTCHALOGUE value INTO S;               // STACK, QUEUE, TREE, OUIJA, BOTTLE, TECHHOP
CAPTCHALOGUE value WITH key INTO H;      // HASHMAP (WITH key required)
CAPTCHALOGUE value INTO J SLOT n;        // JUJU (SLOT required)
```

**Read:**

```ath
EJECT FROM S                             // STACK, QUEUE, OUIJA
EJECT FROM B                             // BOTTLE: lowest non-dead slot
EJECT SLOT n FROM B                      // BOTTLE: specific slot (becomes dead)
EJECT SLOT n FROM H                      // HASHMAP: by physical slot index
EJECT "key" FROM H                       // HASHMAP: by name (returns VOID on miss/collision)
EJECT ROOT FROM T                        // TREE: removes all, returns in-order ARRAY
EJECT LEAF FROM T                        // TREE: removes leftmost deepest leaf
EJECT GROOVE g SHADE s FROM TH           // TECHHOP: specific cell (required)
EJECT SLOT n FROM J                      // JUJU: must be different branch from writer
```

**Per-type summary:**

| Type | Size | Write inserts at | Read returns from | Overflow |
|---|---|---|---|---|
| STACK | fixed | front (slot 0), others shift right | front (slot 0) | last slot discarded |
| QUEUE | fixed | front (slot 0), others shift right | back (slot n-1) | last slot discarded |
| TREE | unbounded | BST position (string-coerced comparison) | ROOT (all, sorted) or LEAF (deepest left) | never overflows |
| HASHMAP | fixed | `abs(hash(key)) % size` | by key or by slot index | collision discards old pair |
| OUIJA | fixed | random slot | random slot | random slot discarded |
| BOTTLE | fixed | lowest empty slot | lowest non-dead slot (then slot dies) | discarded if no empty slots |
| TECHHOP | fixed 2D | lowest valid cell per predicate rites | explicit (groove, shade) required | discarded if no valid empty cell |
| JUJU | fixed | explicit SLOT n; records writer branch | explicit SLOT n; different branch required | error if slot occupied |

**Example (STACK):**

```ath
BIRTH S WITH STACK(3);
CAPTCHALOGUE 1 INTO S;
CAPTCHALOGUE 3 INTO S;
CAPTCHALOGUE 5 INTO S;
// S is STACK[5, 3, 1]

BIRTH top WITH EJECT FROM S;
UTTER(top);               // 5
UTTER(STRING(S));         // STACK[3, 1, VOID]
THIS.DIE();
```

**Example (JUJU — cross-branch communication):**

```ath
BIRTH J WITH JUJU(2);
bifurcate THIS[CALIBORN, CALLIOPE];

~ATH(CALIBORN) {
    CAPTCHALOGUE "HELLO WORLD" INTO J SLOT 0;
    import timer TA(10ms);
    ~ATH(TA) { } EXECUTE(VOID);
} EXECUTE(VOID);

~ATH(CALLIOPE) {
    import timer TB(5ms);
    ~ATH(TB) { } EXECUTE(
        BIRTH msg WITH EJECT SLOT 0 FROM J;
        UTTER("Calliope got:", msg);      // Calliope got: HELLO WORLD
    );
} EXECUTE(VOID);

[CALIBORN, CALLIOPE].DIE();
```

**Truthiness and COUNT:**

A sylladex is truthy if it contains any non-`VOID` value. A dead JUJU is always falsy. `COUNT(s)` returns the number of non-`VOID` occupied slots/nodes. `TYPEOF(s)` returns the uppercase type name (`"STACK"`, `"TREE"`, etc.).

#### OPERATORS

```ath
+ - * / %                     // arithmetic (/ is integer div for ints)
& | ^ ~ << >>                 // bitwise (AND, OR, XOR, NOT, shifts)
== != < > <= >=               // comparison
AND OR NOT                    // logical (short-circuit)
arr[0]  map["key"]  map.key   // indexing
```

#### CONTROL FLOW

```ath
SHOULD condition {
    // if truthy
} LEST {
    // else
}
```

**No loops in the expression language.** Use `~ATH` for iteration:

```ath
RITE countdown(n) {
    SHOULD n > 0 {
        UTTER(n);
        import timer T(1s);
        ~ATH(T) { } EXECUTE(countdown(n - 1));
    }
}
countdown(5);
THIS.DIE();
```

#### FUNCTIONS (RITES)

```ath
RITE add(a, b) {
    BEQUEATH a + b;       // return value
}
BIRTH sum WITH add(2, 3);
```

#### ERROR HANDLING

```ath
ATTEMPT {
    BIRTH x WITH PARSE_INT("bad");
} SALVAGE error {
    UTTER("Error: " + error);
}

CONDEMN "Something went wrong";  // throw error
```

#### BUILT-IN RITES

**I/O:**

```ath
UTTER("Hello", x);        // print (space-separated, newline appended)
BIRTH line WITH HEED();   // read line from input
BIRTH s WITH SCRY(VOID);  // read STDIN until EOF
BIRTH f WITH SCRY("filename"); // read file
INSCRIBE("file.txt", s);  // write file
```

**Type operations:**

```ath
TYPEOF(x)                 // "INTEGER", "FLOAT", "STRING", etc.
LENGTH(arr), LENGTH(str)  // length of array or string
PARSE_INT("42")           // string to integer
PARSE_FLOAT("3.14")       // string to float
STRING(42)                // value to string
INT(3.7)                  // float to integer (truncates)
FLOAT(42)                 // integer to float
CHAR(65), CODE("A")       // int to char / char to int code
BIN(10), HEX(255)         // int to binary/hex string
```

**Array operations:**

```ath
APPEND(arr, val)          // add to end (returns new array)
PREPEND(arr, val)         // add to start
SLICE(arr, start, end)    // subsequence
FIRST(arr), LAST(arr)     // first/last element
CONCAT(arr1, arr2)        // concatenate arrays
```

**Map operations:**

```ath
KEYS(map), VALUES(map)    // get keys/values as arrays
HAS(map, key)             // check if key exists
SET(map, key, val)        // set key (returns new map)
DELETE(map, key)          // remove key
```

**String operations:**

```ath
SPLIT("a,b,c", ",")       // split to array
JOIN(arr, ",")            // join array to string
SUBSTRING(s, start, end)  // extract substring
UPPERCASE(s), LOWERCASE(s), TRIM(s)
REPLACE(s, old, new)      // replace all occurrences
```

**Utility:**

```ath
RANDOM()                  // random float 0 to 1
RANDOM_INT(min, max)      // random integer in range
TIME()                    // Unix timestamp in ms
COUNT(sylladex)           // number of non-VOID values held by a sylladex
```
