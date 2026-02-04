"""Tests for the !~ATH parser."""

import unittest
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from untildeath.lexer import Lexer
from untildeath.parser import Parser
from untildeath.ast_nodes import *
from untildeath.errors import ParseError


def parse(source: str):
    """Helper to parse source code."""
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    return parser.parse()


class TestParserImport(unittest.TestCase):
    """Test import statement parsing."""

    def test_import_timer_ms(self):
        program = parse("import timer T(100ms);")
        self.assertEqual(len(program.statements), 1)
        stmt = program.statements[0]
        self.assertIsInstance(stmt, ImportStmt)
        self.assertEqual(stmt.entity_type, "timer")
        self.assertEqual(stmt.name, "T")
        self.assertIsInstance(stmt.args[0], Duration)
        self.assertEqual(stmt.args[0].unit, "ms")
        self.assertEqual(stmt.args[0].value, 100)

    def test_import_timer_seconds(self):
        program = parse("import timer delay(5s);")
        stmt = program.statements[0]
        self.assertEqual(stmt.args[0].unit, "s")
        self.assertEqual(stmt.args[0].value, 5)

    def test_import_timer_minutes(self):
        program = parse("import timer wait(2m);")
        stmt = program.statements[0]
        self.assertEqual(stmt.args[0].unit, "m")
        self.assertEqual(stmt.args[0].value, 2)

    def test_import_timer_hours(self):
        program = parse("import timer long(1h);")
        stmt = program.statements[0]
        self.assertEqual(stmt.args[0].unit, "h")
        self.assertEqual(stmt.args[0].value, 1)

    def test_import_timer_no_unit(self):
        """Plain integer defaults to milliseconds."""
        program = parse("import timer T(100);")
        stmt = program.statements[0]
        self.assertEqual(stmt.args[0].unit, "ms")
        self.assertEqual(stmt.args[0].value, 100)

    def test_import_process(self):
        program = parse('import process P("./script.sh");')
        stmt = program.statements[0]
        self.assertEqual(stmt.entity_type, "process")
        self.assertEqual(stmt.name, "P")
        self.assertIsInstance(stmt.args[0], Literal)
        self.assertEqual(stmt.args[0].value, "./script.sh")

    def test_import_process_with_args(self):
        program = parse('import process P("python", "script.py", "--verbose");')
        stmt = program.statements[0]
        self.assertEqual(len(stmt.args), 3)
        self.assertEqual(stmt.args[0].value, "python")
        self.assertEqual(stmt.args[1].value, "script.py")
        self.assertEqual(stmt.args[2].value, "--verbose")

    def test_import_connection(self):
        program = parse('import connection C("localhost", 8080);')
        stmt = program.statements[0]
        self.assertEqual(stmt.entity_type, "connection")
        self.assertEqual(stmt.name, "C")
        self.assertEqual(len(stmt.args), 2)
        self.assertEqual(stmt.args[0].value, "localhost")
        self.assertEqual(stmt.args[1].value, 8080)

    def test_import_watcher(self):
        program = parse('import watcher W("./config.txt");')
        stmt = program.statements[0]
        self.assertEqual(stmt.entity_type, "watcher")
        self.assertEqual(stmt.name, "W")
        self.assertEqual(stmt.args[0].value, "./config.txt")


class TestParserBifurcate(unittest.TestCase):
    """Test bifurcate statement parsing."""

    def test_bifurcate_this(self):
        program = parse("bifurcate THIS[LEFT, RIGHT];")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, BifurcateStmt)
        self.assertEqual(stmt.entity, "THIS")
        self.assertEqual(stmt.branch1, "LEFT")
        self.assertEqual(stmt.branch2, "RIGHT")

    def test_bifurcate_identifier(self):
        program = parse("bifurcate myEntity[A, B];")
        stmt = program.statements[0]
        self.assertEqual(stmt.entity, "myEntity")
        self.assertEqual(stmt.branch1, "A")
        self.assertEqual(stmt.branch2, "B")


class TestParserAthLoop(unittest.TestCase):
    """Test ~ATH loop parsing."""

    def test_simple_ath_loop(self):
        program = parse("~ATH(T) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, AthLoop)
        self.assertIsInstance(stmt.entity_expr, EntityIdent)
        self.assertEqual(stmt.entity_expr.name, "T")
        self.assertEqual(len(stmt.body), 0)

    def test_ath_with_this(self):
        program = parse("~ATH(THIS) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertEqual(stmt.entity_expr.name, "THIS")

    def test_ath_with_and(self):
        program = parse("~ATH(T1 && T2) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.entity_expr, EntityAnd)
        self.assertEqual(stmt.entity_expr.left.name, "T1")
        self.assertEqual(stmt.entity_expr.right.name, "T2")

    def test_ath_with_or(self):
        program = parse("~ATH(T1 || T2) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.entity_expr, EntityOr)

    def test_ath_with_not(self):
        program = parse("~ATH(!T) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.entity_expr, EntityNot)
        self.assertEqual(stmt.entity_expr.operand.name, "T")

    def test_ath_complex_entity_expr(self):
        program = parse("~ATH((T1 && T2) || T3) { } EXECUTE(VOID);")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.entity_expr, EntityOr)
        self.assertIsInstance(stmt.entity_expr.left, EntityAnd)

    def test_ath_with_execute_expression(self):
        program = parse('~ATH(T) { } EXECUTE(UTTER("hello"));')
        stmt = program.statements[0]
        self.assertEqual(len(stmt.execute), 1)
        self.assertIsInstance(stmt.execute[0], ExprStmt)


class TestParserDie(unittest.TestCase):
    """Test DIE statement parsing."""

    def test_this_die(self):
        program = parse("THIS.DIE();")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, DieStmt)
        self.assertIsInstance(stmt.target, DieIdent)
        self.assertEqual(stmt.target.name, "THIS")

    def test_identifier_die(self):
        program = parse("myTimer.DIE();")
        stmt = program.statements[0]
        self.assertEqual(stmt.target.name, "myTimer")

    def test_pair_die(self):
        program = parse("[LEFT, RIGHT].DIE();")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.target, DiePair)
        self.assertEqual(stmt.target.left.name, "LEFT")
        self.assertEqual(stmt.target.right.name, "RIGHT")

    def test_nested_pair_die(self):
        program = parse("[A, [B1, B2]].DIE();")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.target.right, DiePair)


class TestParserVarDecl(unittest.TestCase):
    """Test variable declaration parsing."""

    def test_birth_integer(self):
        program = parse("BIRTH x WITH 5;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, VarDecl)
        self.assertEqual(stmt.name, "x")
        self.assertIsInstance(stmt.value, Literal)
        self.assertEqual(stmt.value.value, 5)

    def test_birth_string(self):
        program = parse('BIRTH name WITH "Karkat";')
        stmt = program.statements[0]
        self.assertEqual(stmt.value.value, "Karkat")

    def test_birth_array(self):
        program = parse("BIRTH arr WITH [1, 2, 3];")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.value, ArrayLiteral)
        self.assertEqual(len(stmt.value.elements), 3)

    def test_birth_map(self):
        program = parse("BIRTH obj WITH {x: 1, y: 2};")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.value, MapLiteral)
        self.assertEqual(len(stmt.value.entries), 2)


class TestParserConstDecl(unittest.TestCase):
    """Test constant declaration parsing."""

    def test_entomb(self):
        program = parse("ENTOMB PI WITH 3.14159;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, ConstDecl)
        self.assertEqual(stmt.name, "PI")
        self.assertAlmostEqual(stmt.value.value, 3.14159)


class TestParserAssignment(unittest.TestCase):
    """Test assignment parsing."""

    def test_simple_assignment(self):
        program = parse("x = 10;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, Assignment)
        self.assertEqual(stmt.target.name, "x")
        self.assertEqual(stmt.value.value, 10)

    def test_index_assignment(self):
        program = parse("arr[0] = 5;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.target, IndexExpr)

    def test_member_assignment(self):
        program = parse("obj.x = 5;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.target, MemberExpr)


class TestParserRiteDef(unittest.TestCase):
    """Test rite (function) definition parsing."""

    def test_simple_rite(self):
        program = parse("RITE greet() { UTTER(\"hello\"); }")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, RiteDef)
        self.assertEqual(stmt.name, "greet")
        self.assertEqual(len(stmt.params), 0)

    def test_rite_with_params(self):
        program = parse("RITE add(a, b) { BEQUEATH a + b; }")
        stmt = program.statements[0]
        self.assertEqual(stmt.params, ["a", "b"])

    def test_rite_with_bequeath(self):
        program = parse("RITE identity(x) { BEQUEATH x; }")
        stmt = program.statements[0]
        self.assertEqual(len(stmt.body), 1)
        self.assertIsInstance(stmt.body[0], BequeathStmt)


class TestParserConditional(unittest.TestCase):
    """Test conditional parsing."""

    def test_should_only(self):
        program = parse("SHOULD ALIVE { UTTER(1); }")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, Conditional)
        self.assertIsNone(stmt.else_branch)

    def test_should_lest(self):
        program = parse("SHOULD ALIVE { UTTER(1); } LEST { UTTER(2); }")
        stmt = program.statements[0]
        self.assertIsNotNone(stmt.else_branch)

    def test_chained_should(self):
        program = parse("SHOULD x == 1 { } LEST SHOULD x == 2 { } LEST { }")
        stmt = program.statements[0]
        self.assertIsInstance(stmt.else_branch[0], Conditional)


class TestParserAttemptSalvage(unittest.TestCase):
    """Test error handling parsing."""

    def test_attempt_salvage(self):
        program = parse('ATTEMPT { UTTER(1); } SALVAGE err { UTTER(err); }')
        stmt = program.statements[0]
        self.assertIsInstance(stmt, AttemptSalvage)
        self.assertEqual(stmt.error_name, "err")


class TestParserCondemn(unittest.TestCase):
    """Test CONDEMN parsing."""

    def test_condemn(self):
        program = parse('CONDEMN "error message";')
        stmt = program.statements[0]
        self.assertIsInstance(stmt, CondemnStmt)
        self.assertEqual(stmt.message.value, "error message")


class TestParserBequeath(unittest.TestCase):
    """Test BEQUEATH parsing."""

    def test_bequeath_with_value(self):
        program = parse("BEQUEATH 42;")
        stmt = program.statements[0]
        self.assertIsInstance(stmt, BequeathStmt)
        self.assertEqual(stmt.value.value, 42)

    def test_bequeath_void(self):
        program = parse("BEQUEATH;")
        stmt = program.statements[0]
        self.assertIsNone(stmt.value)


class TestParserExpressions(unittest.TestCase):
    """Test expression parsing."""

    def test_literal_integer(self):
        """Expressions are tested inside BIRTH statements."""
        program = parse("BIRTH x WITH 42;")
        self.assertIsInstance(program.statements[0].value, Literal)
        self.assertEqual(program.statements[0].value.value, 42)

    def test_literal_float(self):
        program = parse("BIRTH x WITH 3.14;")
        self.assertEqual(program.statements[0].value.value, 3.14)

    def test_literal_string(self):
        program = parse('BIRTH x WITH "hello";')
        self.assertEqual(program.statements[0].value.value, "hello")

    def test_literal_alive(self):
        program = parse("BIRTH x WITH ALIVE;")
        self.assertEqual(program.statements[0].value.value, True)

    def test_literal_dead(self):
        program = parse("BIRTH x WITH DEAD;")
        self.assertEqual(program.statements[0].value.value, False)

    def test_literal_void(self):
        program = parse("BIRTH x WITH VOID;")
        self.assertEqual(program.statements[0].value.value, None)

    def test_identifier(self):
        program = parse("BIRTH x WITH myVar;")
        self.assertIsInstance(program.statements[0].value, Identifier)
        self.assertEqual(program.statements[0].value.name, "myVar")

    def test_binary_add(self):
        program = parse("BIRTH x WITH 1 + 2;")
        expr = program.statements[0].value
        self.assertIsInstance(expr, BinaryOp)
        self.assertEqual(expr.operator, "+")

    def test_binary_subtract(self):
        program = parse("BIRTH x WITH 5 - 3;")
        self.assertEqual(program.statements[0].value.operator, "-")

    def test_binary_multiply(self):
        program = parse("BIRTH x WITH 2 * 3;")
        self.assertEqual(program.statements[0].value.operator, "*")

    def test_binary_divide(self):
        program = parse("BIRTH x WITH 10 / 2;")
        self.assertEqual(program.statements[0].value.operator, "/")

    def test_binary_modulo(self):
        program = parse("BIRTH x WITH 7 % 3;")
        self.assertEqual(program.statements[0].value.operator, "%")

    def test_comparison_eq(self):
        program = parse("BIRTH r WITH x == y;")
        self.assertEqual(program.statements[0].value.operator, "==")

    def test_comparison_ne(self):
        program = parse("BIRTH r WITH x != y;")
        self.assertEqual(program.statements[0].value.operator, "!=")

    def test_comparison_lt(self):
        program = parse("BIRTH r WITH x < y;")
        self.assertEqual(program.statements[0].value.operator, "<")

    def test_comparison_gt(self):
        program = parse("BIRTH r WITH x > y;")
        self.assertEqual(program.statements[0].value.operator, ">")

    def test_comparison_le(self):
        program = parse("BIRTH r WITH x <= y;")
        self.assertEqual(program.statements[0].value.operator, "<=")

    def test_comparison_ge(self):
        program = parse("BIRTH r WITH x >= y;")
        self.assertEqual(program.statements[0].value.operator, ">=")

    def test_logical_and(self):
        program = parse("BIRTH r WITH x AND y;")
        self.assertEqual(program.statements[0].value.operator, "AND")

    def test_logical_or(self):
        program = parse("BIRTH r WITH x OR y;")
        self.assertEqual(program.statements[0].value.operator, "OR")

    def test_unary_not(self):
        program = parse("BIRTH r WITH NOT x;")
        expr = program.statements[0].value
        self.assertIsInstance(expr, UnaryOp)
        self.assertEqual(expr.operator, "NOT")

    def test_unary_minus(self):
        program = parse("BIRTH r WITH -x;")
        self.assertEqual(program.statements[0].value.operator, "-")

    def test_call_no_args(self):
        program = parse("func();")
        expr = program.statements[0].expression
        self.assertIsInstance(expr, CallExpr)
        self.assertEqual(len(expr.args), 0)

    def test_call_with_args(self):
        program = parse("func(1, 2, 3);")
        expr = program.statements[0].expression
        self.assertEqual(len(expr.args), 3)

    def test_index_expr(self):
        program = parse("BIRTH x WITH arr[0];")
        self.assertIsInstance(program.statements[0].value, IndexExpr)

    def test_member_expr(self):
        program = parse("BIRTH x WITH obj.field;")
        expr = program.statements[0].value
        self.assertIsInstance(expr, MemberExpr)
        self.assertEqual(expr.member, "field")

    def test_array_literal_empty(self):
        program = parse("BIRTH x WITH [];")
        self.assertIsInstance(program.statements[0].value, ArrayLiteral)
        self.assertEqual(len(program.statements[0].value.elements), 0)

    def test_array_literal(self):
        program = parse("BIRTH x WITH [1, 2, 3];")
        self.assertEqual(len(program.statements[0].value.elements), 3)

    def test_map_literal_empty(self):
        program = parse("BIRTH x WITH {};")
        self.assertIsInstance(program.statements[0].value, MapLiteral)
        self.assertEqual(len(program.statements[0].value.entries), 0)

    def test_map_literal(self):
        program = parse("BIRTH x WITH {x: 1, y: 2};")
        self.assertEqual(len(program.statements[0].value.entries), 2)

    def test_map_literal_string_keys(self):
        program = parse('BIRTH x WITH {"key": 1};')
        self.assertEqual(program.statements[0].value.entries[0][0], "key")

    def test_grouped_expression(self):
        program = parse("BIRTH x WITH (1 + 2) * 3;")
        expr = program.statements[0].value
        self.assertEqual(expr.operator, "*")
        self.assertEqual(expr.left.operator, "+")


class TestParserPrecedence(unittest.TestCase):
    """Test operator precedence."""

    def test_multiply_before_add(self):
        program = parse("BIRTH x WITH 1 + 2 * 3;")
        expr = program.statements[0].value
        # Should be 1 + (2 * 3)
        self.assertEqual(expr.operator, "+")
        self.assertEqual(expr.right.operator, "*")

    def test_divide_before_subtract(self):
        program = parse("BIRTH x WITH 6 - 4 / 2;")
        expr = program.statements[0].value
        # Should be 6 - (4 / 2)
        self.assertEqual(expr.operator, "-")
        self.assertEqual(expr.right.operator, "/")

    def test_comparison_before_logical(self):
        program = parse("BIRTH x WITH x < 5 AND y > 3;")
        expr = program.statements[0].value
        # Should be (x < 5) AND (y > 3)
        self.assertEqual(expr.operator, "AND")

    def test_and_before_or(self):
        program = parse("BIRTH x WITH a OR b AND c;")
        expr = program.statements[0].value
        # Should be a OR (b AND c)
        self.assertEqual(expr.operator, "OR")
        self.assertEqual(expr.right.operator, "AND")


class TestParserErrors(unittest.TestCase):
    """Test parser error handling."""

    def test_missing_semicolon(self):
        with self.assertRaises(ParseError):
            parse("BIRTH x WITH 5")

    def test_missing_paren(self):
        with self.assertRaises(ParseError):
            parse("~ATH(T { } EXECUTE(VOID);")

    def test_missing_execute(self):
        """~ATH without EXECUTE should fail."""
        with self.assertRaises(ParseError):
            parse("~ATH(T) { }")


if __name__ == '__main__':
    unittest.main()
