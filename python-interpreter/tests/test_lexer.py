"""Tests for the !~ATH lexer."""

import unittest
import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from untildeath.lexer import Lexer, TokenType
from untildeath.errors import LexerError


class TestLexerBasics(unittest.TestCase):
    """Test basic lexer functionality."""

    def test_empty_source(self):
        """Empty source produces only EOF."""
        lexer = Lexer("")
        tokens = lexer.tokenize()
        self.assertEqual(len(tokens), 1)
        self.assertEqual(tokens[0].type, TokenType.EOF)

    def test_whitespace_only(self):
        """Whitespace-only source produces only EOF."""
        lexer = Lexer("   \t\n\r  ")
        tokens = lexer.tokenize()
        self.assertEqual(len(tokens), 1)
        self.assertEqual(tokens[0].type, TokenType.EOF)

    def test_single_line_comment(self):
        """Comments are ignored."""
        lexer = Lexer("// this is a comment")
        tokens = lexer.tokenize()
        self.assertEqual(len(tokens), 1)
        self.assertEqual(tokens[0].type, TokenType.EOF)

    def test_comment_with_code(self):
        """Comments don't consume following lines."""
        lexer = Lexer("// comment\n42")
        tokens = lexer.tokenize()
        self.assertEqual(len(tokens), 2)
        self.assertEqual(tokens[0].type, TokenType.INTEGER)
        self.assertEqual(tokens[0].value, 42)


class TestLexerIntegers(unittest.TestCase):
    """Test integer literal tokenization."""

    def test_simple_integer(self):
        """Simple positive integer."""
        lexer = Lexer("42")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.INTEGER)
        self.assertEqual(tokens[0].value, 42)

    def test_zero(self):
        """Zero is a valid integer."""
        lexer = Lexer("0")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.INTEGER)
        self.assertEqual(tokens[0].value, 0)

    def test_negative_integer(self):
        """Negative integer."""
        lexer = Lexer("-7")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.INTEGER)
        self.assertEqual(tokens[0].value, -7)

    def test_large_integer(self):
        """Large integer."""
        lexer = Lexer("1234567890")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.INTEGER)
        self.assertEqual(tokens[0].value, 1234567890)


class TestLexerFloats(unittest.TestCase):
    """Test float literal tokenization."""

    def test_simple_float(self):
        """Simple float."""
        lexer = Lexer("3.14")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.FLOAT)
        self.assertAlmostEqual(tokens[0].value, 3.14)

    def test_negative_float(self):
        """Negative float."""
        lexer = Lexer("-0.5")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.FLOAT)
        self.assertAlmostEqual(tokens[0].value, -0.5)

    def test_zero_float(self):
        """Zero as float."""
        lexer = Lexer("0.0")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.FLOAT)
        self.assertEqual(tokens[0].value, 0.0)

    def test_float_leading_zero(self):
        """Float with leading zero."""
        lexer = Lexer("0.123")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.FLOAT)
        self.assertAlmostEqual(tokens[0].value, 0.123)


class TestLexerDurations(unittest.TestCase):
    """Test duration literal tokenization."""

    def test_milliseconds(self):
        """Milliseconds duration."""
        lexer = Lexer("100ms")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DURATION)
        self.assertEqual(tokens[0].value, ('ms', 100))

    def test_seconds(self):
        """Seconds duration."""
        lexer = Lexer("5s")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DURATION)
        self.assertEqual(tokens[0].value, ('s', 5))

    def test_minutes(self):
        """Minutes duration."""
        lexer = Lexer("2m")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DURATION)
        self.assertEqual(tokens[0].value, ('m', 2))

    def test_hours(self):
        """Hours duration."""
        lexer = Lexer("1h")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DURATION)
        self.assertEqual(tokens[0].value, ('h', 1))


class TestLexerStrings(unittest.TestCase):
    """Test string literal tokenization."""

    def test_simple_string(self):
        """Simple string."""
        lexer = Lexer('"hello"')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.STRING)
        self.assertEqual(tokens[0].value, "hello")

    def test_empty_string(self):
        """Empty string."""
        lexer = Lexer('""')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.STRING)
        self.assertEqual(tokens[0].value, "")

    def test_string_with_spaces(self):
        """String with spaces."""
        lexer = Lexer('"hello world"')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.STRING)
        self.assertEqual(tokens[0].value, "hello world")

    def test_escape_newline(self):
        """Escape sequence: newline."""
        lexer = Lexer(r'"line1\nline2"')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].value, "line1\nline2")

    def test_escape_tab(self):
        """Escape sequence: tab."""
        lexer = Lexer(r'"col1\tcol2"')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].value, "col1\tcol2")

    def test_escape_backslash(self):
        """Escape sequence: backslash."""
        lexer = Lexer(r'"path\\file"')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].value, "path\\file")

    def test_escape_quote(self):
        """Escape sequence: quote."""
        lexer = Lexer(r'"say \"hello\""')
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].value, 'say "hello"')

    def test_unterminated_string(self):
        """Unterminated string raises error."""
        lexer = Lexer('"hello')
        with self.assertRaises(LexerError):
            lexer.tokenize()


class TestLexerKeywords(unittest.TestCase):
    """Test keyword tokenization."""

    def test_import(self):
        lexer = Lexer("import")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.IMPORT)

    def test_bifurcate(self):
        lexer = Lexer("bifurcate")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.BIFURCATE)

    def test_execute(self):
        lexer = Lexer("EXECUTE")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.EXECUTE)

    def test_die(self):
        lexer = Lexer("DIE")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DIE)

    def test_this(self):
        lexer = Lexer("THIS")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.THIS)

    def test_tilde_ath(self):
        lexer = Lexer("~ATH")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.TILDE_ATH)

    def test_entity_types(self):
        """Entity type keywords."""
        for keyword, expected in [
            ("timer", TokenType.TIMER),
            ("process", TokenType.PROCESS),
            ("connection", TokenType.CONNECTION),
            ("watcher", TokenType.WATCHER),
        ]:
            with self.subTest(keyword=keyword):
                lexer = Lexer(keyword)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_expression_keywords(self):
        """Expression language keywords."""
        keywords = [
            ("BIRTH", TokenType.BIRTH),
            ("ENTOMB", TokenType.ENTOMB),
            ("WITH", TokenType.WITH),
            ("SHOULD", TokenType.SHOULD),
            ("LEST", TokenType.LEST),
            ("RITE", TokenType.RITE),
            ("BEQUEATH", TokenType.BEQUEATH),
            ("ATTEMPT", TokenType.ATTEMPT),
            ("SALVAGE", TokenType.SALVAGE),
            ("CONDEMN", TokenType.CONDEMN),
            ("AND", TokenType.AND),
            ("OR", TokenType.OR),
            ("NOT", TokenType.NOT),
        ]
        for keyword, expected in keywords:
            with self.subTest(keyword=keyword):
                lexer = Lexer(keyword)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_boolean_alive(self):
        lexer = Lexer("ALIVE")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.ALIVE)
        self.assertEqual(tokens[0].value, True)

    def test_boolean_dead(self):
        lexer = Lexer("DEAD")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.DEAD)
        self.assertEqual(tokens[0].value, False)

    def test_void(self):
        lexer = Lexer("VOID")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.VOID)
        self.assertEqual(tokens[0].value, None)


class TestLexerIdentifiers(unittest.TestCase):
    """Test identifier tokenization."""

    def test_simple_identifier(self):
        lexer = Lexer("myVar")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.IDENTIFIER)
        self.assertEqual(tokens[0].value, "myVar")

    def test_identifier_with_underscore(self):
        lexer = Lexer("_private")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.IDENTIFIER)
        self.assertEqual(tokens[0].value, "_private")

    def test_identifier_with_numbers(self):
        lexer = Lexer("timer2")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.IDENTIFIER)
        self.assertEqual(tokens[0].value, "timer2")

    def test_case_sensitivity(self):
        """Identifiers are case-sensitive."""
        lexer = Lexer("this THIS This")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.IDENTIFIER)  # lowercase 'this'
        self.assertEqual(tokens[1].type, TokenType.THIS)  # uppercase 'THIS'
        self.assertEqual(tokens[2].type, TokenType.IDENTIFIER)  # 'This'


class TestLexerOperators(unittest.TestCase):
    """Test operator tokenization."""

    def test_arithmetic_operators(self):
        operators = [
            ("+", TokenType.PLUS),
            ("-", TokenType.MINUS),
            ("*", TokenType.STAR),
            ("/", TokenType.SLASH),
            ("%", TokenType.PERCENT),
        ]
        for op, expected in operators:
            with self.subTest(operator=op):
                lexer = Lexer(op)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_comparison_operators(self):
        operators = [
            ("==", TokenType.EQ),
            ("!=", TokenType.NE),
            ("<", TokenType.LT),
            (">", TokenType.GT),
            ("<=", TokenType.LE),
            (">=", TokenType.GE),
        ]
        for op, expected in operators:
            with self.subTest(operator=op):
                lexer = Lexer(op)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_entity_operators(self):
        operators = [
            ("&&", TokenType.AMPAMP),
            ("||", TokenType.PIPEPIPE),
            ("!", TokenType.BANG),
        ]
        for op, expected in operators:
            with self.subTest(operator=op):
                lexer = Lexer(op)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_bitwise_operators(self):
        operators = [
            ("&", TokenType.AMP),
            ("|", TokenType.PIPE),
            ("^", TokenType.CARET),
            ("~", TokenType.TILDE),
            ("<<", TokenType.LSHIFT),
            (">>", TokenType.RSHIFT),
        ]
        for op, expected in operators:
            with self.subTest(operator=op):
                lexer = Lexer(op)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)

    def test_assignment(self):
        lexer = Lexer("=")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].type, TokenType.ASSIGN)


class TestLexerPunctuation(unittest.TestCase):
    """Test punctuation tokenization."""

    def test_punctuation(self):
        punctuation = [
            ("(", TokenType.LPAREN),
            (")", TokenType.RPAREN),
            ("{", TokenType.LBRACE),
            ("}", TokenType.RBRACE),
            ("[", TokenType.LBRACKET),
            ("]", TokenType.RBRACKET),
            (";", TokenType.SEMICOLON),
            (",", TokenType.COMMA),
            (".", TokenType.DOT),
            (":", TokenType.COLON),
        ]
        for punct, expected in punctuation:
            with self.subTest(punctuation=punct):
                lexer = Lexer(punct)
                tokens = lexer.tokenize()
                self.assertEqual(tokens[0].type, expected)


class TestLexerLineColumn(unittest.TestCase):
    """Test line and column tracking."""

    def test_first_token_position(self):
        lexer = Lexer("hello")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].line, 1)
        self.assertEqual(tokens[0].column, 1)

    def test_second_line(self):
        lexer = Lexer("hello\nworld")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[1].line, 2)
        self.assertEqual(tokens[1].column, 1)

    def test_column_tracking(self):
        lexer = Lexer("a b c")
        tokens = lexer.tokenize()
        self.assertEqual(tokens[0].column, 1)
        self.assertEqual(tokens[1].column, 3)
        self.assertEqual(tokens[2].column, 5)


class TestLexerComplexExamples(unittest.TestCase):
    """Test complex tokenization scenarios."""

    def test_import_statement(self):
        lexer = Lexer("import timer T(100ms);")
        tokens = lexer.tokenize()
        types = [t.type for t in tokens[:-1]]  # Exclude EOF
        expected = [
            TokenType.IMPORT, TokenType.TIMER, TokenType.IDENTIFIER,
            TokenType.LPAREN, TokenType.DURATION, TokenType.RPAREN,
            TokenType.SEMICOLON
        ]
        self.assertEqual(types, expected)

    def test_ath_loop(self):
        lexer = Lexer("~ATH(T) { } EXECUTE(VOID);")
        tokens = lexer.tokenize()
        types = [t.type for t in tokens[:-1]]
        expected = [
            TokenType.TILDE_ATH, TokenType.LPAREN, TokenType.IDENTIFIER,
            TokenType.RPAREN, TokenType.LBRACE, TokenType.RBRACE,
            TokenType.EXECUTE, TokenType.LPAREN, TokenType.VOID,
            TokenType.RPAREN, TokenType.SEMICOLON
        ]
        self.assertEqual(types, expected)

    def test_bifurcate_statement(self):
        lexer = Lexer("bifurcate THIS[LEFT, RIGHT];")
        tokens = lexer.tokenize()
        types = [t.type for t in tokens[:-1]]
        expected = [
            TokenType.BIFURCATE, TokenType.THIS, TokenType.LBRACKET,
            TokenType.IDENTIFIER, TokenType.COMMA, TokenType.IDENTIFIER,
            TokenType.RBRACKET, TokenType.SEMICOLON
        ]
        self.assertEqual(types, expected)


if __name__ == '__main__':
    unittest.main()
