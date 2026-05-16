/**
 * Error types for the !~ATH interpreter.
 */

/**
 * Base error for all !~ATH errors.
 */
export class TildeAthError extends Error {
  constructor(message, line = null, column = null) {
    super(TildeAthError._formatMessage(message, line, column));
    this.name = 'TildeAthError';
    this.tildeAthMessage = message;
    this.line = line;
    this.column = column;
  }

  static _formatMessage(message, line, column) {
    if (line !== null) {
      if (column !== null) {
        return `[line ${line}, col ${column}] ${message}`;
      }
      return `[line ${line}] ${message}`;
    }
    return message;
  }
}

/**
 * Error during lexical analysis.
 */
export class LexerError extends TildeAthError {
  constructor(message, line = null, column = null) {
    super(message, line, column);
    this.name = 'LexerError';
  }
}

/**
 * Error during parsing.
 */
export class ParseError extends TildeAthError {
  constructor(message, line = null, column = null) {
    super(message, line, column);
    this.name = 'ParseError';
  }
}

/**
 * Error during execution.
 */
export class RuntimeError extends TildeAthError {
  constructor(message, line = null, column = null) {
    super(message, line, column);
    this.name = 'RuntimeError';
  }
}

/**
 * User-thrown error via CONDEMN.
 */
export class CondemnError extends RuntimeError {
  constructor(message, line = null, column = null) {
    super(message, line, column);
    this.name = 'CondemnError';
  }
}

/**
 * Control flow for BEQUEATH (not a real error).
 */
export class BequeathError extends Error {
  constructor(value) {
    super();
    this.name = 'BequeathError';
    this.value = value;
  }
}
