"""Lexer for the !~ATH language."""

import re
from dataclasses import dataclass
from enum import Enum, auto
from typing import List, Optional

from .errors import LexerError


class TokenType(Enum):
    # Literals
    INTEGER = auto()
    FLOAT = auto()
    STRING = auto()

    # Keywords - ATH constructs
    IMPORT = auto()
    BIFURCATE = auto()
    EXECUTE = auto()
    DIE = auto()
    THIS = auto()
    TILDE_ATH = auto()  # ~ATH

    # Entity types
    TIMER = auto()
    PROCESS = auto()
    CONNECTION = auto()
    WATCHER = auto()

    # Expression keywords
    BIRTH = auto()
    ENTOMB = auto()
    WITH = auto()
    ALIVE = auto()
    DEAD = auto()
    VOID = auto()
    SHOULD = auto()
    LEST = auto()
    RITE = auto()
    BEQUEATH = auto()
    ATTEMPT = auto()
    SALVAGE = auto()
    CONDEMN = auto()
    AND = auto()
    OR = auto()
    NOT = auto()

    # Operators
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()
    EQ = auto()
    NE = auto()
    LT = auto()
    GT = auto()
    LE = auto()
    GE = auto()
    ASSIGN = auto()

    # Entity operators (only valid in entity expressions)
    AMPAMP = auto()  # &&
    PIPEPIPE = auto()  # ||
    BANG = auto()  # !

    # Punctuation
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    SEMICOLON = auto()
    COMMA = auto()
    DOT = auto()
    COLON = auto()

    # Duration suffixes (attached to integers)
    DURATION = auto()  # e.g., 100ms, 5s, 2m, 1h

    # Identifier
    IDENTIFIER = auto()

    # End of file
    EOF = auto()


@dataclass
class Token:
    type: TokenType
    value: any
    line: int
    column: int

    def __repr__(self):
        return f"Token({self.type.name}, {self.value!r}, {self.line}:{self.column})"


KEYWORDS = {
    'import': TokenType.IMPORT,
    'bifurcate': TokenType.BIFURCATE,
    'EXECUTE': TokenType.EXECUTE,
    'DIE': TokenType.DIE,
    'THIS': TokenType.THIS,
    'timer': TokenType.TIMER,
    'process': TokenType.PROCESS,
    'connection': TokenType.CONNECTION,
    'watcher': TokenType.WATCHER,
    'BIRTH': TokenType.BIRTH,
    'ENTOMB': TokenType.ENTOMB,
    'WITH': TokenType.WITH,
    'ALIVE': TokenType.ALIVE,
    'DEAD': TokenType.DEAD,
    'VOID': TokenType.VOID,
    'SHOULD': TokenType.SHOULD,
    'LEST': TokenType.LEST,
    'RITE': TokenType.RITE,
    'BEQUEATH': TokenType.BEQUEATH,
    'ATTEMPT': TokenType.ATTEMPT,
    'SALVAGE': TokenType.SALVAGE,
    'CONDEMN': TokenType.CONDEMN,
    'AND': TokenType.AND,
    'OR': TokenType.OR,
    'NOT': TokenType.NOT,
}


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens: List[Token] = []

    def error(self, message: str) -> LexerError:
        return LexerError(message, self.line, self.column)

    def peek(self, offset: int = 0) -> Optional[str]:
        pos = self.pos + offset
        if pos >= len(self.source):
            return None
        return self.source[pos]

    def advance(self) -> Optional[str]:
        if self.pos >= len(self.source):
            return None
        ch = self.source[self.pos]
        self.pos += 1
        if ch == '\n':
            self.line += 1
            self.column = 1
        else:
            self.column += 1
        return ch

    def skip_whitespace_and_comments(self):
        while self.pos < len(self.source):
            ch = self.peek()
            if ch in ' \t\r\n':
                self.advance()
            elif ch == '/' and self.peek(1) == '/':
                # Single-line comment
                while self.peek() is not None and self.peek() != '\n':
                    self.advance()
            else:
                break

    def read_string(self) -> str:
        start_line, start_col = self.line, self.column
        self.advance()  # consume opening quote
        result = []

        while True:
            ch = self.peek()
            if ch is None:
                raise self.error("Unterminated string")
            if ch == '"':
                self.advance()
                break
            if ch == '\\':
                self.advance()
                escape = self.peek()
                if escape is None:
                    raise self.error("Unterminated string")
                if escape == 'n':
                    result.append('\n')
                elif escape == 't':
                    result.append('\t')
                elif escape == '\\':
                    result.append('\\')
                elif escape == '"':
                    result.append('"')
                else:
                    raise self.error(f"Unknown escape sequence: \\{escape}")
                self.advance()
            else:
                result.append(ch)
                self.advance()

        return ''.join(result)

    def read_number(self) -> Token:
        start_line, start_col = self.line, self.column
        result = []

        # Handle negative sign
        if self.peek() == '-':
            result.append(self.advance())

        # Read digits
        while self.peek() is not None and self.peek().isdigit():
            result.append(self.advance())

        # Check for float
        if self.peek() == '.' and self.peek(1) is not None and self.peek(1).isdigit():
            result.append(self.advance())  # consume '.'
            while self.peek() is not None and self.peek().isdigit():
                result.append(self.advance())
            return Token(TokenType.FLOAT, float(''.join(result)), start_line, start_col)

        value = int(''.join(result))

        # Check for duration suffix
        if self.peek() in ('m', 's', 'h'):
            suffix_start = self.pos
            suffix = self.advance()
            if suffix == 'm' and self.peek() == 's':
                self.advance()
                return Token(TokenType.DURATION, ('ms', value), start_line, start_col)
            elif suffix == 's':
                return Token(TokenType.DURATION, ('s', value), start_line, start_col)
            elif suffix == 'm':
                return Token(TokenType.DURATION, ('m', value), start_line, start_col)
            elif suffix == 'h':
                return Token(TokenType.DURATION, ('h', value), start_line, start_col)

        return Token(TokenType.INTEGER, value, start_line, start_col)

    def read_identifier(self) -> Token:
        start_line, start_col = self.line, self.column
        result = []

        while self.peek() is not None and (self.peek().isalnum() or self.peek() == '_'):
            result.append(self.advance())

        name = ''.join(result)
        token_type = KEYWORDS.get(name, TokenType.IDENTIFIER)

        # Special value handling
        if token_type == TokenType.ALIVE:
            return Token(token_type, True, start_line, start_col)
        elif token_type == TokenType.DEAD:
            return Token(token_type, False, start_line, start_col)
        elif token_type == TokenType.VOID:
            return Token(token_type, None, start_line, start_col)

        return Token(token_type, name, start_line, start_col)

    def tokenize(self) -> List[Token]:
        self.tokens = []

        while self.pos < len(self.source):
            self.skip_whitespace_and_comments()

            if self.pos >= len(self.source):
                break

            start_line, start_col = self.line, self.column
            ch = self.peek()

            # ~ATH special token
            if ch == '~' and self.peek(1) == 'A' and self.peek(2) == 'T' and self.peek(3) == 'H':
                for _ in range(4):
                    self.advance()
                self.tokens.append(Token(TokenType.TILDE_ATH, '~ATH', start_line, start_col))
                continue

            # String
            if ch == '"':
                value = self.read_string()
                self.tokens.append(Token(TokenType.STRING, value, start_line, start_col))
                continue

            # Number (including negative)
            is_negative = False
            if ch == '-' and self.peek(1) is not None and self.peek(1).isdigit():
                # Check if this is unary minus (part of number) or binary minus (subtraction)
                # If preceded by an expression terminator, it's subtraction (not negative number)
                is_subtraction = False
                if self.tokens:
                    last = self.tokens[-1].type
                    if last in (
                        TokenType.IDENTIFIER,
                        TokenType.INTEGER, TokenType.FLOAT, TokenType.STRING, TokenType.DURATION,
                        TokenType.ALIVE, TokenType.DEAD, TokenType.VOID,
                        TokenType.RPAREN, TokenType.RBRACKET
                    ):
                        is_subtraction = True
                
                if not is_subtraction:
                    is_negative = True

            if ch.isdigit() or is_negative:
                self.tokens.append(self.read_number())
                continue

            # Identifier or keyword
            if ch.isalpha() or ch == '_':
                self.tokens.append(self.read_identifier())
                continue

            # Two-character operators
            if ch == '&' and self.peek(1) == '&':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.AMPAMP, '&&', start_line, start_col))
                continue

            if ch == '|' and self.peek(1) == '|':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.PIPEPIPE, '||', start_line, start_col))
                continue

            if ch == '=' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.EQ, '==', start_line, start_col))
                continue

            if ch == '!' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.NE, '!=', start_line, start_col))
                continue

            if ch == '<' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.LE, '<=', start_line, start_col))
                continue

            if ch == '>' and self.peek(1) == '=':
                self.advance()
                self.advance()
                self.tokens.append(Token(TokenType.GE, '>=', start_line, start_col))
                continue

            # Single-character tokens
            single_char_tokens = {
                '+': TokenType.PLUS,
                '-': TokenType.MINUS,
                '*': TokenType.STAR,
                '/': TokenType.SLASH,
                '%': TokenType.PERCENT,
                '<': TokenType.LT,
                '>': TokenType.GT,
                '=': TokenType.ASSIGN,
                '!': TokenType.BANG,
                '(': TokenType.LPAREN,
                ')': TokenType.RPAREN,
                '{': TokenType.LBRACE,
                '}': TokenType.RBRACE,
                '[': TokenType.LBRACKET,
                ']': TokenType.RBRACKET,
                ';': TokenType.SEMICOLON,
                ',': TokenType.COMMA,
                '.': TokenType.DOT,
                ':': TokenType.COLON,
            }

            if ch in single_char_tokens:
                self.advance()
                self.tokens.append(Token(single_char_tokens[ch], ch, start_line, start_col))
                continue

            raise self.error(f"Unexpected character: {ch!r}")

        self.tokens.append(Token(TokenType.EOF, None, self.line, self.column))
        return self.tokens
