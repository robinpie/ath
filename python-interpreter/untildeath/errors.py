"""Error types for the !~ATH interpreter."""


class TildeAthError(Exception):
    """Base error for all !~ATH errors."""

    def __init__(self, message: str, line: int = None, column: int = None):
        self.message = message
        self.line = line
        self.column = column
        super().__init__(self._format_message())

    def _format_message(self) -> str:
        if self.line is not None:
            if self.column is not None:
                return f"[line {self.line}, col {self.column}] {self.message}"
            return f"[line {self.line}] {self.message}"
        return self.message


class LexerError(TildeAthError):
    """Error during lexical analysis."""
    pass


class ParseError(TildeAthError):
    """Error during parsing."""
    pass


class RuntimeError(TildeAthError):
    """Error during execution."""
    pass


class CondemnError(RuntimeError):
    """User-thrown error via CONDEMN."""
    pass


class BequeathError(Exception):
    """Control flow for BEQUEATH (not a real error)."""

    def __init__(self, value):
        self.value = value
        super().__init__()


class DebuggerQuitException(Exception):
    """Raised when user quits the debugger."""
    pass
