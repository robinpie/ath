/*
 * Copyright (C) 2026 robinpie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

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
