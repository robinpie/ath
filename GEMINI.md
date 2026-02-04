# GEMINI.md

This file provides guidance to agents when working with code in this repository.

## Project Overview

This is an interpreter for **!~ATH**, an esoteric programming language where all control flow is predicated on waiting for things to die. The language is inspired by the fictional ~ATH language from Homestuck.

The specification is in `!~ATH spec.md`. The Python interpreter is in `python-interpreter/`.

## Commands

All commands run from `python-interpreter/` directory:

```bash
# Run all tests (336 tests)
python3 run_tests.py

# Run a specific test module
python3 run_tests.py test_lexer
python3 run_tests.py test_parser
python3 run_tests.py test_interpreter
python3 run_tests.py test_builtins
python3 run_tests.py test_edge_cases

# Run a !~ATH program
python3 untildeath.py examples/hello.~ath

# Start the REPL
python3 untildeath.py

# Install as package
pip install -e .
```

## Python Interpreter Architecture

The interpreter follows a classic pipeline: **Lexer → Parser → Interpreter**

### Pipeline Flow

1. **`lexer.py`** - Tokenizes source code into tokens (TokenType enum). Handles durations (e.g., `1ms`, `5s`), strings, keywords, operators.

2. **`parser.py`** - Recursive descent parser. Builds AST from tokens. Key parsing methods:
   - `parse_statement()` - Dispatches to specific statement parsers
   - `parse_expression()` - Entry point for expression parsing with precedence climbing
   - Special handling for `.DIE()` since DIE is a keyword, not an identifier

3. **`interpreter.py`** - Async interpreter using Python's asyncio. Core classes:
   - `Interpreter` - Main execution engine with entity management
   - `Scope` - Variable scoping with parent chain for closures
   - `UserRite` - User-defined function representation

4. **`ast_nodes.py`** - Dataclass-based AST nodes. All fields have defaults to avoid dataclass inheritance issues.

5. **`entities.py`** - Entity implementations (things that can die):
   - `ThisEntity` - The program itself
   - `TimerEntity` - Dies after duration
   - `ProcessEntity` - Dies when subprocess exits
   - `ConnectionEntity` - Dies when TCP connection closes
   - `WatcherEntity` - Dies when file is deleted
   - `BranchEntity` - For bifurcation
   - `CompositeEntity` - For `&&`, `||`, `!` combinations

6. **`builtins.py`** - Built-in rites (UTTER, HEED, SCRY, etc.)

7. **`errors.py`** - Error hierarchy: TildeAthError → LexerError, ParseError, RuntimeError, CondemnError, BequeathError

### Interpreter Key Concepts

- **Entities** use `asyncio.Event` for death notification
- **~ATH loops** wait for entity death, then execute the EXECUTE clause
- **Bifurcation** creates concurrent branches via `asyncio.create_task()`
- **Entity reuse**: When re-importing an entity with the same name, the old one is killed and replaced

## JavaScript Interpreter

A browser-compatible JavaScript implementation of the !~ATH interpreter is in `js-interpreter/`. This is a complete port of the Python version, excluding features that require OS access.

**IMPORTANT:** The interpreter is bundled in two locations:
- `js-interpreter/` - ES modules version (for Node.js, testing)
- `webpage/tildeath.js` - IIFE bundle (for browser, works with `file:///`)

When making changes to the JS interpreter that are applicable to both, **update both locations** to keep them in sync.

### Features & Limitations

**Included**:
- All lexer, parser, and interpreter functionality
- Timer entities (using `setTimeout`)
- Bifurcation and branch entities
- All built-in rites except SCRY/INSCRIBE
- Full expression language with rites, conditionals, try-catch
- Entity combinations (&&, ||, !)
- 301 test cases (all passing)

**Excluded** (browser limitations):
- `ProcessEntity` - Cannot spawn OS processes
- `ConnectionEntity` - No raw TCP sockets
- `WatcherEntity` - No file system access
- `SCRY` builtin - No file reading
- `INSCRIBE` builtin - No file writing

### Running Tests

From project root:
```bash
# Run all 301 tests (lexer, parser, interpreter, builtins, edge-cases)
node --test js-interpreter/test/*.test.js

# Run specific test file
node --test js-interpreter/test/interpreter.test.js
```

### Directory Structure

```
js-interpreter/
├── package.json
├── src/
│   ├── index.js          # Public API (TildeAth class)
│   ├── errors.js         # Error hierarchy
│   ├── tokens.js         # TokenType enum, Token class
│   ├── lexer.js          # Tokenizer
│   ├── parser.js         # Recursive descent parser
│   ├── ast.js            # AST node factories
│   ├── entities.js       # Entity classes (uses Promise-based DeathEvent)
│   ├── scope.js          # Scope and UserRite classes
│   ├── builtins.js       # Built-in rites
│   └── interpreter.js    # Async interpreter engine
└── test/
    ├── lexer.test.js     # 60+ tokenizer tests
    ├── parser.test.js    # 100+ parser tests
    ├── interpreter.test.js   # 63 runtime behavior tests
    ├── builtins.test.js  # 70+ built-in function tests
    └── edge-cases.test.js    # Complex scenario tests
```

### Usage

```javascript
import { TildeAth } from './js-interpreter/src/index.js';

const output = [];
const runtime = new TildeAth({
  onOutput: (text) => output.push(text),
});

await runtime.run(`
  import timer T(1ms);
  ~ATH(T) { } EXECUTE(UTTER("Hello from !~ATH!"));
  THIS.DIE();
`);

console.log(output); // ["Hello from !~ATH!"]
```

### Key Implementation Details

- **Promise-based death signaling**: Uses custom `DeathEvent` class (Promise wrapper) instead of Python's `asyncio.Event`
- **Entity entity starts**: Timers use `setTimeout`, entity start methods return Promises tracked in `_pendingPromises`
- **Cleanup**: Interpreter's finally block kills all entities to ensure pending timers don't hang
- **Test framework**: Node.js built-in `node:test` module with no external dependencies
- **Module type**: ESM (ECMAScript modules)

## Known Gotchas

- The directory name `!~ath dev` contains `!` which causes bash escaping issues. Always refer to file paths from the perspective of the project directory (use relative paths like `js-interpreter/test/foo.js` rather than trying to cd into directories with special characters).
- In !~ATH, bare expressions like `1 + 2;` are not valid at the top level; must be wrapped in statements.

# !~ATH Spec

The rest of this file is the full !~ATH spec in "!~ATH spec.md" (sans implementation notes), to help you write !~ATH code.

# !~ATH Language Specification

Version 1.1

## Overview

!~ATH (pronounced "until death") is an esoteric programming language where all control flow is predicated on waiting for things to die. Inspired by the fictional ~ATH language from Homestuck, this specification describes a real, Turing-complete implementation.

Everything is about death. Loops wait for entities to die. Computation happens in death callbacks. Iteration requires chaining mortal entities. The language is deliberately inconvenient; you must "trick" !~ATH into doing what you want.

---

## Lexical Structure

### File Extension

!~ATH source code files are recommended to use the file extension `.~ath` (e.g., `program.~ath`).

### Character Set

!~ATH source files are UTF-8 encoded.

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

This program imports a 1ms timer, waits for it to die, executes VOID (doing nothing), then terminates.

**Note**: A program that tries to `~ATH(THIS)` before calling `THIS.DIE()` will deadlock—the loop waits for THIS to die, but `THIS.DIE()` is never reached. This is faithful to the original ~ATH's "insufferable" design.

---

## Entity System

Entities are mortal things that can be waited upon. Each entity is either **alive** or **dead**. ~ATH loops block until their bound entity dies.

### Entity Lifecycle

1. **Birth**: Entity is created via `import` or implicitly (THIS)
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

**Multiple statements** (semicolon-separated, final semicolon optional):
```
} EXECUTE(
    BIRTH x WITH 5;
    BIRTH y WITH x + 10;
    UTTER(y)
);
```

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

!~ATH uses **cooperative concurrency** (single-threaded event loop), not true parallelism. This means:

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

### Type Coercion

Minimal implicit coercion:
- In boolean contexts (SHOULD, AND, OR, NOT): `DEAD`, `VOID`, `0`, `""`, `[]`, `{}` are falsy; all else truthy
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
|----------|--------------------------------------------------|
| `+`      | Addition (also string concatenation)             |
| `-`      | Subtraction                                      |
| `*`      | Multiplication                                   |
| `/`      | Division (integer division for INTEGER operands) |
| `%`      | Modulo                                           |

**Comparison** (returns BOOLEAN):
| Operator |      Description      |
|----------|-----------------------|
| `==`     | Equal                 |
| `!=`     | Not equal             |
| `<`      | Less than             |
| `>`      | Greater than          |
| `<=`     | Less than or equal    |
| `>=`     | Greater than or equal |

**Logical** (operate on truthiness):
| Operator |         Description         |
|----------|-----------------------------|
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
2. `NOT` `-` (unary negation)
3. `*` `/` `%`
4. `+` `-`
5. `<` `>` `<=` `>=`
6. `==` `!=`
7. `AND`
8. `OR`

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
```

**LENGTH(value)** — Length of string or array
```
LENGTH("hello")      // 5
LENGTH([1, 2, 3])    // 3
```

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
//   watcher:    single string expression (file path)
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

expression      = logic_or ;
logic_or        = logic_and { "OR" logic_and } ;
logic_and       = equality { "AND" equality } ;
equality        = comparison { ( "==" | "!=" ) comparison } ;
comparison      = term { ( "<" | ">" | "<=" | ">=" ) term } ;
term            = factor { ( "+" | "-" ) factor } ;
factor          = unary { ( "*" | "/" | "%" ) unary } ;
unary           = ( "NOT" | "-" ) unary | postfix ;
postfix         = primary { "[" expression "]" | "." IDENTIFIER | "(" [ arg_list ] ")" } ;
primary         = INTEGER | FLOAT | STRING | "ALIVE" | "DEAD" | "VOID"
                | IDENTIFIER
                | "(" expression ")"
                | array_literal
                | map_literal
                ;

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


