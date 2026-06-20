# Copyright (C) 2026 robinpie
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

"""!~ATH (Until Death) - An esoteric programming language interpreter."""

from .lexer import Lexer, Token, TokenType
from .parser import Parser
from .interpreter import Interpreter
from .errors import TildeAthError, LexerError, ParseError, RuntimeError

__version__ = "1.0.0"
__all__ = [
    'Lexer', 'Token', 'TokenType',
    'Parser',
    'Interpreter',
    'TildeAthError', 'LexerError', 'ParseError', 'RuntimeError',
]
