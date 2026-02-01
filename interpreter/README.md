# !~ATH Interpreter

A Python interpreter for the **!~ATH** (pronounced "until death") esoteric programming language, inspired by the fictional ~ATH language from Homestuck.

## Overview

!~ATH is a programming language where all control flow is predicated on waiting for things to die. Loops wait for entities to die, computation happens in death callbacks, and iteration requires chaining mortal entities.

## Installation

```bash
# From the project directory
pip install -e .

# Or run directly
python untildeath.py <file.~ath>
python -m untildeath <file.~ath>
```

## Usage

```bash
# Run a !~ATH program
python untildeath.py examples/hello.~ath

# Start the REPL
python untildeath.py
```

## Language Features

- **Entities**: Mortal things that can be waited upon (timers, processes, connections, file watchers)
- **~ATH Loops**: Wait for entities to die, then execute code
- **Bifurcation**: Split execution into concurrent branches
- **Expression Language**: Variables, functions (rites), conditionals, and more

## Example

```
// Hello World in !~ATH
import timer T(1ms);

~ATH(T) {
} EXECUTE(UTTER("Hello, world!"));

THIS.DIE();
```

## Entity Types

| Type | Description |
|------|-------------|
| `timer` | Dies after a specified duration |
| `process` | Dies when a subprocess exits |
| `connection` | Dies when a TCP connection closes |
| `watcher` | Dies when a file is deleted |
| `THIS` | The program itself |

## Expression Language

- `BIRTH x WITH value;` - Declare a variable
- `ENTOMB x WITH value;` - Declare a constant
- `RITE name(params) { ... }` - Define a function
- `SHOULD cond { ... } LEST { ... }` - Conditional
- `ATTEMPT { ... } SALVAGE err { ... }` - Error handling
- `CONDEMN "message";` - Throw an error
- `BEQUEATH value;` - Return from a rite

## Built-in Rites

### I/O
- `UTTER(value, ...)` - Print to stdout
- `HEED()` - Read line from stdin
- `SCRY(path)` - Read file contents
- `INSCRIBE(path, content)` - Write to file

### Type Operations
- `TYPEOF(value)` - Get type name
- `LENGTH(value)` - Get length of string/array
- `PARSE_INT(string)` - Parse string to integer
- `STRING(value)` - Convert to string

### Collections
- `APPEND(array, value)` - Add to end of array
- `KEYS(map)` - Get array of map keys
- `HAS(map, key)` - Check if key exists

See the specification for the complete list.

## License

MIT
