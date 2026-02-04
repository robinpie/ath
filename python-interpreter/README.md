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

See the specification for info on how to use.
