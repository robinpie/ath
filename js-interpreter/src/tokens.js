/**
 * Token types and Token class for the !~ATH lexer.
 */

/**
 * Token type enumeration.
 */
export const TokenType = {
  // Literals
  INTEGER: 'INTEGER',
  FLOAT: 'FLOAT',
  STRING: 'STRING',

  // Keywords - ATH constructs
  IMPORT: 'IMPORT',
  BIFURCATE: 'BIFURCATE',
  EXECUTE: 'EXECUTE',
  DIE: 'DIE',
  THIS: 'THIS',
  TILDE_ATH: 'TILDE_ATH',  // ~ATH

  // Entity types
  TIMER: 'TIMER',
  PROCESS: 'PROCESS',
  CONNECTION: 'CONNECTION',
  WATCHER: 'WATCHER',

  // Expression keywords
  BIRTH: 'BIRTH',
  ENTOMB: 'ENTOMB',
  WITH: 'WITH',
  ALIVE: 'ALIVE',
  DEAD: 'DEAD',
  VOID: 'VOID',
  SHOULD: 'SHOULD',
  LEST: 'LEST',
  RITE: 'RITE',
  BEQUEATH: 'BEQUEATH',
  ATTEMPT: 'ATTEMPT',
  SALVAGE: 'SALVAGE',
  CONDEMN: 'CONDEMN',
  AND: 'AND',
  OR: 'OR',
  NOT: 'NOT',

  // Operators
  PLUS: 'PLUS',
  MINUS: 'MINUS',
  STAR: 'STAR',
  SLASH: 'SLASH',
  PERCENT: 'PERCENT',
  EQ: 'EQ',
  NE: 'NE',
  LT: 'LT',
  GT: 'GT',
  LE: 'LE',
  GE: 'GE',
  ASSIGN: 'ASSIGN',

  // Entity operators (only valid in entity expressions)
  AMPAMP: 'AMPAMP',    // &&
  PIPEPIPE: 'PIPEPIPE', // ||
  BANG: 'BANG',        // !

  // Punctuation
  LPAREN: 'LPAREN',
  RPAREN: 'RPAREN',
  LBRACE: 'LBRACE',
  RBRACE: 'RBRACE',
  LBRACKET: 'LBRACKET',
  RBRACKET: 'RBRACKET',
  SEMICOLON: 'SEMICOLON',
  COMMA: 'COMMA',
  DOT: 'DOT',
  COLON: 'COLON',

  // Duration suffixes (attached to integers)
  DURATION: 'DURATION',  // e.g., 100ms, 5s, 2m, 1h

  // Identifier
  IDENTIFIER: 'IDENTIFIER',

  // End of file
  EOF: 'EOF',
};

/**
 * Keyword to TokenType mapping.
 */
export const KEYWORDS = {
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
};

/**
 * Token class representing a single token.
 */
export class Token {
  constructor(type, value, line, column) {
    this.type = type;
    this.value = value;
    this.line = line;
    this.column = column;
  }

  toString() {
    return `Token(${this.type}, ${JSON.stringify(this.value)}, ${this.line}:${this.column})`;
  }
}
