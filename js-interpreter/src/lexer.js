/**
 * Lexer for the !~ATH language.
 */

import { LexerError } from './errors.js';
import { TokenType, KEYWORDS, Token } from './tokens.js';

export class Lexer {
  constructor(source) {
    this.source = source;
    this.pos = 0;
    this.line = 1;
    this.column = 1;
    this.tokens = [];
  }

  error(message) {
    return new LexerError(message, this.line, this.column);
  }

  peek(offset = 0) {
    const pos = this.pos + offset;
    if (pos >= this.source.length) {
      return null;
    }
    return this.source[pos];
  }

  advance() {
    if (this.pos >= this.source.length) {
      return null;
    }
    const ch = this.source[this.pos];
    this.pos++;
    if (ch === '\n') {
      this.line++;
      this.column = 1;
    } else {
      this.column++;
    }
    return ch;
  }

  skipWhitespaceAndComments() {
    while (this.pos < this.source.length) {
      const ch = this.peek();
      if (ch === ' ' || ch === '\t' || ch === '\r' || ch === '\n') {
        this.advance();
      } else if (ch === '/' && this.peek(1) === '/') {
        // Single-line comment
        while (this.peek() !== null && this.peek() !== '\n') {
          this.advance();
        }
      } else {
        break;
      }
    }
  }

  readString() {
    const startLine = this.line;
    const startCol = this.column;
    this.advance();  // consume opening quote
    const result = [];

    while (true) {
      const ch = this.peek();
      if (ch === null) {
        throw this.error('Unterminated string');
      }
      if (ch === '"') {
        this.advance();
        break;
      }
      if (ch === '\\') {
        this.advance();
        const escape = this.peek();
        if (escape === null) {
          throw this.error('Unterminated string');
        }
        if (escape === 'n') {
          result.push('\n');
        } else if (escape === 't') {
          result.push('\t');
        } else if (escape === '\\') {
          result.push('\\');
        } else if (escape === '"') {
          result.push('"');
        } else {
          throw this.error(`Unknown escape sequence: \\${escape}`);
        }
        this.advance();
      } else {
        result.push(ch);
        this.advance();
      }
    }

    return result.join('');
  }

  readNumber() {
    const startLine = this.line;
    const startCol = this.column;
    const result = [];

    // Handle negative sign
    if (this.peek() === '-') {
      result.push(this.advance());
    }

    // Read digits
    while (this.peek() !== null && /\d/.test(this.peek())) {
      result.push(this.advance());
    }

    // Check for float
    if (this.peek() === '.' && this.peek(1) !== null && /\d/.test(this.peek(1))) {
      result.push(this.advance());  // consume '.'
      while (this.peek() !== null && /\d/.test(this.peek())) {
        result.push(this.advance());
      }
      return new Token(TokenType.FLOAT, parseFloat(result.join('')), startLine, startCol);
    }

    const value = parseInt(result.join(''), 10);

    // Check for duration suffix
    if (this.peek() === 'm' || this.peek() === 's' || this.peek() === 'h') {
      const suffix = this.advance();
      if (suffix === 'm' && this.peek() === 's') {
        this.advance();
        return new Token(TokenType.DURATION, { unit: 'ms', value }, startLine, startCol);
      } else if (suffix === 's') {
        return new Token(TokenType.DURATION, { unit: 's', value }, startLine, startCol);
      } else if (suffix === 'm') {
        return new Token(TokenType.DURATION, { unit: 'm', value }, startLine, startCol);
      } else if (suffix === 'h') {
        return new Token(TokenType.DURATION, { unit: 'h', value }, startLine, startCol);
      }
    }

    return new Token(TokenType.INTEGER, value, startLine, startCol);
  }

  readIdentifier() {
    const startLine = this.line;
    const startCol = this.column;
    const result = [];

    while (this.peek() !== null && /[a-zA-Z0-9_]/.test(this.peek())) {
      result.push(this.advance());
    }

    const name = result.join('');
    const tokenType = KEYWORDS[name] || TokenType.IDENTIFIER;

    // Special value handling
    if (tokenType === TokenType.ALIVE) {
      return new Token(tokenType, true, startLine, startCol);
    } else if (tokenType === TokenType.DEAD) {
      return new Token(tokenType, false, startLine, startCol);
    } else if (tokenType === TokenType.VOID) {
      return new Token(tokenType, null, startLine, startCol);
    }

    return new Token(tokenType, name, startLine, startCol);
  }

  tokenize() {
    this.tokens = [];

    while (this.pos < this.source.length) {
      this.skipWhitespaceAndComments();

      if (this.pos >= this.source.length) {
        break;
      }

      const startLine = this.line;
      const startCol = this.column;
      const ch = this.peek();

      // ~ATH special token
      if (ch === '~' && this.peek(1) === 'A' && this.peek(2) === 'T' && this.peek(3) === 'H') {
        for (let i = 0; i < 4; i++) {
          this.advance();
        }
        this.tokens.push(new Token(TokenType.TILDE_ATH, '~ATH', startLine, startCol));
        continue;
      }

      // String
      if (ch === '"') {
        const value = this.readString();
        this.tokens.push(new Token(TokenType.STRING, value, startLine, startCol));
        continue;
      }

      // Number (including negative)
      let isNegative = false;
      if (ch === '-' && this.peek(1) !== null && /\d/.test(this.peek(1))) {
        let isSubtraction = false;
        if (this.tokens.length > 0) {
          const last = this.tokens[this.tokens.length - 1].type;
          if (
            last === TokenType.IDENTIFIER ||
            last === TokenType.INTEGER ||
            last === TokenType.FLOAT ||
            last === TokenType.STRING ||
            last === TokenType.DURATION ||
            last === TokenType.ALIVE ||
            last === TokenType.DEAD ||
            last === TokenType.VOID ||
            last === TokenType.RPAREN ||
            last === TokenType.RBRACKET
          ) {
            isSubtraction = true;
          }
        }

        if (!isSubtraction) {
          isNegative = true;
        }
      }

      if (/\d/.test(ch) || isNegative) {
        this.tokens.push(this.readNumber());
        continue;
      }

      // Identifier or keyword
      if (/[a-zA-Z_]/.test(ch)) {
        this.tokens.push(this.readIdentifier());
        continue;
      }

      // Two-character operators
      if (ch === '&' && this.peek(1) === '&') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.AMPAMP, '&&', startLine, startCol));
        continue;
      }

      if (ch === '|' && this.peek(1) === '|') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.PIPEPIPE, '||', startLine, startCol));
        continue;
      }
      
      // Shift operators
      if (ch === '<' && this.peek(1) === '<') {
        this.advance(); this.advance();
        this.tokens.push(new Token(TokenType.LSHIFT, '<<', startLine, startCol));
        continue;
      }

      if (ch === '>' && this.peek(1) === '>') {
        this.advance(); this.advance();
        this.tokens.push(new Token(TokenType.RSHIFT, '>>', startLine, startCol));
        continue;
      }

      if (ch === '=' && this.peek(1) === '=') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.EQ, '==', startLine, startCol));
        continue;
      }

      if (ch === '!' && this.peek(1) === '=') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.NE, '!=', startLine, startCol));
        continue;
      }

      if (ch === '<' && this.peek(1) === '=') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.LE, '<=', startLine, startCol));
        continue;
      }

      if (ch === '>' && this.peek(1) === '=') {
        this.advance();
        this.advance();
        this.tokens.push(new Token(TokenType.GE, '>=', startLine, startCol));
        continue;
      }

      // Single-character tokens
      const singleCharTokens = {
        '+': TokenType.PLUS,
        '-': TokenType.MINUS,
        '*': TokenType.STAR,
        '/': TokenType.SLASH,
        '%': TokenType.PERCENT,
        '<': TokenType.LT,
        '>': TokenType.GT,
        '=': TokenType.ASSIGN,
        '!': TokenType.BANG,
        '&': TokenType.AMP,
        '|': TokenType.PIPE,
        '^': TokenType.CARET,
        '~': TokenType.TILDE,
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
      };

      if (singleCharTokens[ch]) {
        this.advance();
        this.tokens.push(new Token(singleCharTokens[ch], ch, startLine, startCol));
        continue;
      }

      throw this.error(`Unexpected character: ${JSON.stringify(ch)}`);
    }

    this.tokens.push(new Token(TokenType.EOF, null, this.line, this.column));
    return this.tokens;
  }
}
