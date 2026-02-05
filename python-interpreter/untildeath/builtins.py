"""Built-in rites (functions) for !~ATH."""

import random
import sys
import time
from typing import Any, List

from .errors import RuntimeError


def stringify(value: Any) -> str:
    """Convert a value to its string representation."""
    if value is None:
        return "VOID"
    if isinstance(value, bool):
        return "ALIVE" if value else "DEAD"
    if isinstance(value, list):
        return "[" + ", ".join(stringify(v) for v in value) + "]"
    if isinstance(value, dict):
        entries = ", ".join(f"{k}: {stringify(v)}" for k, v in value.items())
        return "{" + entries + "}"
    return str(value)


def type_name(value: Any) -> str:
    """Get the type name of a value."""
    if value is None:
        return "VOID"
    if isinstance(value, bool):
        return "BOOLEAN"
    if isinstance(value, int):
        return "INTEGER"
    if isinstance(value, float):
        return "FLOAT"
    if isinstance(value, str):
        return "STRING"
    if isinstance(value, list):
        return "ARRAY"
    if isinstance(value, dict):
        return "MAP"
    return "UNKNOWN"


def is_truthy(value: Any) -> bool:
    """Determine if a value is truthy."""
    if value is None:
        return False
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return len(value) > 0
    if isinstance(value, (list, dict)):
        return len(value) > 0
    return True


class Builtins:
    """Container for all built-in rites."""

    def __init__(self, interpreter):
        self.interpreter = interpreter
        self._input_buffer = []

    def get(self, name: str):
        """Get a built-in function by name."""
        builtins = {
            # I/O
            'UTTER': self.utter,
            'HEED': self.heed,
            'SCRY': self.scry,
            'INSCRIBE': self.inscribe,

            # Type operations
            'TYPEOF': self.typeof,
            'LENGTH': self.length,
            'PARSE_INT': self.parse_int,
            'PARSE_FLOAT': self.parse_float,
            'STRING': self.string,
            'INT': self.int_,
            'FLOAT': self.float_,
            'CHAR': self.char,
            'CODE': self.code,
            'BIN': self.bin,
            'HEX': self.hex,

            # Array operations
            'APPEND': self.append,
            'PREPEND': self.prepend,
            'SLICE': self.slice,
            'FIRST': self.first,
            'LAST': self.last,
            'CONCAT': self.concat,

            # Map operations
            'KEYS': self.keys,
            'VALUES': self.values,
            'HAS': self.has,
            'SET': self.set_,
            'DELETE': self.delete,

            # String operations
            'SPLIT': self.split,
            'JOIN': self.join,
            'SUBSTRING': self.substring,
            'UPPERCASE': self.uppercase,
            'LOWERCASE': self.lowercase,
            'TRIM': self.trim,
            'REPLACE': self.replace,

            # Utility
            'RANDOM': self.random,
            'RANDOM_INT': self.random_int,
            'TIME': self.time,
        }
        return builtins.get(name)

    # ============ I/O ============

    def utter(self, *args) -> None:
        """Print values to stdout."""
        output = " ".join(stringify(arg) for arg in args)
        print(output)
        return None

    def heed(self) -> str:
        """Read a line from stdin."""
        try:
            return input()
        except EOFError:
            return ""

    def scry(self, path: Any) -> str:
        """Read file contents or stdin."""
        if path is None:
            return sys.stdin.read()

        if not isinstance(path, str):
            raise RuntimeError(f"SCRY expects string path or VOID, got {type_name(path)}")
        try:
            with open(path, 'r', encoding='utf-8') as f:
                return f.read()
        except FileNotFoundError:
            raise RuntimeError(f"File not found: {path}")
        except IOError as e:
            raise RuntimeError(f"Cannot read file: {e}")

    def inscribe(self, path: str, content: str) -> None:
        """Write content to file."""
        if not isinstance(path, str):
            raise RuntimeError(f"INSCRIBE expects string path, got {type_name(path)}")
        if not isinstance(content, str):
            content = stringify(content)
        try:
            with open(path, 'w', encoding='utf-8') as f:
                f.write(content)
        except IOError as e:
            raise RuntimeError(f"Cannot write file: {e}")
        return None

    # ============ Type Operations ============

    def typeof(self, value: Any) -> str:
        """Get type name of value."""
        return type_name(value)

    def length(self, value: Any) -> int:
        """Get length of string or array."""
        if isinstance(value, (str, list)):
            return len(value)
        raise RuntimeError(f"LENGTH expects string or array, got {type_name(value)}")

    def parse_int(self, value: str) -> int:
        """Parse string to integer."""
        if not isinstance(value, str):
            raise RuntimeError(f"PARSE_INT expects string, got {type_name(value)}")
        try:
            # Ensure it's a valid integer (not float)
            if '.' in value:
                raise ValueError("Float string")
            return int(value)
        except ValueError:
            raise RuntimeError(f"Cannot parse '{value}' as integer")

    def parse_float(self, value: str) -> float:
        """Parse string to float."""
        if not isinstance(value, str):
            raise RuntimeError(f"PARSE_FLOAT expects string, got {type_name(value)}")
        try:
            return float(value)
        except ValueError:
            raise RuntimeError(f"Cannot parse '{value}' as float")

    def string(self, value: Any) -> str:
        """Convert value to string."""
        return stringify(value)

    def int_(self, value: Any) -> int:
        """Convert float to integer (truncate)."""
        if isinstance(value, float):
            return int(value)
        if isinstance(value, int):
            return value
        raise RuntimeError(f"INT expects number, got {type_name(value)}")

    def float_(self, value: Any) -> float:
        """Convert integer to float."""
        if isinstance(value, (int, float)):
            return float(value)
        raise RuntimeError(f"FLOAT expects number, got {type_name(value)}")

    def char(self, value: int) -> str:
        """Convert integer code point to character."""
        if not isinstance(value, int):
            raise RuntimeError(f"CHAR expects integer, got {type_name(value)}")
        try:
            return chr(value)
        except ValueError:
            raise RuntimeError(f"Invalid code point: {value}")

    def code(self, value: str) -> int:
        """Get integer code point of first character."""
        if not isinstance(value, str):
            raise RuntimeError(f"CODE expects string, got {type_name(value)}")
        if len(value) == 0:
            raise RuntimeError("CODE called on empty string")
        return ord(value[0])

    def bin(self, value: int) -> str:
        """Convert integer to binary string."""
        if not isinstance(value, int):
            raise RuntimeError(f"BIN expects integer, got {type_name(value)}")
        return bin(value)[2:]  # Strip '0b' prefix

    def hex(self, value: int) -> str:
        """Convert integer to hex string."""
        if not isinstance(value, int):
            raise RuntimeError(f"HEX expects integer, got {type_name(value)}")
        return hex(value)[2:].upper()  # Strip '0x' prefix and uppercase

    # ============ Array Operations ============

    def append(self, arr: list, value: Any) -> list:
        """Add element to end of array."""
        if not isinstance(arr, list):
            raise RuntimeError(f"APPEND expects array, got {type_name(arr)}")
        return arr + [value]

    def prepend(self, arr: list, value: Any) -> list:
        """Add element to beginning of array."""
        if not isinstance(arr, list):
            raise RuntimeError(f"PREPEND expects array, got {type_name(arr)}")
        return [value] + arr

    def slice(self, arr: list, start: int, end: int) -> list:
        """Extract subsequence from array."""
        if not isinstance(arr, list):
            raise RuntimeError(f"SLICE expects array, got {type_name(arr)}")
        if not isinstance(start, int) or not isinstance(end, int):
            raise RuntimeError("SLICE expects integer indices")
        return arr[start:end]

    def first(self, arr: list) -> Any:
        """Get first element of array."""
        if not isinstance(arr, list):
            raise RuntimeError(f"FIRST expects array, got {type_name(arr)}")
        if len(arr) == 0:
            raise RuntimeError("FIRST called on empty array")
        return arr[0]

    def last(self, arr: list) -> Any:
        """Get last element of array."""
        if not isinstance(arr, list):
            raise RuntimeError(f"LAST expects array, got {type_name(arr)}")
        if len(arr) == 0:
            raise RuntimeError("LAST called on empty array")
        return arr[-1]

    def concat(self, arr1: list, arr2: list) -> list:
        """Concatenate two arrays."""
        if not isinstance(arr1, list) or not isinstance(arr2, list):
            raise RuntimeError("CONCAT expects two arrays")
        return arr1 + arr2

    # ============ Map Operations ============

    def keys(self, m: dict) -> list:
        """Get array of map keys."""
        if not isinstance(m, dict):
            raise RuntimeError(f"KEYS expects map, got {type_name(m)}")
        return list(m.keys())

    def values(self, m: dict) -> list:
        """Get array of map values."""
        if not isinstance(m, dict):
            raise RuntimeError(f"VALUES expects map, got {type_name(m)}")
        return list(m.values())

    def has(self, m: dict, key: str) -> bool:
        """Check if map has key."""
        if not isinstance(m, dict):
            raise RuntimeError(f"HAS expects map, got {type_name(m)}")
        return key in m

    def set_(self, m: dict, key: str, value: Any) -> dict:
        """Set key-value pair in map."""
        if not isinstance(m, dict):
            raise RuntimeError(f"SET expects map, got {type_name(m)}")
        result = m.copy()
        result[key] = value
        return result

    def delete(self, m: dict, key: str) -> dict:
        """Remove key from map."""
        if not isinstance(m, dict):
            raise RuntimeError(f"DELETE expects map, got {type_name(m)}")
        result = m.copy()
        result.pop(key, None)
        return result

    # ============ String Operations ============

    def split(self, s: str, delimiter: str) -> list:
        """Split string into array."""
        if not isinstance(s, str) or not isinstance(delimiter, str):
            raise RuntimeError("SPLIT expects two strings")
        if delimiter == "":
            return list(s)
        return s.split(delimiter)

    def join(self, arr: list, delimiter: str) -> str:
        """Join array into string."""
        if not isinstance(arr, list):
            raise RuntimeError(f"JOIN expects array, got {type_name(arr)}")
        if not isinstance(delimiter, str):
            raise RuntimeError(f"JOIN expects string delimiter, got {type_name(delimiter)}")
        return delimiter.join(stringify(v) if not isinstance(v, str) else v for v in arr)

    def substring(self, s: str, start: int, end: int) -> str:
        """Extract substring."""
        if not isinstance(s, str):
            raise RuntimeError(f"SUBSTRING expects string, got {type_name(s)}")
        if not isinstance(start, int) or not isinstance(end, int):
            raise RuntimeError("SUBSTRING expects integer indices")
        return s[start:end]

    def uppercase(self, s: str) -> str:
        """Convert to uppercase."""
        if not isinstance(s, str):
            raise RuntimeError(f"UPPERCASE expects string, got {type_name(s)}")
        return s.upper()

    def lowercase(self, s: str) -> str:
        """Convert to lowercase."""
        if not isinstance(s, str):
            raise RuntimeError(f"LOWERCASE expects string, got {type_name(s)}")
        return s.lower()

    def trim(self, s: str) -> str:
        """Remove leading/trailing whitespace."""
        if not isinstance(s, str):
            raise RuntimeError(f"TRIM expects string, got {type_name(s)}")
        return s.strip()

    def replace(self, s: str, old: str, new: str) -> str:
        """Replace occurrences in string."""
        if not isinstance(s, str) or not isinstance(old, str) or not isinstance(new, str):
            raise RuntimeError("REPLACE expects three strings")
        return s.replace(old, new)

    # ============ Utility ============

    def random(self) -> float:
        """Random float between 0 and 1."""
        return random.random()

    def random_int(self, min_val: int, max_val: int) -> int:
        """Random integer in range (inclusive)."""
        if not isinstance(min_val, int) or not isinstance(max_val, int):
            raise RuntimeError("RANDOM_INT expects two integers")
        return random.randint(min_val, max_val)

    def time(self) -> int:
        """Current Unix timestamp in milliseconds."""
        return int(time.time() * 1000)
