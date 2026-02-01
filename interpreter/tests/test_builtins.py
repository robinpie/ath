"""Tests for the !~ATH built-in rites."""

import unittest
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from untildeath.builtins import Builtins, stringify, type_name, is_truthy
from untildeath.errors import RuntimeError


class TestStringify(unittest.TestCase):
    """Test the stringify helper function."""

    def test_stringify_none(self):
        self.assertEqual(stringify(None), "VOID")

    def test_stringify_true(self):
        self.assertEqual(stringify(True), "ALIVE")

    def test_stringify_false(self):
        self.assertEqual(stringify(False), "DEAD")

    def test_stringify_int(self):
        self.assertEqual(stringify(42), "42")

    def test_stringify_float(self):
        self.assertEqual(stringify(3.14), "3.14")

    def test_stringify_string(self):
        self.assertEqual(stringify("hello"), "hello")

    def test_stringify_empty_array(self):
        self.assertEqual(stringify([]), "[]")

    def test_stringify_array(self):
        self.assertEqual(stringify([1, 2, 3]), "[1, 2, 3]")

    def test_stringify_nested_array(self):
        self.assertEqual(stringify([[1, 2], [3, 4]]), "[[1, 2], [3, 4]]")

    def test_stringify_empty_map(self):
        self.assertEqual(stringify({}), "{}")

    def test_stringify_map(self):
        result = stringify({"a": 1, "b": 2})
        self.assertIn("a: 1", result)
        self.assertIn("b: 2", result)

    def test_stringify_mixed_array(self):
        result = stringify([1, "two", True, None])
        self.assertEqual(result, "[1, two, ALIVE, VOID]")


class TestTypeName(unittest.TestCase):
    """Test the type_name helper function."""

    def test_type_name_void(self):
        self.assertEqual(type_name(None), "VOID")

    def test_type_name_boolean(self):
        self.assertEqual(type_name(True), "BOOLEAN")
        self.assertEqual(type_name(False), "BOOLEAN")

    def test_type_name_integer(self):
        self.assertEqual(type_name(42), "INTEGER")

    def test_type_name_float(self):
        self.assertEqual(type_name(3.14), "FLOAT")

    def test_type_name_string(self):
        self.assertEqual(type_name("hello"), "STRING")

    def test_type_name_array(self):
        self.assertEqual(type_name([1, 2, 3]), "ARRAY")

    def test_type_name_map(self):
        self.assertEqual(type_name({"a": 1}), "MAP")


class TestIsTruthy(unittest.TestCase):
    """Test the is_truthy helper function."""

    def test_truthy_true(self):
        self.assertTrue(is_truthy(True))

    def test_truthy_false(self):
        self.assertFalse(is_truthy(False))

    def test_truthy_none(self):
        self.assertFalse(is_truthy(None))

    def test_truthy_zero(self):
        self.assertFalse(is_truthy(0))

    def test_truthy_nonzero(self):
        self.assertTrue(is_truthy(1))
        self.assertTrue(is_truthy(-1))

    def test_truthy_empty_string(self):
        self.assertFalse(is_truthy(""))

    def test_truthy_nonempty_string(self):
        self.assertTrue(is_truthy("hello"))

    def test_truthy_empty_array(self):
        self.assertFalse(is_truthy([]))

    def test_truthy_nonempty_array(self):
        self.assertTrue(is_truthy([1]))

    def test_truthy_empty_map(self):
        self.assertFalse(is_truthy({}))

    def test_truthy_nonempty_map(self):
        self.assertTrue(is_truthy({"a": 1}))


class TestBuiltinTypeOperations(unittest.TestCase):
    """Test built-in type operations."""

    def setUp(self):
        self.builtins = Builtins(None)

    def test_typeof(self):
        self.assertEqual(self.builtins.typeof(42), "INTEGER")
        self.assertEqual(self.builtins.typeof(3.14), "FLOAT")
        self.assertEqual(self.builtins.typeof("hello"), "STRING")
        self.assertEqual(self.builtins.typeof(True), "BOOLEAN")
        self.assertEqual(self.builtins.typeof(None), "VOID")
        self.assertEqual(self.builtins.typeof([1, 2]), "ARRAY")
        self.assertEqual(self.builtins.typeof({"a": 1}), "MAP")

    def test_length_string(self):
        self.assertEqual(self.builtins.length("hello"), 5)
        self.assertEqual(self.builtins.length(""), 0)

    def test_length_array(self):
        self.assertEqual(self.builtins.length([1, 2, 3]), 3)
        self.assertEqual(self.builtins.length([]), 0)

    def test_length_invalid(self):
        with self.assertRaises(RuntimeError):
            self.builtins.length(42)

    def test_parse_int(self):
        self.assertEqual(self.builtins.parse_int("42"), 42)
        self.assertEqual(self.builtins.parse_int("-7"), -7)

    def test_parse_int_invalid(self):
        with self.assertRaises(RuntimeError):
            self.builtins.parse_int("not a number")
        with self.assertRaises(RuntimeError):
            self.builtins.parse_int("3.14")

    def test_parse_float(self):
        self.assertAlmostEqual(self.builtins.parse_float("3.14"), 3.14)
        self.assertAlmostEqual(self.builtins.parse_float("42"), 42.0)

    def test_parse_float_invalid(self):
        with self.assertRaises(RuntimeError):
            self.builtins.parse_float("not a number")

    def test_string_conversion(self):
        self.assertEqual(self.builtins.string(42), "42")
        self.assertEqual(self.builtins.string(True), "ALIVE")
        self.assertEqual(self.builtins.string([1, 2]), "[1, 2]")

    def test_int_conversion(self):
        self.assertEqual(self.builtins.int_(3.7), 3)
        self.assertEqual(self.builtins.int_(-2.9), -2)
        self.assertEqual(self.builtins.int_(42), 42)

    def test_int_conversion_invalid(self):
        with self.assertRaises(RuntimeError):
            self.builtins.int_("not a number")

    def test_float_conversion(self):
        self.assertEqual(self.builtins.float_(42), 42.0)
        self.assertEqual(self.builtins.float_(3.14), 3.14)


class TestBuiltinArrayOperations(unittest.TestCase):
    """Test built-in array operations."""

    def setUp(self):
        self.builtins = Builtins(None)

    def test_append(self):
        result = self.builtins.append([1, 2], 3)
        self.assertEqual(result, [1, 2, 3])

    def test_append_immutable(self):
        original = [1, 2]
        result = self.builtins.append(original, 3)
        self.assertEqual(original, [1, 2])  # Original unchanged

    def test_prepend(self):
        result = self.builtins.prepend([2, 3], 1)
        self.assertEqual(result, [1, 2, 3])

    def test_slice(self):
        result = self.builtins.slice([1, 2, 3, 4, 5], 1, 4)
        self.assertEqual(result, [2, 3, 4])

    def test_slice_full(self):
        result = self.builtins.slice([1, 2, 3], 0, 3)
        self.assertEqual(result, [1, 2, 3])

    def test_first(self):
        self.assertEqual(self.builtins.first([10, 20, 30]), 10)

    def test_first_empty(self):
        with self.assertRaises(RuntimeError):
            self.builtins.first([])

    def test_last(self):
        self.assertEqual(self.builtins.last([10, 20, 30]), 30)

    def test_last_empty(self):
        with self.assertRaises(RuntimeError):
            self.builtins.last([])

    def test_concat(self):
        result = self.builtins.concat([1, 2], [3, 4])
        self.assertEqual(result, [1, 2, 3, 4])

    def test_concat_empty(self):
        result = self.builtins.concat([], [1, 2])
        self.assertEqual(result, [1, 2])


class TestBuiltinMapOperations(unittest.TestCase):
    """Test built-in map operations."""

    def setUp(self):
        self.builtins = Builtins(None)

    def test_keys(self):
        result = self.builtins.keys({"a": 1, "b": 2})
        self.assertEqual(set(result), {"a", "b"})

    def test_keys_empty(self):
        result = self.builtins.keys({})
        self.assertEqual(result, [])

    def test_values(self):
        result = self.builtins.values({"a": 1, "b": 2})
        self.assertEqual(set(result), {1, 2})

    def test_has_true(self):
        self.assertTrue(self.builtins.has({"a": 1}, "a"))

    def test_has_false(self):
        self.assertFalse(self.builtins.has({"a": 1}, "b"))

    def test_set(self):
        result = self.builtins.set_({"a": 1}, "b", 2)
        self.assertEqual(result, {"a": 1, "b": 2})

    def test_set_overwrite(self):
        result = self.builtins.set_({"a": 1}, "a", 99)
        self.assertEqual(result, {"a": 99})

    def test_set_immutable(self):
        original = {"a": 1}
        result = self.builtins.set_(original, "b", 2)
        self.assertEqual(original, {"a": 1})

    def test_delete(self):
        result = self.builtins.delete({"a": 1, "b": 2}, "a")
        self.assertEqual(result, {"b": 2})

    def test_delete_nonexistent(self):
        result = self.builtins.delete({"a": 1}, "b")
        self.assertEqual(result, {"a": 1})


class TestBuiltinStringOperations(unittest.TestCase):
    """Test built-in string operations."""

    def setUp(self):
        self.builtins = Builtins(None)

    def test_split(self):
        result = self.builtins.split("a,b,c", ",")
        self.assertEqual(result, ["a", "b", "c"])

    def test_split_empty_delimiter(self):
        result = self.builtins.split("hello", "")
        self.assertEqual(result, ["h", "e", "l", "l", "o"])

    def test_join(self):
        result = self.builtins.join(["a", "b", "c"], ",")
        self.assertEqual(result, "a,b,c")

    def test_join_empty(self):
        result = self.builtins.join([], ",")
        self.assertEqual(result, "")

    def test_substring(self):
        result = self.builtins.substring("hello", 1, 4)
        self.assertEqual(result, "ell")

    def test_uppercase(self):
        result = self.builtins.uppercase("hello")
        self.assertEqual(result, "HELLO")

    def test_lowercase(self):
        result = self.builtins.lowercase("HELLO")
        self.assertEqual(result, "hello")

    def test_trim(self):
        result = self.builtins.trim("  hello  ")
        self.assertEqual(result, "hello")

    def test_trim_tabs_newlines(self):
        result = self.builtins.trim("\t\nhello\t\n")
        self.assertEqual(result, "hello")

    def test_replace(self):
        result = self.builtins.replace("hello", "l", "w")
        self.assertEqual(result, "hewwo")

    def test_replace_no_match(self):
        result = self.builtins.replace("hello", "x", "y")
        self.assertEqual(result, "hello")


class TestBuiltinUtility(unittest.TestCase):
    """Test built-in utility functions."""

    def setUp(self):
        self.builtins = Builtins(None)

    def test_random_range(self):
        for _ in range(100):
            result = self.builtins.random()
            self.assertGreaterEqual(result, 0)
            self.assertLess(result, 1)

    def test_random_int_range(self):
        for _ in range(100):
            result = self.builtins.random_int(1, 6)
            self.assertGreaterEqual(result, 1)
            self.assertLessEqual(result, 6)

    def test_random_int_same(self):
        result = self.builtins.random_int(5, 5)
        self.assertEqual(result, 5)

    def test_time_positive(self):
        result = self.builtins.time()
        self.assertGreater(result, 0)

    def test_time_is_int(self):
        result = self.builtins.time()
        self.assertIsInstance(result, int)


if __name__ == '__main__':
    unittest.main()
