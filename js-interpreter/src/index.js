/**
 * !~ATH JavaScript interpreter.
 * Copyright (C) 2026 Robin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * Public API for the !~ATH JavaScript interpreter.
 */

import { Lexer } from './lexer.js';
import { Parser } from './parser.js';
import { Interpreter } from './interpreter.js';

// Re-export error types
export { TildeAthError, LexerError, ParseError, RuntimeError, CondemnError } from './errors.js';

// Re-export other useful classes
export { Lexer } from './lexer.js';
export { Parser } from './parser.js';
export { Interpreter } from './interpreter.js';
export { TokenType, Token } from './tokens.js';

/**
 * Main runtime class for executing !~ATH programs.
 *
 * @example
 * const runtime = new TildeAth({
 *   onOutput: (text) => console.log(text),
 *   inputQueue: ['line1', 'line2'],
 * });
 * await runtime.run(`
 *   import timer T(100ms);
 *   ~ATH(T) { } EXECUTE(UTTER("Hello!"));
 *   THIS.DIE();
 * `);
 */
export class TildeAth {
  /**
   * Create a new TildeAth runtime.
   *
   * @param {Object} options - Configuration options
   * @param {Function} [options.onOutput] - Callback for UTTER output. Defaults to console.log.
   * @param {Function} [options.onInput] - Callback for HEED input. Called when inputQueue is empty.
   * @param {Array<string>} [options.inputQueue] - Pre-populated input queue for HEED.
   * @param {Array<string>} [options.scryQueue] - Pre-populated input queue for SCRY(VOID).
   */
  constructor(options = {}) {
    this.options = {
      onOutput: options.onOutput || ((text) => console.log(text)),
      onInput: options.onInput || null,
      inputQueue: options.inputQueue || [],
      scryQueue: options.scryQueue || [],
      debugger: options.debugger || null,
    };
  }

  /**
   * Parse source code into an AST without executing it.
   *
   * @param {string} source - !~ATH source code
   * @returns {Object} The parsed AST program node
   * @throws {LexerError} If tokenization fails
   * @throws {ParseError} If parsing fails
   */
  parse(source) {
    const lexer = new Lexer(source);
    const tokens = lexer.tokenize();
    const parser = new Parser(tokens);
    return parser.parse();
  }

  /**
   * Execute !~ATH source code.
   *
   * @param {string} source - !~ATH source code
   * @returns {Promise<void>}
   * @throws {LexerError} If tokenization fails
   * @throws {ParseError} If parsing fails
   * @throws {RuntimeError} If execution fails
   * @throws {CondemnError} If an uncaught CONDEMN is thrown
   */
  async run(source) {
    const program = this.parse(source);
    const interpreter = new Interpreter(this.options);
    await interpreter.run(program);
  }

  /**
   * Execute an already-parsed AST program.
   *
   * @param {Object} program - Parsed AST program node
   * @returns {Promise<void>}
   * @throws {RuntimeError} If execution fails
   * @throws {CondemnError} If an uncaught CONDEMN is thrown
   */
  async runProgram(program) {
    const interpreter = new Interpreter(this.options);
    await interpreter.run(program);
  }
}

// Default export
export default TildeAth;
