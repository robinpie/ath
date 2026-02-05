# Try the web interpreter at [https://robinpie.neocities.org/ath/](https://robinpie.neocities.org/ath/?code=Ly8gRml6ekJ1enogdXNpbmcgcmVjdXJzaW9uIHdpdGggdGltZXIgY2hhaW5zClJJVEUgZml6emJ1enoobiwgbWF4KSB7CiAgICBTSE9VTEQgbiA8PSBtYXggewogICAgICAgIFNIT1VMRCBuICUgMTUgPT0gMCB7CiAgICAgICAgICAgIFVUVEVSKCJGaXp6QnV6eiIpOwogICAgICAgIH0gTEVTVCBTSE9VTEQgbiAlIDMgPT0gMCB7CiAgICAgICAgICAgIFVUVEVSKCJGaXp6Iik7CiAgICAgICAgfSBMRVNUIFNIT1VMRCBuICUgNSA9PSAwIHsKICAgICAgICAgICAgVVRURVIoIkJ1enoiKTsKICAgICAgICB9IExFU1QgewogICAgICAgICAgICBVVFRFUihuKTsKICAgICAgICB9CgogICAgICAgIGltcG9ydCB0aW1lciBUKDFtcyk7CiAgICAgICAgfkFUSCghVCkgewogICAgICAgIH0gRVhFQ1VURShmaXp6YnV6eihuICsgMSwgbWF4KSk7CiAgICB9Cn0KCmZpenpidXp6KDEsIDUwKTsKVEhJUy5ESUUoKTs%3D)

---

**!~ATH** (pronounced "until death") is an esoteric programming language where all control flow is predicated on waiting for things to die. Inspired by the fictional ~ATH language from Homestuck. Everything is about death. Loops wait for entities to die. Computation happens in death callbacks. The language is deliberately inconvenient. 

## Usage

All commands for the Python interpreter run from the `python-interpreter/` directory:

```
# Run a !~ATH program
python3 untildeath.py ./examples/hello.~ath

# Start the REPL
python3 untildeath.py

# Install as package
pip install -e .
```

Use the JavaScript interpreter at `js-interpreter/ath.js`.

## Debugging !~ATH programs

Both the Python and JavaScript interpreters include a stepping debugger that pauses after each statement, allowing you to inspect variables, entities, and async tasks.

#### Debugger commands

|   Command   |   Shortcut    |                Action                 |
|-------------|---------------|---------------------------------------|
| `step`      | `s` / `Enter` | Step to the next statement            |
| `continue`  | `c`           | Continue execution freely             |
| `variables` | `v`           | Show variables in current scope chain |
| `entities`  | `e`           | Show status of all entities           |
| `tasks`     | `t`           | Show pending async tasks              |
| `quit`      | `q`           | Quit the debugger and program         |

#### Python interpreter

**CLI debugger:**
Use the `--step` (or `-d`) flag:

```
python3 untildeath.py --step ./examples/hello.~ath
```

In the REPL, type `:step` to toggle debugging for the next execution.

**TUI debugger (Textual):**
For a richer graphical interface in the terminal, use the `--tui` flag:

```
python3 untildeath.py --tui ./examples/hello.~ath
```

Requires Textual. Just `pip install textual` if not installed.

### JavaScript interpreter

Use the `ath.js` CLI script with the `--step` flag:

```
node js-interpreter/ath.js --step ./examples/hello.~ath
```

## Spec and Reference

The full !~ATH spec is located at /athSpec.md, but a quick reference is located below.

### Quick Reference

#### ENTITIES
Entities are mortal things that can be waited upon. Each entity is either **alive** or **dead**. Create entities with `import`:

```ath
import timer T(1s);        // dies after 1 second
import timer T2(500ms);    // dies after 500 milliseconds
import process P("cmd");   // dies when process exits
import connection C("host", 80); // dies when connection closes
import watcher W("file.txt");    // dies when file is deleted
```

`THIS` is an implicit entity representing the program itself. Kill entities manually with `.DIE()`. The program ends when `THIS.DIE();` is called.

#### ~ATH LOOPS
The fundamental control structure. Waits for an entity to die, then runs the EXECUTE clause:

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
```

#### OPERATORS

```ath
+ - * / %                     // arithmetic (/ is integer div for ints)
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

**No loops in the expression language.** Use ~ATH for iteration:

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
```
