# !~ATH Language Specification

Version 2.0

## Overview

!~ATH (pronounced "until death") is an esoteric programming language where all control flow is predicated on waiting for things to die. Inspired by the fictional ~ATH language from Homestuck, this specification describes a real, Turing-complete implementation.

Everything is about death. Loops wait for entities to die. Computation happens in death callbacks. Iteration requires chaining mortal entities. The language is deliberately inconvenient; you must "trick" !~ATH into doing what you want.

---

## Lexical Structure

### Files

!~ATH source code files are recommended to use the file extension `.~ATH` (e.g., `program.~ATH`). !~ATH source files are UTF-8 encoded.

### Comments

Single-line comments only:

```
// This is a comment
```

### Identifiers

Identifiers begin with a letter or underscore, followed by letters, digits, or underscores:

```
THIS
myTimer
_private
timer2
```

Identifiers are case-sensitive. `THIS` is reserved for the program entity.

### Keywords

Reserved words (cannot be used as identifiers):

```
// !~ATH constructs
import bifurcate EXECUTE DIE THIS

// Entity types
timer process connection watcher

// Expression language
BIRTH ENTOMB WITH ALIVE DEAD VOID
SHOULD LEST RITE BEQUEATH
ATTEMPT SALVAGE CONDEMN
AND OR NOT

// Sylladex constructs
CAPTCHALOGUE INTO EJECT FROM
SLOT GROOVE SHADE ROOT LEAF

// Sylladex types
STACK QUEUE TREE HASHMAP
OUIJA BOTTLE TECHHOP JUJU
```

### Literals

**Integers**: Decimal digits, optionally prefixed with `-`
```
42
-7
0
```

**Floats**: Decimal digits with a decimal point, optionally prefixed with `-`
```
3.14
-0.5
0.0
```

**Strings**: Double-quoted, with escape sequences
```
"hello"
"line1\nline2"
"say \"hello\""
```

Escape sequences:
- `\\` — backslash
- `\"` — double quote
- `\n` — newline
- `\t` — tab

**Booleans**:
```
ALIVE   // truthy
DEAD    // falsy
```

**Void**:
```
VOID    // absence of value
```

**Arrays**: Square brackets, comma-separated
```
[1, 2, 3]
["a", "b", "c"]
[1, "mixed", ALIVE]
[]
```

**Maps**: Curly braces, colon-separated key-value pairs
```
{name: "Karkat", age: 6}
{x: 1, y: 2}
{}
```

Map keys are identifiers (unquoted) or strings (quoted). Both refer to string keys.

### Duration Literals

Used in timer imports. The minimum duration is 1 millisecond:

```
1ms      // milliseconds
5s       // seconds (= 5000ms)
2m       // minutes (= 120000ms)
1h       // hours (= 3600000ms)
100      // no unit = milliseconds (default)
```

---

## Program Structure

A !~ATH program consists of these kinds of statements, which may appear in any order and at any nesting level:

1. Import statements (create entities)
2. Expression language statements (BIRTH, ENTOMB, RITE definitions, etc.)
3. ~ATH loop constructs (wait for death)
4. Bifurcation constructs (split execution)
5. DIE invocations (trigger death)

Imports are **not** restricted to the top of the file—they execute at runtime when encountered, creating entities dynamically. This allows patterns like importing timers inside EXECUTE clauses for chained iteration.

The entry point is the top level of the file. Execution proceeds sequentially until a ~ATH loop blocks, then resumes when the awaited entity dies.

Every program has an implicit `THIS` entity representing the program itself. The program terminates when `THIS.DIE()` is called and all pending operations complete.

### Minimal Valid !~ATH Program

```
import timer T(1ms);

~ATH(T) {
} EXECUTE(VOID);

THIS.DIE();
```

This program imports a 1ms timer, waits for it to die, executes `VOID` (doing nothing), then terminates.

**Note**: A program that tries to `~ATH(THIS)` before calling `THIS.DIE()` will deadlock—the loop waits for `THIS` to die, but `THIS.DIE()` is never reached. This is faithful to the original ~ATH's "insufferable" design.

---

## Entity System

Entities are mortal things that can be waited upon. Each entity is either `ALIVE` or `DEAD`. ~ATH loops block until their bound entity dies.

### Entity Lifecycle

1. **Birth**: Entity is created via `import` or implicitly (`THIS`)
2. **Life**: Entity is alive; ~ATH loops bound to it will block
3. **Death**: Entity dies (naturally or via `.DIE()`); ~ATH loops unblock and execute

### Built-in Entity Types

#### THIS

The program itself. Always implicitly available. Dies when `.DIE()` is called on it.

To use `~ATH(THIS)`, you must call `THIS.DIE()` *before* the loop (since death is scheduled asynchronously):

```
THIS.DIE();  // Schedule death

~ATH(THIS) {
} EXECUTE(UTTER("Program ending"));
```

Or more commonly, use THIS only for program termination without waiting on it:

```
import timer T(5s);

~ATH(T) {
} EXECUTE(UTTER("Done waiting"));

THIS.DIE();  // Terminate program
```

#### timer

Dies after a specified duration elapses.

**Syntax**:
```
import timer <identifier>(<duration>);
```

**Examples**:
```
import timer T(1000ms);
import timer delay(5s);
import timer longWait(2h);
```

The timer begins counting immediately upon import. When the duration elapses, the timer dies.

#### process

Dies when an external process exits.

**Syntax**:
```
import process <identifier>(<command>, <arg1>, <arg2>, ...);
```

**Examples**:
```
import process P("./script.sh");
import process P2("python", "myscript.py", "--verbose");
import process P3("sleep", "5");
```

The process is spawned immediately upon import. When it exits (for any reason, including error), the process entity dies.

#### connection

Dies when a TCP connection closes.

**Syntax**:
```
import connection <identifier>(<host>, <port>);
```

**Examples**:
```
import connection C("localhost", 8080);
import connection remote("example.com", 443);
```

A TCP connection is opened immediately upon import. When the connection closes (from either end, or due to error), the entity dies.

**Error behavior**: If the connection cannot be established (host unreachable, connection refused, etc.), the entity dies immediately. The death is scheduled via the event loop, not synchronous with the import.

#### watcher

Dies when a file is deleted.

**Syntax**:
```
import watcher <identifier>(<filepath>);
```

**Examples**:
```
import watcher W("./config.txt");
import watcher W2("/tmp/lockfile");
```

The watcher monitors the specified file. When the file is deleted, the entity dies.

**Edge case**: If the file does not exist at import time, the entity's death is scheduled immediately via the event loop (not synchronous with the import). This allows `~ATH(!W)` to still work correctly.

#### Module Imports

When the watcher's filepath ends in `.~ATH`, the file is automatically loaded as a **module**. The file is parsed and executed at import time, and its top-level rites and variables become accessible via dot notation on the watcher entity.

```
// library.~ATH
RITE add(a, b) {
    BEQUEATH a + b;
}
BIRTH version WITH 1;
THIS.DIE();
```

```
// main.~ATH
import watcher Lib("./library.~ATH");

BIRTH result WITH Lib.add(3, 4);
UTTER(result);   // 7
UTTER(Lib.version);  // 1

THIS.DIE();
```

**Module semantics**:
- The module runs in its own scope with its own `THIS` entity. Module execution completes fully before the import statement returns.
- All top-level `BIRTH` variables, `ENTOMB` constants, and `RITE` definitions become exports accessible via `ModuleName.export`.
- The watcher entity still functions normally—it dies when the file is deleted.
- Modules should call `THIS.DIE()` (a warning is emitted if missing, same as regular programs).
- `TYPEOF(ModuleName)` returns `"MODULE"`.

**Circular import detection**: If module A imports module B which imports module A, a runtime error is raised.

**Re-importing**: Importing the same file again re-executes it and refreshes exports (the old watcher entity is killed and replaced, as with all entity re-imports).

**Non-`.~ATH` files**: Watcher entities for non-`.~ATH` files behave as before—no module loading, no exports, not accessible as values in expressions.

### Entity Operations

#### .DIE()

Manually kills an entity.

```
THIS.DIE();
myTimer.DIE();
```

Calling `.DIE()` on an already-dead entity has no effect.

### Entity Combinations

Entities can be combined using operators to create compound death conditions. These operators are **only valid within entity expressions** (inside `~ATH(...)` parentheses).

**Important**: Entities are **not** booleans. An entity's alive/dead state does not coerce to `ALIVE`/`DEAD`. You cannot use an entity in a `SHOULD` condition or assign it to a variable. Entities exist solely to be waited upon.

#### AND (&&)

Dies when **both** entities have died.

```
~ATH(T1 && T2) {
} EXECUTE(UTTER("Both timers finished"));
```

#### OR (||)

Dies when **either** entity has died.

```
~ATH(T1 || T2) {
} EXECUTE(UTTER("At least one timer finished"));
```

#### NOT (!)

The inverse of an entity. Dies when the entity **begins to exist** (is imported). Useful for triggering on creation rather than destruction.

```
import timer T(1s);

~ATH(!T) {
} EXECUTE(UTTER("Timer was created"));  // Executes immediately
```

**Lexical note**: The `!` operator is **only valid inside entity expressions**. Using `!` anywhere else (e.g., in a regular expression like `!x`) is a **syntax error**. Use `NOT` for boolean negation in expressions.

Combinations can be nested:

```
~ATH((T1 && T2) || T3) {
} EXECUTE(UTTER("Complex condition met"));
```

---

## ~ATH Loop Construct

The fundamental control structure. ~ATH has **two distinct modes** depending on the type of entity:

1. **Wait mode** (regular entities): Waits for the entity to die, then executes code
2. **Branch mode** (branch entities): Defines code that runs as the branch

### Syntax

```
~ATH(<entity-expression>) {
    // Body
} EXECUTE(<expression-or-block>);
```

### Wait Mode Semantics (Regular Entities)

When the entity is a regular entity (timer, process, connection, watcher, THIS, or combinations thereof):

1. The loop binds to the specified entity (or entity combination)
2. If the entity is already dead, the EXECUTE is **scheduled** (proceeds via event loop)
3. If the entity is alive, **yield** to the event loop until it dies
4. When the entity dies, execute the EXECUTE clause
5. Continue to the next statement

**Body restriction**: In wait mode, the body may only contain nested ~ATH loops. Any other statements (imports, variable declarations, expressions) are a **semantic error**. All computation must go in EXECUTE clauses.

```
~ATH(T) {
    // Only nested ~ATH loops allowed here—anything else is an error
    ~ATH(T2) {
    } EXECUTE(...);
} EXECUTE(...);
```

### Branch Mode Semantics (Branch Entities)

When the entity is a branch entity (created by `bifurcate`):

1. The body and EXECUTE clause together define the branch's code
2. The branch executes this code concurrently with sibling branches
3. The branch entity dies when its code **fully completes**

**Branch completion** means:
- All statements in the body have executed
- All nested ~ATH waits have resolved (their entities have died)
- All EXECUTE clauses (including nested ones) have finished
- Any recursively nested ~ATH loops have fully completed

In other words, a branch doesn't die until its entire subtree of execution is done.

**Body freedom**: In branch mode, the body may contain any statements, including imports, variable declarations, and nested ~ATH loops.

See the **Bifurcation** section for details.

### EXECUTE Clause

The EXECUTE clause runs after the entity dies (wait mode) or as part of the branch (branch mode). It contains expression language code.

**Single expression**:
```
} EXECUTE(UTTER("done"));
```

**Multiple statements**: Zero or more statements (each terminated by `;` per its own production), optionally followed by a single trailing bare expression with no `;`:
```
} EXECUTE(
    BIRTH x WITH 5;
    BIRTH y WITH x + 10;
    UTTER(y)
);
```
Statements such as variable declarations, assignments, and nested `~ATH` loops must each end in `;`. Only the optional final bare expression may omit its semicolon.

**Empty EXECUTE**: Use `VOID` as the canonical no-op:
```
} EXECUTE(VOID);
```

Note: `EXECUTE()` with nothing inside is a syntax error. Always use `EXECUTE(VOID)` for an empty execution.

**Nested ~ATH** (for chaining):
```
} EXECUTE(
    import timer T2(1s);
    ~ATH(T2) {
    } EXECUTE(UTTER("Second timer done"));
);
```

### Nesting

~ATH loops can be nested. To chain timers, place imports and nested loops inside EXECUTE clauses:

```
import timer T1(1s);

~ATH(T1) {
} EXECUTE(
    UTTER("Outer timer done");
    
    import timer T2(500ms);
    ~ATH(T2) {
    } EXECUTE(UTTER("Inner timer done"));
);

THIS.DIE();
```

Output (after ~1.5s):
```
Outer timer done
Inner timer done
```

---

## Bifurcation

Bifurcation splits program execution into concurrent branches. Each branch can wait on different entities independently.

### Syntax

```
bifurcate <entity>[<branch1>, <branch2>];
```

### Semantics

1. The specified entity is split into two named branches
2. Both branches begin executing concurrently (structured concurrency via async)
3. The original entity (e.g., THIS) now represents the combination of its branches—it dies only when ALL branches have died
4. Branches can be further bifurcated

### Example

```
bifurcate THIS[LEFT, RIGHT];

~ATH(LEFT) {
    import timer T1(1s);
    ~ATH(T1) {
    } EXECUTE(UTTER("Left branch: 1 second"));
} EXECUTE(VOID);

~ATH(RIGHT) {
    import timer T2(2s);
    ~ATH(T2) {
    } EXECUTE(UTTER("Right branch: 2 seconds"));
} EXECUTE(VOID);

[LEFT, RIGHT].DIE();
```

Output:
```
Left branch: 1 second
Right branch: 2 seconds
```

### Recombination

Bifurcated branches must be recombined to be killed together:

```
[BRANCH1, BRANCH2].DIE();
```

**Note**: The `[A, B]` syntax in DIE targets is syntactic sugar for "both A and B". It is equivalent to killing both entities. Order does not matter. This syntax is **only valid in DIE targets**—it is not an entity combination like `&&`.

Nested bifurcation requires nested recombination:

```
bifurcate THIS[A, B];
bifurcate B[B1, B2];

// ... code ...

[A, [B1, B2]].DIE();
```

### Branch Independence

Each branch has its own execution context:
- Branches run concurrently (via async/cooperative multitasking, not OS threads)
- One branch dying does **not** kill sibling branches
- Variables are shared (lexical scoping applies across branches)
- Imports in one branch are visible to other branches

### Memory Model

!~ATH uses cooperative concurrency (single-threaded event loop), not true parallelism. This means:

- Only one branch executes at a time
- Branches yield at ~ATH wait points (when waiting for an entity to die)
- Variable access is **not** subject to data races in the traditional sense
- However, the order of execution between branches at yield points is **nondeterministic**

If two branches both modify a shared variable between yield points, the final value depends on scheduling order. This is intentional—!~ATH does not provide synchronization primitives.

### Branch Semantics

After `bifurcate THIS[LEFT, RIGHT]`, `LEFT` and `RIGHT` become **branch entities**. The interpreter tracks which identifiers are branch entities.

Using a branch entity in `~ATH` triggers **branch mode** (see ~ATH Loop Construct):

```
~ATH(LEFT) {
    // Code that runs as the LEFT branch
} EXECUTE(...);
```

Within a branch, you can use regular ~ATH loops to wait on other entities (timers, processes, etc.). The branch doesn't die until all its internal waits complete.

---

## Expression Language

The expression language (!~ATH/EXPR) handles computation within EXECUTE clauses and at the top level.

### Data Types

|  Type   |         Description         |       Examples        |
|---------|-----------------------------|-----------------------|
| INTEGER | Arbitrary-precision integer | `42`, `-7`, `0`       |
| FLOAT   | IEEE 754 floating point     | `3.14`, `-0.5`        |
| STRING  | UTF-8 text                  | `"hello"`, `"line\n"` |
| BOOLEAN | Truth value                 | `ALIVE`, `DEAD`       |
| VOID    | Absence of value            | `VOID`                |
| ARRAY   | Ordered collection          | `[1, 2, 3]`           |
| MAP     | Key-value collection        | `{a: 1, b: 2}`        |
| STACK   | Fixed-size LIFO sylladex    | `STACK(3)`            |
| QUEUE   | Fixed-size FIFO sylladex    | `QUEUE(5)`            |
| TREE    | Unbounded BST sylladex      | `TREE()`              |
| HASHMAP | Key-indexed sylladex        | `HASHMAP(8)`          |
| OUIJA   | Random-access sylladex      | `OUIJA(8)`            |
| BOTTLE  | Single-use-slot sylladex    | `BOTTLE(5)`           |
| TECHHOP | 2D predicate-filtered       | `TECHHOP(3, 4, g, s)` |
| JUJU    | Cross-branch sylladex       | `JUJU(4)`             |

See the **Sylladices** section for details on the sylladex types.

### Type Coercion

Minimal implicit coercion:
- In boolean contexts (SHOULD, AND, OR, NOT): `DEAD`, `VOID`, `0`, `""`, `[]`, `{}`, empty/all-`VOID` sylladices, and dead JUJUs are falsy; all else truthy (see the **Sylladices** section for per-type truthiness rules)
- String concatenation with `+`: non-strings are converted via `STRING()` built-in
- No implicit numeric coercion between INTEGER and FLOAT (use explicit conversion)

### Variables

**Declaration with initialization** (required):
```
BIRTH x WITH 5;
BIRTH name WITH "Karkat";
BIRTH list WITH [1, 2, 3];
```

**Constant declaration** (immutable):
```
ENTOMB PI WITH 3.14159;
ENTOMB GREETING WITH "Hello";
```

Attempting to reassign an entombed variable is a runtime error.

**Reassignment**:
```
BIRTH x WITH 5;
x = 10;
x = x + 1;
```

### Operators

**Arithmetic** (INTEGER and FLOAT):
| Operator |                   Description                    |
|----------|------------------------------------------------|
| `+`      | Addition (also string concatenation)             |
| `-`      | Subtraction                                      |
| `*`      | Multiplication                                   |
| `/`      | Division (integer division for INTEGER operands) |
| `%`      | Modulo                                           |

**Bitwise** (INTEGER only):
| Operator |      Description       |
|---------|------------------------|
| `&`      | Bitwise AND            |
| `|`      | Bitwise OR             |
| `^`      | Bitwise XOR            |
| `~`      | Bitwise NOT (unary)    |
| `<<`     | Left shift             |
| `>>`     | Right shift            |

**Comparison** (returns BOOLEAN):
| Operator |      Description      |
|----------|----------------------|
| `==`     | Equal                 |
| `!=`     | Not equal             |
| `<`      | Less than             |
| `>`      | Greater than          |
| `<=`     | Less than or equal    |
| `>=`     | Greater than or equal |

**Logical** (operate on truthiness):
| Operator |         Description         |
|----------|----------------------------|
| `AND`    | Logical and (short-circuit) |
| `OR`     | Logical or (short-circuit)  |
| `NOT`    | Logical negation            |

**Indexing**:
```
arr[0]          // array index (0-based)
map["key"]      // map access with string
map.key         // map access with identifier (equivalent to map["key"])
```

**Operator Precedence** (highest to lowest):
1. `.` `[]` (member access, indexing)
2. `NOT` `-` `~` (unary negation, bitwise NOT)
3. `*` `/` `%`
4. `+` `-`
5. `<<` `>>`
6. `&`
7. `^`
8. `|`
9. `<` `>` `<=` `>=`
10. `==` `!=`
11. `AND`
12. `OR`

Parentheses override precedence.

### Control Flow

**Conditional**:
```
SHOULD condition {
    // executes if truthy
}
```

**Conditional with alternative**:
```
SHOULD condition {
    // executes if truthy
} LEST {
    // executes if falsy
}
```

**Chained conditional**:
```
SHOULD condition1 {
    // ...
} LEST SHOULD condition2 {
    // ...
} LEST {
    // ...
}
```

**No loops in expression language**. Iteration must be achieved through ~ATH constructs with mortal entities.

### Functions (Rites)

**Definition**:
```
RITE functionName(param1, param2) {
    // body
    BEQUEATH returnValue;
}
```

**Calling**:
```
BIRTH result WITH functionName(arg1, arg2);
```

**Return value**:
- `BEQUEATH value;` returns a value and exits the rite
- If no BEQUEATH is reached, the rite returns `VOID`
- BEQUEATH with no value returns `VOID`

**Example**:
```
RITE factorial(n) {
    SHOULD n <= 1 {
        BEQUEATH 1;
    }
    BEQUEATH n * factorial(n - 1);
}

BIRTH result WITH factorial(5);
UTTER(result);  // 120
```

Note: Recursion is allowed but deep recursion may hit stack limits. For iteration, prefer ~ATH with timers.

### Error Handling

**Try-catch equivalent**:
```
ATTEMPT {
    // code that might fail
    BIRTH x WITH PARSE_INT("not a number");
} SALVAGE error {
    // handle error
    UTTER("Error: " + error);
}
```

The `error` variable in SALVAGE is a STRING describing the error.

**Throwing errors**:
```
CONDEMN "Something went wrong";
```

CONDEMN immediately exits to the nearest SALVAGE block, or terminates the program if uncaught.

### Built-in Rites

#### I/O

**UTTER(value, ...)** — Print to stdout
```
UTTER("Hello");              // prints: Hello
UTTER("x =", x);             // prints: x = <value of x>
UTTER(1, 2, 3);              // prints: 1 2 3
```
Multiple arguments are space-separated. A newline is appended.

**HEED()** — Read line from stdin
```
BIRTH input WITH HEED();     // blocks until line entered
```
Returns the line as a STRING (without trailing newline).

**SCRY(path)** — Read file contents or stdin
```
BIRTH contents WITH SCRY("./data.txt");
BIRTH stdin WITH SCRY(VOID); // read from stdin until EOF
```
Returns file contents as a STRING. Throws error if file doesn't exist or can't be read.

**INSCRIBE(path, content)** — Write to file
```
INSCRIBE("./output.txt", "Hello, world!");
```
Overwrites file if it exists, creates if it doesn't. Throws error on failure.

#### Type Operations

**TYPEOF(value)** — Get type as string
```
TYPEOF(42)           // "INTEGER"
TYPEOF(3.14)         // "FLOAT"
TYPEOF("hi")         // "STRING"
TYPEOF(ALIVE)        // "BOOLEAN"
TYPEOF(VOID)         // "VOID"
TYPEOF([1,2])        // "ARRAY"
TYPEOF({a:1})        // "MAP"
TYPEOF(STACK(3))     // "STACK"
TYPEOF(TREE())       // "TREE"
// ...and similarly "QUEUE", "HASHMAP", "OUIJA", "BOTTLE", "TECHHOP", "JUJU"
```

**LENGTH(value)** — Length of string or array
```
LENGTH("hello")      // 5
LENGTH([1, 2, 3])    // 3
```

**COUNT(sylladex)** — Number of non-`VOID` values held by a sylladex
```
COUNT(STACK(3))                  // 0 (empty stack)
COUNT(myTree)                    // number of nodes
COUNT(myHashmap)                 // number of occupied slots
```
The precise definition varies by sylladex type; see the **Sylladices** section.

**PARSE_INT(string)** — Parse string to integer
```
PARSE_INT("42")      // 42
PARSE_INT("3.14")    // error
PARSE_INT("abc")     // error
```

**PARSE_FLOAT(string)** — Parse string to float
```
PARSE_FLOAT("3.14")  // 3.14
PARSE_FLOAT("42")    // 42.0
PARSE_FLOAT("abc")   // error
```

**STRING(value)** — Convert to string representation
```
STRING(42)           // "42"
STRING([1,2,3])      // "[1, 2, 3]"
STRING({a:1})        // "{a: 1}"
```

**INT(value)** — Convert float to integer (truncates)
```
INT(3.7)             // 3
INT(-2.9)            // -2
```

**FLOAT(value)** — Convert integer to float
```
FLOAT(42)            // 42.0
```

**CHAR(value)** — Convert integer code point to character string
```
CHAR(65)             // "A"
CHAR(9786)           // "☺"
```

**CODE(value)** — Get integer code point of first character in string
```
CODE("A")            // 65
CODE("☺")            // 9786
```

**BIN(value)** — Convert integer to binary string
```
BIN(10)              // "1010"
```

**HEX(value)** — Convert integer to hexadecimal string
```
HEX(255)             // "FF"
```

#### Array Operations

**APPEND(array, value)** — Add element to end
```
BIRTH arr WITH [1, 2];
arr = APPEND(arr, 3);    // [1, 2, 3]
```
Returns a new array (does not mutate).

**PREPEND(array, value)** — Add element to beginning
```
BIRTH arr WITH [2, 3];
arr = PREPEND(arr, 1);   // [1, 2, 3]
```

**SLICE(array, start, end)** — Extract subsequence
```
SLICE([1,2,3,4,5], 1, 4)   // [2, 3, 4]
```
Indices are 0-based. End is exclusive.

**FIRST(array)** — Get first element
```
FIRST([1, 2, 3])     // 1
FIRST([])            // error
```

**LAST(array)** — Get last element
```
LAST([1, 2, 3])      // 3
LAST([])             // error
```

**CONCAT(array1, array2)** — Concatenate arrays
```
CONCAT([1, 2], [3, 4])   // [1, 2, 3, 4]
```

#### Map Operations

**KEYS(map)** — Get array of keys
```
KEYS({a: 1, b: 2})   // ["a", "b"]
```

**VALUES(map)** — Get array of values
```
VALUES({a: 1, b: 2}) // [1, 2]
```

**HAS(map, key)** — Check if key exists
```
HAS({a: 1}, "a")     // ALIVE
HAS({a: 1}, "b")     // DEAD
```

**SET(map, key, value)** — Set key-value pair
```
BIRTH m WITH {a: 1};
m = SET(m, "b", 2);      // {a: 1, b: 2}
```
Returns a new map (does not mutate).

**DELETE(map, key)** — Remove key
```
BIRTH m WITH {a: 1, b: 2};
m = DELETE(m, "a");      // {b: 2}
```

#### String Operations

**SPLIT(string, delimiter)** — Split string into array
```
SPLIT("a,b,c", ",")      // ["a", "b", "c"]
SPLIT("hello", "")       // ["h", "e", "l", "l", "o"]
```

**JOIN(array, delimiter)** — Join array into string
```
JOIN(["a", "b", "c"], ",")   // "a,b,c"
```

**SUBSTRING(string, start, end)** — Extract substring
```
SUBSTRING("hello", 1, 4)     // "ell"
```

**UPPERCASE(string)** — Convert to uppercase
```
UPPERCASE("hello")       // "HELLO"
```

**LOWERCASE(string)** — Convert to lowercase
```
LOWERCASE("HELLO")       // "hello"
```

**TRIM(string)** — Remove leading/trailing whitespace
```
TRIM("  hello  ")        // "hello"
```

**REPLACE(string, old, new)** — Replace occurrences
```
REPLACE("hello", "l", "w")   // "hewwo"
```

#### Utility

**RANDOM()** — Random float between 0 (inclusive) and 1 (exclusive)
```
BIRTH r WITH RANDOM();   // e.g., 0.7291...
```

**RANDOM_INT(min, max)** — Random integer in range (inclusive)
```
BIRTH r WITH RANDOM_INT(1, 6);   // e.g., 4
```

**TIME()** — Current Unix timestamp in milliseconds
```
BIRTH now WITH TIME();
```

---

## Sylladices

A **sylladex** is a mutable, structured collection of values. Unlike arrays, sylladices are interacted with only through type-specific write and read operations — there is no random access, no indexing, and no iteration construct. To inspect a stored value, one must read it out, and reading consumes.

Each sylladex type defines its own capacity model (fixed-size with overflow ejection, or unbounded), its own insertion rule, and its own read access points. Slots and nodes may hold any value (INTEGER, FLOAT, STRING, BOOLEAN, VOID, ARRAY, MAP, or another sylladex); element types need not be uniform within a single sylladex.

The currently defined sylladex types are **STACK**, **QUEUE**, **TREE**, **HASHMAP**, **OUIJA**, **BOTTLE**, **TECHHOP**, and **JUJU**.

> *Naming note:* Sylladices are named after the Homestuck inventory system of the same name, in which most "fetch modi" impose deliberately inconvenient rules on item retrieval. !~ATH preserves this inconvenience: every operation displaces or consumes something, and the choice of sylladex type is a commitment to a particular flavor of inconvenience. `STACK`, `QUEUE`, and `TREE` are not exactly like the typical abstract data types of the same name

### Common operations

Two operations are defined on all sylladices. Their precise semantics vary by type (see each type's section).

- `CAPTCHALOGUE expr INTO sylladex;` — write. Statement only; does not yield a value. The ejected value (if any) is discarded. Some sylladex types extend this syntax with modifiers (e.g., `WITH key` for HASHMAP, `SLOT n` for JUJU).
- `EJECT FROM sylladex` — read. Expression; evaluates to the consumed value. The exact return value (and whether `VOID`, a scalar, or an array) depends on the sylladex type and the read modifier used. Some sylladex types extend this syntax with modifiers (e.g., `EJECT ROOT FROM T`).

Sylladices may **not** be indexed with `[]` or have their internal structure assigned directly. Attempting to do so is a runtime error. There is no iteration construct; to traverse contents, one must repeatedly `EJECT`, which destroys the sylladex.

The built-in `COUNT(sylladex)` returns the number of non-`VOID` values currently held. For fixed-size sylladices, this is the number of populated slots. For trees, it is the number of nodes. For hashmaps and other typed sylladices, it is the number of occupied slots.

### Initialization

Each sylladex type has its own initialization syntax (see each type's section). All sylladices are initialized empty.

Size requirements vary by type: STACK and QUEUE permit any non-negative size (including zero, which has well-defined no-op behavior); HASHMAP, OUIJA, BOTTLE, JUJU require positive sizes (their operations involve modulo arithmetic or per-slot bookkeeping that is undefined at size zero); TREE is unbounded and takes no size argument; TECHHOP requires positive dimensions for both grooves and shades.

Some sylladex types (HASHMAP, TECHHOP) accept rite arguments at initialization. For purposes of these types, rites are first-class values: a rite name evaluates to a reference to that rite, which can be passed as an argument and stored internally by the sylladex. Rite-valued arguments are compared by identity (two references to the same rite definition are equal; two distinct rite definitions with identical bodies are not equal). When a sylladex evaluates a user-supplied predicate rite, the rite's return value is interpreted using the standard truthiness rules from the Type Coercion section (so `ALIVE`, non-zero numbers, non-empty strings, etc. all count as "yes").

### Equality

Two sylladices are `==` iff they have the same type and are structurally identical (same shape, same values in corresponding positions, and any type-specific metadata such as hash functions or branch participants). Sylladices of different types are never `==`, even if they contain the same values.

### Truthiness

A sylladex is **truthy** if it contains any non-`VOID` value, and **falsy** otherwise. Specific truthiness rules are clarified per type.

### Concurrency

A single `CAPTCHALOGUE` or `EJECT` operation is atomic with respect to branch scheduling: the entire operation completes before yielding to the event loop. Two branches racing to operate on the same sylladex will produce a well-defined result in some order, but the order is nondeterministic, consistent with the general bifurcation memory model.

### STACK

A STACK is a fixed-size, LIFO (last-in, first-out) sylladex. Both writes and reads operate at the **front** of the stack (slot 0).

**Initialization:**

```
BIRTH S WITH STACK(3);   // STACK[VOID, VOID, VOID]
```

The size argument must be a non-negative integer. Negative sizes are a runtime error. Size is fixed at initialization and cannot change.

**Write** (`CAPTCHALOGUE expr INTO S`): Inserts a value into slot 0. All existing slots shift one position toward higher indices. The value previously occupying the final slot is ejected (discarded).

**Read** (`EJECT FROM S`): Removes and returns the value in slot 0. All remaining slots shift one position toward lower indices, and the final slot is refilled with `VOID`.

**Example:**

```
BIRTH S WITH STACK(3);
CAPTCHALOGUE 1 INTO S;
CAPTCHALOGUE 3 INTO S;
CAPTCHALOGUE 5 INTO S;
// S is now STACK[5, 3, 1]

CAPTCHALOGUE 2 INTO S;
// S is now STACK[2, 5, 3]; 1 was discarded
UTTER(STRING(S));                // STACK[2, 5, 3]

BIRTH top WITH EJECT FROM S;
UTTER(top);                      // 2
UTTER(STRING(S));                // STACK[5, 3, VOID]

THIS.DIE();
```

**Truthiness:** Truthy if any slot holds a non-`VOID` value; falsy if all slots are `VOID` (including zero-size stacks, vacuously).

**Degenerate case:** `STACK(0)` is legal. `CAPTCHALOGUE x INTO STACK(0)` discards `x` immediately. `EJECT FROM STACK(0)` returns `VOID`. The sylladex is unchanged in both cases.

**Type and representation:**
- `TYPEOF(STACK(...))` returns `"STACK"`
- `STRING(STACK(3))` returns `"STACK[VOID, VOID, VOID]"`

### QUEUE

A QUEUE is a fixed-size, FIFO (first-in, first-out) sylladex. Writes operate at the **front** (slot 0); reads operate at the **back** (slot *n*−1).

**Initialization:**

```
BIRTH Q WITH QUEUE(5);   // QUEUE[VOID, VOID, VOID, VOID, VOID]
```

The size argument must be a non-negative integer. Negative sizes are a runtime error. Size is fixed at initialization and cannot change.

**Write** (`CAPTCHALOGUE expr INTO Q`): Inserts a value into slot 0. All existing slots shift one position toward higher indices. The value previously occupying the final slot is ejected (discarded). *(Identical to STACK write.)*

**Read** (`EJECT FROM Q`): Removes and returns the value in the final slot (slot *n*−1). All remaining slots shift one position toward higher indices to fill the vacated slot, and slot 0 is refilled with `VOID`.

**Example:**

```
BIRTH Q WITH QUEUE(3);
CAPTCHALOGUE "mixed" INTO Q;
CAPTCHALOGUE 1 INTO Q;
// Q is now QUEUE[1, "mixed", VOID]

CAPTCHALOGUE 2 INTO Q;
// Q is now QUEUE[2, 1, "mixed"]; the trailing VOID was discarded
UTTER(STRING(Q));                // QUEUE[2, 1, "mixed"]

BIRTH first WITH EJECT FROM Q;
UTTER(STRING(first));            // "mixed"
UTTER(STRING(Q));                // QUEUE[VOID, 2, 1]

THIS.DIE();
```

**Truthiness:** Truthy if any slot holds a non-`VOID` value; falsy if all slots are `VOID` (including zero-size queues, vacuously).

**Degenerate case:** `QUEUE(0)` is legal. `CAPTCHALOGUE x INTO QUEUE(0)` discards `x` immediately. `EJECT FROM QUEUE(0)` returns `VOID`. The sylladex is unchanged in both cases.

**Type and representation:**
- `TYPEOF(QUEUE(...))` returns `"QUEUE"`
- `STRING(QUEUE(2))` returns `"QUEUE[VOID, VOID]"`

### TREE

A TREE is an unbounded binary-search-tree sylladex. Unlike STACK and QUEUE, it has no declared size and grows as values are captchalogued. Internal nodes are inaccessible; only the **root** and **leaf** nodes can be read.

**Initialization:**

```
BIRTH T1 WITH TREE();             // unbalanced (default)
BIRTH T1 WITH TREE(DEAD);         // unbalanced (explicit)
BIRTH T2 WITH TREE(ALIVE);        // AVL-rebalancing on insert
```

The optional argument is a BOOLEAN indicating whether to enable auto-balancing. `ALIVE` enables AVL-style rebalancing after each insert; `DEAD` (the default if no argument is given) leaves the tree as inserted. The flag is set at birth and cannot change. Although `ALIVE` and `DEAD` are the canonical arguments, any truthy value enables balancing and any falsy value disables it.

**Write** (`CAPTCHALOGUE expr INTO T`):
- If the tree is empty, the new value becomes the root.
- Otherwise, the new value is compared against the root using string-coerced lexicographic comparison (`STRING(new) < STRING(node)` goes left, otherwise right). Recursion continues until an empty child slot is reached, where the new value is placed.
- Equal-valued items (where `STRING(new) == STRING(node)`) are placed in the right subtree.
- If the tree was created with `ALIVE`, the tree may rebalance after insertion using AVL-style rotations, which can promote previously-internal nodes to root or leaf positions.

Trees are unbounded and never overflow. Captchaloguing never ejects an existing value.

**Read** (`EJECT ROOT FROM T` or `EJECT LEAF FROM T`):

Trees require an explicit modifier. Bare `EJECT FROM T` on a tree is a runtime error.

- `EJECT ROOT FROM T` — Removes the root and cascades: all values in the tree are ejected. Returns an ARRAY of all ejected values in in-order traversal (which, given the BST invariant, is sorted by string comparison). The tree becomes empty. If the tree was already empty, returns `[]`. A single-node tree yields a single-element array.
- `EJECT LEAF FROM T` — Removes and returns the **leftmost leaf at maximum depth** as a bare value. If the tree has only a root (the root is itself a leaf), the root is removed and returned as a bare value, and the tree becomes empty. If the tree is empty, returns `VOID`.

Note that `EJECT ROOT` always returns an array (or `[]` for an empty tree), while `EJECT LEAF` returns a bare value or `VOID`. These return shapes are asymmetric by design.

**Example:**

```
BIRTH T WITH TREE();
CAPTCHALOGUE "banana" INTO T;
CAPTCHALOGUE "apple" INTO T;
CAPTCHALOGUE "cherry" INTO T;
// T is now:
//        banana
//        /    \
//    apple   cherry
UTTER(STRING(T));                // TREE["banana": ["apple", "banana", "cherry"]]

BIRTH plucked WITH EJECT LEAF FROM T;
UTTER(plucked);                  // apple
// T is now:
//        banana
//             \
//            cherry

BIRTH dumped WITH EJECT ROOT FROM T;
UTTER(STRING(dumped));           // ["banana", "cherry"]
UTTER(STRING(T));                // TREE[]

THIS.DIE();
```

**Truthiness:** Truthy if non-empty; falsy if empty.

**Type and representation:**
- `TYPEOF(TREE())` returns `"TREE"`
- `STRING()` represents a tree as `TREE[<root>: <in-order array>]`, e.g. `TREE["banana": ["apple", "banana", "cherry"]]`. An empty tree is `TREE[]`.

### HASHMAP

A HASHMAP is a fixed-size, key-indexed sylladex. Values are stored in slots determined by hashing their keys. Unlike STACK and QUEUE, retrieval is by key (or by slot index) rather than by position-in-order.

**Initialization:**

```
BIRTH H WITH HASHMAP(8);                  // size 8, default hash function
BIRTH H WITH HASHMAP(8, myHashRite);      // size 8, custom hash function
```

The size argument must be a positive integer. Zero-size and negative-size hashmaps are a runtime error (unlike STACK/QUEUE, the modulo operation is undefined for size 0).

The optional second argument is a rite of signature `RITE(string) -> integer`. The hashmap automatically applies `abs(result) % size` to map the rite's output to a valid slot index. If no rite is provided, the default hash sums the UTF-8 byte values of the key.

The hash function is fixed at birth and cannot change.

**Write** (`CAPTCHALOGUE value WITH key INTO H`):

The `WITH key` clause is **required** for hashmaps. `CAPTCHALOGUE value INTO H` without a key is a syntax error.

- The key is coerced to a string via `STRING(key)`.
- The string is hashed; the slot index is `abs(hash) % size`.
- The value (paired internally with its key) is placed in that slot.
- If the slot was already occupied, the previous (key, value) pair is ejected (discarded). This includes the case where the same key is captchalogued again — the old value is overwritten.

**Read** (`EJECT key FROM H` or `EJECT SLOT n FROM H`):

- `EJECT expr FROM H` — by-name access. The expression is coerced to a string, hashed to find the slot, and the slot's stored key is compared to the requested key.
  - If the slot is empty: returns `VOID`.
  - If the stored key matches the requested key: returns the value and clears the slot.
  - If the stored key differs (collision victim occupies the slot): returns `VOID`; the slot is **not** cleared.

- `EJECT SLOT n FROM H` — by-slot access. Removes and returns the value in slot `n` regardless of key. The stored key is discarded along with the value. If the slot is empty, returns `VOID`. If `n` is out of range (negative or `>= size`), runtime error.

Note that `EJECT 3 FROM H` looks up the key `"3"` (via string coercion), while `EJECT SLOT 3 FROM H` looks up the physical slot at index 3. These are different operations.

**Example:**

```
// HASHMAP(4) with the default hash function:
// "ab" sums to 97+98 = 195; 195 % 4 = 3
// "cd" sums to 99+100 = 199; 199 % 4 = 3
//
BIRTH H WITH HASHMAP(4);

CAPTCHALOGUE "first" WITH "ab" INTO H;
UTTER(STRING(H));                // HASHMAP[VOID, VOID, VOID, "ab"->"first"]

CAPTCHALOGUE "second" WITH "cd" INTO H;
// collision at slot 3; ("ab", "first") is discarded
UTTER(STRING(H));                // HASHMAP[VOID, VOID, VOID, "cd"->"second"]

BIRTH miss WITH EJECT "ab" FROM H;
UTTER(STRING(miss));             // VOID
// "ab" hashes to slot 3, but that slot holds "cd"; H is unchanged
UTTER(STRING(H));                // HASHMAP[VOID, VOID, VOID, "cd"->"second"]

BIRTH hit WITH EJECT "cd" FROM H;
UTTER(hit);                      // second
UTTER(STRING(H));                // HASHMAP[VOID, VOID, VOID, VOID]

BIRTH empty WITH EJECT SLOT 3 FROM H;
UTTER(STRING(empty));            // VOID

THIS.DIE();
```

**Truthiness:** Truthy if any slot is occupied; falsy if all slots are empty.

**Type and representation:**
- `TYPEOF(HASHMAP(...))` returns `"HASHMAP"`
- `STRING()` represents a hashmap by showing all slots in order, with occupied slots formatted as `<key>-><value>` and empty slots as `VOID`. Example: `HASHMAP[VOID, VOID, VOID, "cd"->"second"]`.

**Equality:** Two hashmaps are `==` iff they have the same size, the same hash function (compared by rite identity — not by extensional equivalence of outputs), and the same (key, value) pair in every slot. Two hashmaps with different hash functions are never `==` even if they happen to hold identical contents, since their subsequent operations would diverge.

### OUIJA

An OUIJA is a fixed-size sylladex where both writes and reads target uniformly random slots. The spirits decide what gets stored where, and what gets retrieved.

**Initialization:** `BIRTH O WITH OUIJA(n);` — size *n* must be a positive integer.

**Write** (`CAPTCHALOGUE value INTO O`): Selects a uniformly random slot index. The value previously at that slot is ejected (discarded). The new value is placed there.

**Read** (`EJECT FROM O`): Selects a uniformly random slot index. Returns the value at that slot (possibly `VOID` if the spirits picked an empty one) and clears the slot.

**Truthiness:** Truthy if any slot is occupied; falsy otherwise.

**Type and representation:**
- `TYPEOF(OUIJA(...))` returns `"OUIJA"`
- `STRING()` represents an ouija as `OUIJA[VOID, "hammer", VOID, "skateboard"]`

**Equality:** Two ouijas are `==` iff same size and same contents in same positions.

**Concurrency:** Random selection is implementation-defined and not seedable. Concurrent operations on the same OUIJA get independent random choices.

**Example:**

```
BIRTH O WITH OUIJA(4);
CAPTCHALOGUE "hammer" INTO O;
CAPTCHALOGUE "skateboard" INTO O;
// Each value lands in a random slot; if both land in the same slot,
// the first is discarded. Possible state: OUIJA[VOID, "skateboard", "hammer", VOID]
UTTER(STRING(O));

BIRTH whatever WITH EJECT FROM O;
UTTER(STRING(whatever));         // possibly "hammer", "skateboard", or VOID
UTTER(COUNT(O));                 // 0, 1, or 2 depending on luck

THIS.DIE();
```

### BOTTLE

A BOTTLE is a fixed-size sylladex where each slot is single-use: once read, the slot becomes permanently dead and cannot be written or read again.

Slots have three states: *empty* (no value, usable), *occupied* (holds a value), or *dead* (used up, unusable).

**Initialization:** `BIRTH B WITH BOTTLE(n);` — size *n* must be a positive integer. All slots start *empty*.

**Write** (`CAPTCHALOGUE value INTO B`): Places the value in the lowest-indexed *empty* slot, which becomes *occupied*. If there is no empty slot (all are occupied or dead), the value is discarded.

**Read** (`EJECT FROM B` or `EJECT SLOT n FROM B`):

- `EJECT FROM B` — reads from the lowest-indexed non-dead slot. If that slot is *occupied*, returns its value. If that slot is *empty*, returns `VOID`. Either way, the slot becomes *dead*. If all slots are dead, runtime error.

- `EJECT SLOT n FROM B` — reads from slot `n`. If *occupied*, returns the value; if *empty*, returns `VOID`. Either way, the slot becomes *dead*. If the slot was already *dead*, runtime error. If `n` is out of range, runtime error.

Slot-dead and all-dead errors can be recovered via `ATTEMPT/SALVAGE`.

**Truthiness:** Truthy if any slot is *occupied*; falsy otherwise. Dead slots do not affect truthiness.

`COUNT(B)` returns the number of *occupied* slots; dead slots are not counted.

**Type and representation:**
- `TYPEOF(BOTTLE(...))` returns `"BOTTLE"`
- `STRING()` shows occupied slots as values, empty slots as `VOID`, and dead slots as `DEAD`, e.g. `BOTTLE[VOID, "msg1", DEAD, VOID, "msg2"]`

**Equality:** Two bottles are `==` iff same size and same per-slot state (same values in occupied slots, same dead status in dead slots, same empty status in empty slots).

**Example:**

```
BIRTH B WITH BOTTLE(3);
CAPTCHALOGUE "first message" INTO B;
CAPTCHALOGUE "second message" INTO B;
UTTER(STRING(B));                // BOTTLE["first message", "second message", VOID]

BIRTH msg WITH EJECT FROM B;
UTTER(msg);                      // first message
UTTER(STRING(B));                // BOTTLE[DEAD, "second message", VOID]

// Slot 0 is now permanently dead. Captchaloguing again uses the next
// empty slot (skipping the dead one).
CAPTCHALOGUE "third message" INTO B;
UTTER(STRING(B));                // BOTTLE[DEAD, "second message", "third message"]

// Reading slot 0 would error, so guard with ATTEMPT/SALVAGE:
ATTEMPT {
    BIRTH dead WITH EJECT SLOT 0 FROM B;
    UTTER("got:", dead);
} SALVAGE err {
    UTTER("slot 0 is broken:", err);   // slot 0 is broken: <error message>
}

THIS.DIE();
```

### TECHHOP

A TECHHOP is a fixed-size 2D sylladex organized into **groove rows** and **shade columns**. Membership in each row and column is determined by user-supplied predicate rites. A value can only be placed in a cell where both its row's groove-predicate and its column's shade-predicate evaluate truthy.

**Initialization:**

```
BIRTH TH WITH TECHHOP(grooves, shades, groovePredicate, shadePredicate);
```

- `grooves` — number of groove rows (positive integer)
- `shades` — number of shade columns (positive integer)
- `groovePredicate` — rite of signature `RITE(value, groove_index) -> BOOLEAN`; returns whether the value belongs in that groove
- `shadePredicate` — rite of signature `RITE(value, shade_index) -> BOOLEAN`; returns whether the value belongs in that shade

Both predicates are fixed at birth.

**Write** (`CAPTCHALOGUE value INTO TH`):

1. The modus evaluates `groovePredicate(value, g)` for each groove index `g`, collecting the set of valid grooves.
2. Similarly evaluates `shadePredicate(value, s)` for each shade index `s`, collecting the set of valid shades.
3. The valid placement cells are the cross-product of valid grooves and valid shades.
4. Of these, the lowest-indexed empty cell is chosen (rows-first ordering: groove 0 shade 0, groove 0 shade 1, ..., groove 1 shade 0, ...).
5. The value is placed there.
6. If no valid empty cell exists (because no cells qualified, or all qualifying cells are occupied), the value is ejected (discarded).

Unlike HASHMAP, collisions do not overwrite — occupied cells are inviolate until explicitly ejected.

**Read** (`EJECT GROOVE g SHADE s FROM TH`): Returns and clears the cell at (groove `g`, shade `s`). If the cell is empty, returns `VOID`. If `g` or `s` is out of range, runtime error.

Bare `EJECT FROM TH` is a runtime error — TECHHOP requires explicit positional access.

**Truthiness:** Truthy if any cell is occupied; falsy otherwise.

`COUNT(TH)` returns the number of occupied cells.

**Type and representation:**
- `TYPEOF(TECHHOP(...))` returns `"TECHHOP"`
- `STRING()` shows the grid as nested arrays, rows-first, e.g. `TECHHOP[[VOID, "GameBro", VOID], ["Faygo", VOID, "sword"]]`

**Equality:** Two techhops are `==` iff same dimensions, same predicate rites (by identity, not extensional equivalence), and same cell contents.

**Example:**

```
// Groove 0 holds strings; groove 1 holds integers.
RITE byType(value, g) {
    SHOULD g == 0 {
        BEQUEATH TYPEOF(value) == "STRING";
    }
    SHOULD g == 1 {
        BEQUEATH TYPEOF(value) == "INTEGER";
    }
    BEQUEATH DEAD;
}

// Shade 0 holds short items (length < 4 for strings, value < 10 for ints);
// shade 1 holds long items. Use LENGTH for strings, magnitude for ints.
RITE bySize(value, s) {
    BIRTH metric WITH 0;
    SHOULD TYPEOF(value) == "STRING" {
        metric = LENGTH(value);
    } LEST {
        metric = value;
    }
    SHOULD s == 0 {
        BEQUEATH metric < 4;
    }
    BEQUEATH metric >= 4;
}

BIRTH TH WITH TECHHOP(2, 2, byType, bySize);

CAPTCHALOGUE "hi" INTO TH;       // string, length 2 -> (groove 0, shade 0)
CAPTCHALOGUE 42 INTO TH;          // int, value 42  -> (groove 1, shade 1)
CAPTCHALOGUE "hello" INTO TH;     // string, length 5 -> (groove 0, shade 1)
CAPTCHALOGUE 3 INTO TH;           // int, value 3   -> (groove 1, shade 0)

UTTER(STRING(TH));
// TECHHOP[["hi", "hello"], [3, 42]]

BIRTH s WITH EJECT GROOVE 0 SHADE 1 FROM TH;
UTTER(s);                         // hello

THIS.DIE();
```

### JUJU

A JUJU is a fixed-size, slot-indexed sylladex enforcing that each value passes between *different* bifurcate branches. The branch that writes a value into a slot is not the same branch that may read it back out.

JUJUs maintain internal references to bifurcate branches; these references are interpreter-managed bookkeeping, not first-class values. A user cannot retrieve a branch reference from a JUJU or compare them directly. The relevant comparisons (writer-of-slot vs. current-branch, alive/dead status of participants) are performed by the interpreter at the moment of each operation.

**Initialization:** `BIRTH J WITH JUJU(n);` — size *n* must be a positive integer. No branch arguments; participants are determined dynamically by use.

**Write** (`CAPTCHALOGUE value INTO J SLOT n`): Places the value in slot *n* and records the current branch as the slot's writer. The current branch is added to the JUJU's set of *participants*. If slot *n* is already occupied, runtime error. If *n* is out of range, runtime error. If executed outside any branch (e.g., at top level, before any `bifurcate`), runtime error — JUJUs require a branch context.

**Read** (`EJECT SLOT n FROM J`): Reads slot *n*. The current branch must be a different branch from the one recorded as the slot's writer; if it is the same branch, runtime error. The current branch is added to the JUJU's set of *participants*. Returns the value and clears the slot (the slot becomes empty and has no recorded writer). If slot *n* is empty, returns `VOID` without error (but still records the current branch as a participant). If *n* is out of range, runtime error. If executed outside any branch, runtime error.

Bare `CAPTCHALOGUE value INTO J` and bare `EJECT FROM J` are syntax errors. JUJUs require explicit slot addressing.

**Participants:** Any branch that successfully performs a CAPTCHALOGUE or EJECT on the JUJU becomes a participant. Participation is permanent until the branch dies.

**Death:** A JUJU dies when fewer than two of its participants remain alive. While at least two participants are alive, the JUJU functions normally. A JUJU with zero or one participants is alive but inert; operations still work and add new participants, but if a JUJU's only participant dies before a second one joins, the JUJU dies.

Operations on a dead JUJU are runtime errors (catchable via `ATTEMPT/SALVAGE`).

**Truthiness:** Truthy if any slot is occupied; falsy otherwise. A dead JUJU is falsy regardless of contents.

`COUNT(J)` returns the number of occupied slots; zero for a dead JUJU.

**Type and representation:**
- `TYPEOF(JUJU(...))` returns `"JUJU"`
- `STRING()` shows slots with a diagnostic writer annotation; the exact format of the writer label is implementation-defined since branches are not first-class values. A common rendering is `JUJU[VOID, "msg"<-#1, VOID]` where `#1` is some implementation-chosen identifier (branch name, internal ID, or similar). Empty slots and unrecorded-writer slots render as `VOID`.

**Equality:** Two JUJUs are `==` iff same size, same slot contents with same writer branches, same participant set, and same death status.

**Example:**

```
BIRTH J WITH JUJU(2);

bifurcate THIS[ALICE, BOB];

~ATH(ALICE) {
    // Alice writes into slot 0
    CAPTCHALOGUE "hello bob" INTO J SLOT 0;

    // Wait briefly so Bob has time to receive before Alice dies.
    import timer TA(10ms);
    ~ATH(TA) {
    } EXECUTE(VOID);
} EXECUTE(UTTER("Alice done"));

~ATH(BOB) {
    // Bob waits a moment, then reads slot 0.
    import timer TB(5ms);
    ~ATH(TB) {
    } EXECUTE(
        BIRTH msg WITH EJECT SLOT 0 FROM J;
        UTTER("Bob got:", msg);          // Bob got: hello bob
    );
} EXECUTE(UTTER("Bob done"));

[ALICE, BOB].DIE();
```

If Alice tried to read slot 0 herself (the same branch that wrote it), it would be a runtime error. Similarly, after both ALICE and BOB die the JUJU has fewer than two participants alive and subsequently dies; any further operations would error (catchable via `ATTEMPT/SALVAGE`).

---

## Scoping Rules

!~ATH uses **lexical scoping**.

1. Variables declared at the top level are global
2. Variables declared inside a RITE are local to that rite
3. Variables declared inside EXECUTE blocks are scoped to that block and nested blocks
4. Nested EXECUTE blocks can access variables from outer scopes
5. Bifurcated branches share the same scope (can access and modify the same variables)

Example:
```
BIRTH x WITH 1;              // global

RITE test() {
    BIRTH y WITH 2;          // local to test
    BEQUEATH x + y;          // can access global x
}

import timer T(1s);
~ATH(T) {
} EXECUTE(
    BIRTH z WITH 3;          // scoped to this EXECUTE
    
    import timer T2(1s);
    ~ATH(T2) {
    } EXECUTE(
        UTTER(x);            // can access global
        UTTER(z);            // can access outer EXECUTE scope
    );
);
```

---

## Execution Model

### Event Loop Architecture

!~ATH uses a **single-threaded event loop** for all execution. This has important implications:

1. **No true parallelism**: Only one piece of code runs at a time
2. **Cooperative yielding**: Code yields control at ~ATH wait points
3. **Asynchronous death**: Entity deaths are processed via the event loop, never inline

### Death Notification

When an entity dies (timer expires, process exits, file deleted, `.DIE()` called):

1. The death event is **queued** in the event loop
2. The currently executing code continues until it yields (hits a ~ATH wait or completes)
3. The event loop processes the death, unblocking any ~ATH loops waiting on that entity
4. Unblocked EXECUTE clauses are **scheduled**, not run immediately

This means deaths are **never synchronous**. Even if you call `T.DIE()` and immediately have `~ATH(T)`, the ~ATH will yield to the event loop before its EXECUTE runs.

### Sequential Execution with Blocking

1. Statements execute sequentially from top to bottom
2. When a ~ATH loop is encountered:
   - If the entity is already dead, the EXECUTE is **scheduled** (not run inline)
   - If the entity is alive, **yield** to the event loop until it dies
3. After EXECUTE completes, continue to the next statement

### Nested ~ATH During EXECUTE

When an EXECUTE clause contains a ~ATH loop:

```
~ATH(T1) {
} EXECUTE(
    import timer T2(1s);
    ~ATH(T2) {
    } EXECUTE(UTTER("T2 done"));
    UTTER("After T2");
);
```

Execution proceeds depth-first:
1. T1 dies, EXECUTE begins
2. T2 is imported
3. `~ATH(T2)` yields to event loop, waiting for T2
4. (1 second passes)
5. T2 dies, inner EXECUTE runs, prints "T2 done"
6. "After T2" prints
7. Outer EXECUTE completes

### Concurrent Execution (Bifurcation)

Bifurcated branches run concurrently using structured concurrency:

1. When `bifurcate` is executed, both branches are **scheduled** to run
2. Each branch executes independently and can block on different entities
3. Branches yield at ~ATH wait points, allowing other branches to progress
4. The event loop interleaves branch execution at yield points
5. When an entity dies, all ~ATH loops waiting on it are unblocked

### Program Termination

The program terminates when:
1. `THIS.DIE()` is called (or all branches of a bifurcated THIS die)
2. All pending EXECUTE clauses complete
3. No ~ATH loops are waiting

An uncaught error (CONDEMN without SALVAGE) also terminates the program with an error status.

---

## Grammar (EBNF)

```ebnf
program         = { statement } ;

statement       = import_stmt
                | bifurcate_stmt
                | ath_loop
                | die_stmt
                | expr_stmt
                ;

import_stmt     = "import" entity_type IDENTIFIER "(" import_args ")" ";" ;

// Import arguments differ by entity type:
//   timer:      single duration literal
//   process:    one or more string expressions (command + args)
//   connection: two expressions (host string, port integer)
//   watcher:    single string expression (file path; if .~ATH, loaded as module)
import_args     = duration                              // for timer
                | expression { "," expression }         // for process, connection, watcher
                ;

entity_type     = "timer" | "process" | "connection" | "watcher" ;

bifurcate_stmt  = "bifurcate" IDENTIFIER "[" IDENTIFIER "," IDENTIFIER "]" ";" ;

// ~ATH has two semantic modes with the same syntax.
// The interpreter determines mode based on whether the entity is a branch entity.

ath_loop        = "~ATH" "(" entity_expr ")" "{" ath_body "}" "EXECUTE" "(" execute_body ")" ";" ;

// WAIT-MODE ~ATH (entity is timer, process, connection, watcher, THIS, or combination):
//   - Blocks until entity dies, then runs EXECUTE
//   - ath_body may ONLY contain nested ath_loop statements
//   - Any other statement type in ath_body is a semantic error
//
// BRANCH-MODE ~ATH (entity is a branch identifier created by bifurcate):
//   - Does NOT block; defines code that runs as that branch
//   - ath_body may contain ANY statements (imports, variables, nested ~ATH, etc.)
//   - Branch dies when fully complete: all nested ~ATH waits resolved, all EXECUTEs finished

ath_body        = { statement } ;  // Semantic restrictions based on mode (see above)

entity_expr     = entity_term { ( "&&" | "||" ) entity_term } ;
entity_term     = [ "!" ] entity_atom ;
entity_atom     = IDENTIFIER
                | "(" entity_expr ")"
                ;

die_stmt        = die_target ".DIE" "(" ")" ";" ;
die_target      = IDENTIFIER
                | "[" die_target "," die_target "]"
                ;

execute_body    = { expr_statement } [ expression ] ;

expr_statement  = var_decl
                | const_decl
                | assignment
                | rite_def
                | conditional
                | attempt_salvage
                | condemn_stmt
                | bequeath_stmt
                | import_stmt
                | ath_loop
                | captchalogue_stmt
                | expression ";"
                ;

var_decl        = "BIRTH" IDENTIFIER "WITH" expression ";" ;
const_decl      = "ENTOMB" IDENTIFIER "WITH" expression ";" ;
assignment      = IDENTIFIER "=" expression ";" ;

rite_def        = "RITE" IDENTIFIER "(" [ param_list ] ")" "{" { expr_statement } "}" ;
param_list      = IDENTIFIER { "," IDENTIFIER } ;

conditional     = "SHOULD" expression "{" { expr_statement } "}" [ "LEST" ( conditional | "{" { expr_statement } "}" ) ] ;

attempt_salvage = "ATTEMPT" "{" { expr_statement } "}" "SALVAGE" IDENTIFIER "{" { expr_statement } "}" ;

condemn_stmt    = "CONDEMN" expression ";" ;

bequeath_stmt   = "BEQUEATH" [ expression ] ";" ;

captchalogue_stmt = "CAPTCHALOGUE" expression
                    [ "WITH" expression ]              // hashmap key
                    "INTO" expression
                    [ "SLOT" expression ]              // juju slot
                    ";" ;

// Modifier validity is determined by the destination sylladex type:
//   STACK, QUEUE, TREE, OUIJA, BOTTLE, TECHHOP:  no modifier
//   HASHMAP:                                     WITH clause required
//   JUJU:                                        SLOT clause required
// Using the wrong modifier for the sylladex type is a runtime error.

expression      = logic_or ;
logic_or        = logic_and { "OR" logic_and } ;
logic_and       = equality { "AND" equality } ;
equality        = comparison { ( "==" | "!=" ) comparison } ;
comparison      = bit_or { ( "<" | ">" | "<=" | ">=" ) bit_or } ;
bit_or          = bit_xor { "|" bit_xor } ;
bit_xor         = bit_and { "^" bit_and } ;
bit_and         = shift { "&" shift } ;
shift           = term { ( "<<" | ">>" ) term } ;
term            = factor { ( "+" | "-" ) factor } ;
factor          = unary { ( "*" | "/" | "%" ) unary } ;
unary           = ( "NOT" | "-" | "~" ) unary | postfix ;
postfix         = primary { "[" expression "]" | "." IDENTIFIER | "(" [ arg_list ] ")" } ;
primary         = INTEGER | FLOAT | STRING | "ALIVE" | "DEAD" | "VOID"
                | IDENTIFIER
                | "(" expression ")"
                | array_literal
                | map_literal
                | eject_expr
                | sylladex_constructor
                ;

sylladex_constructor = sylladex_type_name "(" [ arg_list ] ")" ;
sylladex_type_name   = "STACK" | "QUEUE" | "TREE" | "HASHMAP"
                     | "OUIJA" | "BOTTLE" | "TECHHOP" | "JUJU" ;

// Sylladex constructors look syntactically like rite calls, but the type name
// is a reserved keyword and refers to the language's built-in constructor for
// that sylladex type. Argument validity (count, type, value range) is enforced
// per the per-type rules in the Sylladices section.

eject_expr      = "EJECT" [ eject_modifier ] "FROM" expression ;

eject_modifier  = "ROOT"                                          // tree
                | "LEAF"                                          // tree
                | "SLOT" expression                               // hashmap, bottle, juju
                | "GROOVE" expression "SHADE" expression          // techhop
                | expression                                      // hashmap by-name
                ;

// The eject_modifier is optional in the grammar but its presence and form
// are constrained by the source sylladex type:
//   STACK, QUEUE, OUIJA:          modifier omitted
//   BOTTLE:                       modifier omitted, or SLOT expression
//   HASHMAP:                      SLOT expression, or expression (by-name)
//   TREE:                         ROOT or LEAF (required)
//   TECHHOP:                      GROOVE _ SHADE _ (required)
//   JUJU:                         SLOT expression (required)
// Using the wrong modifier (or omitting a required one) for the sylladex type
// is a runtime error.
//
// Parsing note: the `expression` alternative (hashmap by-name) and `FROM` keyword
// are disambiguated by lookahead — after `EJECT`, if the next token is one of
// `ROOT`, `LEAF`, `SLOT`, `GROOVE`, or `FROM`, the corresponding production is
// chosen; otherwise the parser begins an expression and expects it to be
// followed by `FROM`.

array_literal   = "[" [ expression { "," expression } ] "]" ;
map_literal     = "{" [ map_entry { "," map_entry } ] "}" ;
map_entry       = ( IDENTIFIER | STRING ) ":" expression ;

arg_list        = expression { "," expression } ;

duration        = INTEGER [ "ms" | "s" | "m" | "h" ] ;  // no unit = milliseconds
```

### Semantic Notes on Grammar

The grammar above is **syntactically permissive**—it accepts programs that are semantically invalid. The following semantic rules must be enforced by the interpreter:

1. **Wait-mode ~ATH bodies**: When the entity in `~ATH(entity)` is NOT a branch entity, the body may only contain nested `ath_loop` statements. Other statement types are a semantic error.

2. **Entity expression scope**: The `!` operator and `&&`/`||` for entities are only valid inside entity expressions. Using `!` in a regular expression is a syntax error.

3. **EXECUTE cannot be empty**: `EXECUTE()` is a syntax error. Use `EXECUTE(VOID)` for no-op.

4. **Import argument validation**: The `import_args` production accepts either a duration or expressions, but the interpreter must validate that the correct form is used for each entity type (see grammar comments).

5. **Module watcher entities**: When a watcher imports a `.~ATH` file, the entity becomes a module. Member access (`W.name`) on module entities resolves to module exports. Non-module entities are not accessible as values in expressions.

6. **Sylladex modifier validity**: The grammar accepts modifiers on `CAPTCHALOGUE` (`WITH`, `SLOT`) and `EJECT` (`ROOT`, `LEAF`, `SLOT`, `GROOVE`/`SHADE`) generically, but each sylladex type accepts only specific modifiers. Using a modifier inappropriate to the destination sylladex type, or omitting a required modifier, is a runtime error. See the **Sylladices** section for per-type rules.

7. **Sylladex type names as reserved constructors**: `STACK`, `QUEUE`, `TREE`, `HASHMAP`, `OUIJA`, `BOTTLE`, `TECHHOP`, and `JUJU` are reserved constructor names and cannot be redefined as rites.

8. **CAPTCHALOGUE and EJECT target types**: `CAPTCHALOGUE ... INTO x` and `EJECT ... FROM x` require `x` to evaluate to a sylladex. Operating on any other value type is a runtime error.

---

## Example Programs

### Hello World

```
import timer T(1ms);

~ATH(T) {
} EXECUTE(UTTER("Hello, world!"));

THIS.DIE();
```

### Countdown (Chained Timers)

```
RITE countdown(n) {
    SHOULD n > 0 {
        UTTER(n);
        import timer T(1s);
        ~ATH(T) {
        } EXECUTE(countdown(n - 1));
    } LEST {
        UTTER("Liftoff!");
    }
}

countdown(5);
THIS.DIE();
```

### File Watcher (Dies on Deletion)

```
UTTER("Watching for config.txt to be deleted...");

import watcher W("./config.txt");

~ATH(W) {
} EXECUTE(
    UTTER("config.txt was deleted!");
    UTTER("Shutting down gracefully...");
);

THIS.DIE();
```

### Concurrent Timers (Bifurcation)

```
bifurcate THIS[LEFT, RIGHT];

~ATH(LEFT) {
    import timer T1(1s);
    ~ATH(T1) {
    } EXECUTE(UTTER("Left: 1 second"));
    
    import timer T2(1s);
    ~ATH(T2) {
    } EXECUTE(UTTER("Left: 2 seconds"));
} EXECUTE(UTTER("Left branch complete"));

~ATH(RIGHT) {
    import timer T3(1500ms);
    ~ATH(T3) {
    } EXECUTE(UTTER("Right: 1.5 seconds"));
} EXECUTE(UTTER("Right branch complete"));

[LEFT, RIGHT].DIE();
```

Expected output:
```
Left: 1 second
Right: 1.5 seconds
Left: 2 seconds
Left branch complete
Right branch complete
```

### Process Watcher

```
UTTER("Starting long-running process...");

import process P("sleep", "5");

~ATH(P) {
} EXECUTE(
    UTTER("Process completed!");
);

THIS.DIE();
```

### FizzBuzz (Recursive with Timer Chain)

```
RITE fizzbuzz(n, max) {
    SHOULD n <= max {
        SHOULD n % 15 == 0 {
            UTTER("FizzBuzz");
        } LEST SHOULD n % 3 == 0 {
            UTTER("Fizz");
        } LEST SHOULD n % 5 == 0 {
            UTTER("Buzz");
        } LEST {
            UTTER(n);
        }
        
        // Chain to next iteration via timer
        import timer T(1ms);
        ~ATH(T) {
        } EXECUTE(fizzbuzz(n + 1, max));
    }
}

fizzbuzz(1, 15);
THIS.DIE();
```

### Either-Or: First Timer Wins

```
import timer T1(1s);
import timer T2(2s);

~ATH(T1 || T2) {
} EXECUTE(
    UTTER("At least one timer finished!");
);

THIS.DIE();
```

### Both Required

```
import timer T1(1s);
import timer T2(2s);

~ATH(T1 && T2) {
} EXECUTE(
    UTTER("Both timers finished!");
);

THIS.DIE();
```

### Module Import

```
// mathlib.~ATH
RITE factorial(n) {
    SHOULD n <= 1 {
        BEQUEATH 1;
    }
    BEQUEATH n * factorial(n - 1);
}

RITE fib(n) {
    SHOULD n <= 1 {
        BEQUEATH n;
    }
    BEQUEATH fib(n - 1) + fib(n - 2);
}

THIS.DIE();
```

```
// main.~ATH
import watcher Math("./mathlib.~ATH");

UTTER(Math.factorial(5));  // 120
UTTER(Math.fib(10));       // 55

THIS.DIE();
```

---

## Implementation Notes

### Error Handling

Runtime errors should:
1. Include source location (line, column)
2. Propagate to nearest SALVAGE block
3. Terminate program if uncaught
