/**
 * !~ATH Interpreter - Browser Bundle
 * A complete interpreter for the !~ATH esoteric programming language.
 */
(function(global) {
  'use strict';

  // ============ ERRORS ============

  class TildeAthError extends Error {
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

  class LexerError extends TildeAthError {
    constructor(message, line = null, column = null) {
      super(message, line, column);
      this.name = 'LexerError';
    }
  }

  class ParseError extends TildeAthError {
    constructor(message, line = null, column = null) {
      super(message, line, column);
      this.name = 'ParseError';
    }
  }

  class RuntimeError extends TildeAthError {
    constructor(message, line = null, column = null) {
      super(message, line, column);
      this.name = 'RuntimeError';
    }
  }

  class CondemnError extends RuntimeError {
    constructor(message, line = null, column = null) {
      super(message, line, column);
      this.name = 'CondemnError';
    }
  }

  class BequeathError extends Error {
    constructor(value) {
      super();
      this.name = 'BequeathError';
      this.value = value;
    }
  }

  // ============ TOKENS ============

  const TokenType = {
    INTEGER: 'INTEGER',
    FLOAT: 'FLOAT',
    STRING: 'STRING',
    IMPORT: 'IMPORT',
    BIFURCATE: 'BIFURCATE',
    EXECUTE: 'EXECUTE',
    DIE: 'DIE',
    THIS: 'THIS',
    TILDE_ATH: 'TILDE_ATH',
    TIMER: 'TIMER',
    PROCESS: 'PROCESS',
    CONNECTION: 'CONNECTION',
    WATCHER: 'WATCHER',
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
    AMPAMP: 'AMPAMP',
    PIPEPIPE: 'PIPEPIPE',
    BANG: 'BANG',
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
    DURATION: 'DURATION',
    IDENTIFIER: 'IDENTIFIER',
    EOF: 'EOF',
  };

  const KEYWORDS = {
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

  class Token {
    constructor(type, value, line, column) {
      this.type = type;
      this.value = value;
      this.line = line;
      this.column = column;
    }
  }

  // ============ LEXER ============

  class Lexer {
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
      if (pos >= this.source.length) return null;
      return this.source[pos];
    }

    advance() {
      if (this.pos >= this.source.length) return null;
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
          while (this.peek() !== null && this.peek() !== '\n') {
            this.advance();
          }
        } else {
          break;
        }
      }
    }

    readString() {
      this.advance();
      const result = [];
      while (true) {
        const ch = this.peek();
        if (ch === null) throw this.error('Unterminated string');
        if (ch === '"') {
          this.advance();
          break;
        }
        if (ch === '\\') {
          this.advance();
          const escape = this.peek();
          if (escape === null) throw this.error('Unterminated string');
          if (escape === 'n') result.push('\n');
          else if (escape === 't') result.push('\t');
          else if (escape === '\\') result.push('\\');
          else if (escape === '"') result.push('"');
          else throw this.error(`Unknown escape sequence: \\${escape}`);
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
      if (this.peek() === '-') result.push(this.advance());
      while (this.peek() !== null && /\d/.test(this.peek())) {
        result.push(this.advance());
      }
      if (this.peek() === '.' && this.peek(1) !== null && /\d/.test(this.peek(1))) {
        result.push(this.advance());
        while (this.peek() !== null && /\d/.test(this.peek())) {
          result.push(this.advance());
        }
        return new Token(TokenType.FLOAT, parseFloat(result.join('')), startLine, startCol);
      }
      const value = parseInt(result.join(''), 10);
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
      if (tokenType === TokenType.ALIVE) return new Token(tokenType, true, startLine, startCol);
      if (tokenType === TokenType.DEAD) return new Token(tokenType, false, startLine, startCol);
      if (tokenType === TokenType.VOID) return new Token(tokenType, null, startLine, startCol);
      return new Token(tokenType, name, startLine, startCol);
    }

    tokenize() {
      this.tokens = [];
      while (this.pos < this.source.length) {
        this.skipWhitespaceAndComments();
        if (this.pos >= this.source.length) break;
        const startLine = this.line;
        const startCol = this.column;
        const ch = this.peek();

        if (ch === '~' && this.peek(1) === 'A' && this.peek(2) === 'T' && this.peek(3) === 'H') {
          for (let i = 0; i < 4; i++) this.advance();
          this.tokens.push(new Token(TokenType.TILDE_ATH, '~ATH', startLine, startCol));
          continue;
        }
        if (ch === '"') {
          const value = this.readString();
          this.tokens.push(new Token(TokenType.STRING, value, startLine, startCol));
          continue;
        }
        if (/\d/.test(ch) || (ch === '-' && this.peek(1) !== null && /\d/.test(this.peek(1)))) {
          this.tokens.push(this.readNumber());
          continue;
        }
        if (/[a-zA-Z_]/.test(ch)) {
          this.tokens.push(this.readIdentifier());
          continue;
        }
        if (ch === '&' && this.peek(1) === '&') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.AMPAMP, '&&', startLine, startCol)); continue; }
        if (ch === '|' && this.peek(1) === '|') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.PIPEPIPE, '||', startLine, startCol)); continue; }
        if (ch === '=' && this.peek(1) === '=') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.EQ, '==', startLine, startCol)); continue; }
        if (ch === '!' && this.peek(1) === '=') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.NE, '!=', startLine, startCol)); continue; }
        if (ch === '<' && this.peek(1) === '=') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.LE, '<=', startLine, startCol)); continue; }
        if (ch === '>' && this.peek(1) === '=') { this.advance(); this.advance(); this.tokens.push(new Token(TokenType.GE, '>=', startLine, startCol)); continue; }

        const singleCharTokens = {
          '+': TokenType.PLUS, '-': TokenType.MINUS, '*': TokenType.STAR, '/': TokenType.SLASH,
          '%': TokenType.PERCENT, '<': TokenType.LT, '>': TokenType.GT, '=': TokenType.ASSIGN,
          '!': TokenType.BANG, '(': TokenType.LPAREN, ')': TokenType.RPAREN,
          '{': TokenType.LBRACE, '}': TokenType.RBRACE, '[': TokenType.LBRACKET,
          ']': TokenType.RBRACKET, ';': TokenType.SEMICOLON, ',': TokenType.COMMA,
          '.': TokenType.DOT, ':': TokenType.COLON,
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

  // ============ AST ============

  const ast = {
    createProgram: (statements = [], line = 0, column = 0) => ({ type: 'Program', statements, line, column }),
    createImportStmt: (entityType, name, args = [], line = 0, column = 0) => ({ type: 'ImportStmt', entityType, name, args, line, column }),
    createBifurcateStmt: (entity, branch1, branch2, line = 0, column = 0) => ({ type: 'BifurcateStmt', entity, branch1, branch2, line, column }),
    createAthLoop: (entityExpr, body = [], execute = [], line = 0, column = 0) => ({ type: 'AthLoop', entityExpr, body, execute, line, column }),
    createDieStmt: (target, line = 0, column = 0) => ({ type: 'DieStmt', target, line, column }),
    createVarDecl: (name, value, line = 0, column = 0) => ({ type: 'VarDecl', name, value, line, column }),
    createConstDecl: (name, value, line = 0, column = 0) => ({ type: 'ConstDecl', name, value, line, column }),
    createAssignment: (target, value, line = 0, column = 0) => ({ type: 'Assignment', target, value, line, column }),
    createRiteDef: (name, params = [], body = [], line = 0, column = 0) => ({ type: 'RiteDef', name, params, body, line, column }),
    createConditional: (condition, thenBranch = [], elseBranch = null, line = 0, column = 0) => ({ type: 'Conditional', condition, thenBranch, elseBranch, line, column }),
    createAttemptSalvage: (attemptBody = [], errorName, salvageBody = [], line = 0, column = 0) => ({ type: 'AttemptSalvage', attemptBody, errorName, salvageBody, line, column }),
    createCondemnStmt: (message, line = 0, column = 0) => ({ type: 'CondemnStmt', message, line, column }),
    createBequeathStmt: (value = null, line = 0, column = 0) => ({ type: 'BequeathStmt', value, line, column }),
    createExprStmt: (expression, line = 0, column = 0) => ({ type: 'ExprStmt', expression, line, column }),
    createEntityAnd: (left, right, line = 0, column = 0) => ({ type: 'EntityAnd', left, right, line, column }),
    createEntityOr: (left, right, line = 0, column = 0) => ({ type: 'EntityOr', left, right, line, column }),
    createEntityNot: (operand, line = 0, column = 0) => ({ type: 'EntityNot', operand, line, column }),
    createEntityIdent: (name, line = 0, column = 0) => ({ type: 'EntityIdent', name, line, column }),
    createDieIdent: (name, line = 0, column = 0) => ({ type: 'DieIdent', name, line, column }),
    createDiePair: (left, right, line = 0, column = 0) => ({ type: 'DiePair', left, right, line, column }),
    createLiteral: (value, line = 0, column = 0) => ({ type: 'Literal', value, line, column }),
    createIdentifier: (name, line = 0, column = 0) => ({ type: 'Identifier', name, line, column }),
    createBinaryOp: (operator, left, right, line = 0, column = 0) => ({ type: 'BinaryOp', operator, left, right, line, column }),
    createUnaryOp: (operator, operand, line = 0, column = 0) => ({ type: 'UnaryOp', operator, operand, line, column }),
    createCallExpr: (callee, args = [], line = 0, column = 0) => ({ type: 'CallExpr', callee, args, line, column }),
    createIndexExpr: (obj, index, line = 0, column = 0) => ({ type: 'IndexExpr', obj, index, line, column }),
    createMemberExpr: (obj, member, line = 0, column = 0) => ({ type: 'MemberExpr', obj, member, line, column }),
    createArrayLiteral: (elements = [], line = 0, column = 0) => ({ type: 'ArrayLiteral', elements, line, column }),
    createMapLiteral: (entries = [], line = 0, column = 0) => ({ type: 'MapLiteral', entries, line, column }),
    createDuration: (unit, value, line = 0, column = 0) => ({ type: 'Duration', unit, value, line, column }),
  };

  // ============ PARSER ============

  class Parser {
    constructor(tokens) {
      this.tokens = tokens;
      this.pos = 0;
    }
    error(message, token = null) {
      if (token === null) token = this.current();
      return new ParseError(message, token.line, token.column);
    }
    current() {
      if (this.pos >= this.tokens.length) return this.tokens[this.tokens.length - 1];
      return this.tokens[this.pos];
    }
    peek(offset = 0) {
      const pos = this.pos + offset;
      if (pos >= this.tokens.length) return this.tokens[this.tokens.length - 1];
      return this.tokens[pos];
    }
    check(...types) { return types.includes(this.current().type); }
    advance() {
      const token = this.current();
      if (this.pos < this.tokens.length) this.pos++;
      return token;
    }
    consume(tokenType, message) {
      if (this.check(tokenType)) return this.advance();
      throw this.error(message);
    }
    match(...types) {
      if (this.check(...types)) { this.advance(); return true; }
      return false;
    }

    parse() {
      const statements = [];
      while (!this.check(TokenType.EOF)) {
        statements.push(this.parseStatement());
      }
      return ast.createProgram(statements);
    }

    parseStatement() {
      if (this.check(TokenType.IMPORT)) return this.parseImport();
      if (this.check(TokenType.BIFURCATE)) return this.parseBifurcate();
      if (this.check(TokenType.TILDE_ATH)) return this.parseAthLoop();
      if (this.check(TokenType.BIRTH)) return this.parseVarDecl();
      if (this.check(TokenType.ENTOMB)) return this.parseConstDecl();
      if (this.check(TokenType.RITE)) return this.parseRiteDef();
      if (this.check(TokenType.SHOULD)) return this.parseConditional();
      if (this.check(TokenType.ATTEMPT)) return this.parseAttemptSalvage();
      if (this.check(TokenType.CONDEMN)) return this.parseCondemn();
      if (this.check(TokenType.BEQUEATH)) return this.parseBequeath();
      if (this.check(TokenType.IDENTIFIER) || this.check(TokenType.LBRACKET)) return this.parseDieOrAssignmentOrExpr();
      if (this.check(TokenType.THIS)) return this.parseDieOrExpr();
      throw this.error(`Unexpected token: ${this.current().type}`);
    }

    parseImport() {
      const token = this.advance();
      const line = token.line, col = token.column;
      if (!this.check(TokenType.TIMER, TokenType.PROCESS, TokenType.CONNECTION, TokenType.WATCHER)) {
        throw this.error('Expected entity type (timer, process, connection, watcher)');
      }
      const entityType = this.advance().value;
      const name = this.consume(TokenType.IDENTIFIER, "Expected entity name").value;
      this.consume(TokenType.LPAREN, "Expected '(' after entity name");
      const args = [];
      if (entityType === 'timer') {
        if (this.check(TokenType.DURATION)) {
          const durToken = this.advance();
          args.push(ast.createDuration(durToken.value.unit, durToken.value.value, durToken.line, durToken.column));
        } else if (this.check(TokenType.INTEGER)) {
          const intToken = this.advance();
          args.push(ast.createDuration('ms', intToken.value, intToken.line, intToken.column));
        } else {
          throw this.error('Expected duration for timer');
        }
      } else {
        if (!this.check(TokenType.RPAREN)) {
          args.push(this.parseExpression());
          while (this.match(TokenType.COMMA)) args.push(this.parseExpression());
        }
      }
      this.consume(TokenType.RPAREN, "Expected ')' after import arguments");
      this.consume(TokenType.SEMICOLON, "Expected ';' after import statement");
      return ast.createImportStmt(entityType, name, args, line, col);
    }

    parseBifurcate() {
      const token = this.advance();
      const line = token.line, col = token.column;
      let entity;
      if (this.check(TokenType.THIS)) entity = this.advance().value;
      else entity = this.consume(TokenType.IDENTIFIER, "Expected entity to bifurcate").value;
      this.consume(TokenType.LBRACKET, "Expected '[' after entity");
      const branch1 = this.consume(TokenType.IDENTIFIER, "Expected first branch name").value;
      this.consume(TokenType.COMMA, "Expected ',' between branch names");
      const branch2 = this.consume(TokenType.IDENTIFIER, "Expected second branch name").value;
      this.consume(TokenType.RBRACKET, "Expected ']' after branch names");
      this.consume(TokenType.SEMICOLON, "Expected ';' after bifurcate statement");
      return ast.createBifurcateStmt(entity, branch1, branch2, line, col);
    }

    parseAthLoop() {
      const token = this.advance();
      const line = token.line, col = token.column;
      this.consume(TokenType.LPAREN, "Expected '(' after ~ATH");
      const entityExpr = this.parseEntityExpr();
      this.consume(TokenType.RPAREN, "Expected ')' after entity expression");
      this.consume(TokenType.LBRACE, "Expected '{' for ~ATH body");
      const body = [];
      while (!this.check(TokenType.RBRACE)) body.push(this.parseStatement());
      this.consume(TokenType.RBRACE, "Expected '}' after ~ATH body");
      this.consume(TokenType.EXECUTE, "Expected 'EXECUTE' after ~ATH body");
      this.consume(TokenType.LPAREN, "Expected '(' after EXECUTE");
      const execute = this.parseExecuteBody();
      this.consume(TokenType.RPAREN, "Expected ')' after EXECUTE body");
      this.consume(TokenType.SEMICOLON, "Expected ';' after ~ATH loop");
      return ast.createAthLoop(entityExpr, body, execute, line, col);
    }

    parseExecuteBody() {
      const statements = [];
      while (!this.check(TokenType.RPAREN)) {
        const stmt = this.parseExecuteStatement();
        if (stmt !== null) statements.push(stmt);
        if (this.check(TokenType.RPAREN)) break;
      }
      return statements;
    }

    parseExecuteStatement() {
      if (this.check(TokenType.IMPORT)) return this.parseImport();
      if (this.check(TokenType.TILDE_ATH)) return this.parseAthLoop();
      if (this.check(TokenType.BIRTH)) return this.parseVarDecl();
      if (this.check(TokenType.ENTOMB)) return this.parseConstDecl();
      if (this.check(TokenType.RITE)) return this.parseRiteDef();
      if (this.check(TokenType.SHOULD)) return this.parseConditional();
      if (this.check(TokenType.ATTEMPT)) return this.parseAttemptSalvage();
      if (this.check(TokenType.CONDEMN)) return this.parseCondemn();
      if (this.check(TokenType.BEQUEATH)) return this.parseBequeath();
      if (this.check(TokenType.VOID)) {
        const voidToken = this.advance();
        this.match(TokenType.SEMICOLON);
        return ast.createExprStmt(ast.createLiteral(null, voidToken.line, voidToken.column), voidToken.line, voidToken.column);
      }
      const expr = this.parseExpression();
      if (this.check(TokenType.ASSIGN)) {
        this.advance();
        const value = this.parseExpression();
        this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
        return ast.createAssignment(expr, value, expr.line, expr.column);
      }
      this.match(TokenType.SEMICOLON);
      return ast.createExprStmt(expr, expr.line, expr.column);
    }

    parseVarDecl() {
      const token = this.advance();
      const name = this.consume(TokenType.IDENTIFIER, "Expected variable name").value;
      this.consume(TokenType.WITH, "Expected 'WITH' after variable name");
      const value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after variable declaration");
      return ast.createVarDecl(name, value, token.line, token.column);
    }

    parseConstDecl() {
      const token = this.advance();
      const name = this.consume(TokenType.IDENTIFIER, "Expected constant name").value;
      this.consume(TokenType.WITH, "Expected 'WITH' after constant name");
      const value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after constant declaration");
      return ast.createConstDecl(name, value, token.line, token.column);
    }

    parseRiteDef() {
      const token = this.advance();
      const name = this.consume(TokenType.IDENTIFIER, "Expected rite name").value;
      this.consume(TokenType.LPAREN, "Expected '(' after rite name");
      const params = [];
      if (!this.check(TokenType.RPAREN)) {
        params.push(this.consume(TokenType.IDENTIFIER, "Expected parameter name").value);
        while (this.match(TokenType.COMMA)) params.push(this.consume(TokenType.IDENTIFIER, "Expected parameter name").value);
      }
      this.consume(TokenType.RPAREN, "Expected ')' after parameters");
      this.consume(TokenType.LBRACE, "Expected '{' for rite body");
      const body = [];
      while (!this.check(TokenType.RBRACE)) body.push(this.parseExecuteStatement());
      this.consume(TokenType.RBRACE, "Expected '}' after rite body");
      return ast.createRiteDef(name, params, body, token.line, token.column);
    }

    parseConditional() {
      const token = this.advance();
      const condition = this.parseExpression();
      this.consume(TokenType.LBRACE, "Expected '{' after condition");
      const thenBranch = [];
      while (!this.check(TokenType.RBRACE)) thenBranch.push(this.parseExecuteStatement());
      this.consume(TokenType.RBRACE, "Expected '}' after then branch");
      let elseBranch = null;
      if (this.match(TokenType.LEST)) {
        if (this.check(TokenType.SHOULD)) {
          elseBranch = [this.parseConditional()];
        } else {
          this.consume(TokenType.LBRACE, "Expected '{' after LEST");
          elseBranch = [];
          while (!this.check(TokenType.RBRACE)) elseBranch.push(this.parseExecuteStatement());
          this.consume(TokenType.RBRACE, "Expected '}' after else branch");
        }
      }
      return ast.createConditional(condition, thenBranch, elseBranch, token.line, token.column);
    }

    parseAttemptSalvage() {
      const token = this.advance();
      this.consume(TokenType.LBRACE, "Expected '{' after ATTEMPT");
      const attemptBody = [];
      while (!this.check(TokenType.RBRACE)) attemptBody.push(this.parseExecuteStatement());
      this.consume(TokenType.RBRACE, "Expected '}' after ATTEMPT body");
      this.consume(TokenType.SALVAGE, "Expected 'SALVAGE' after ATTEMPT block");
      const errorName = this.consume(TokenType.IDENTIFIER, "Expected error variable name").value;
      this.consume(TokenType.LBRACE, "Expected '{' after error variable");
      const salvageBody = [];
      while (!this.check(TokenType.RBRACE)) salvageBody.push(this.parseExecuteStatement());
      this.consume(TokenType.RBRACE, "Expected '}' after SALVAGE body");
      return ast.createAttemptSalvage(attemptBody, errorName, salvageBody, token.line, token.column);
    }

    parseCondemn() {
      const token = this.advance();
      const message = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after CONDEMN");
      return ast.createCondemnStmt(message, token.line, token.column);
    }

    parseBequeath() {
      const token = this.advance();
      let value = null;
      if (!this.check(TokenType.SEMICOLON)) value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after BEQUEATH");
      return ast.createBequeathStmt(value, token.line, token.column);
    }

    parseDieOrAssignmentOrExpr() {
      if (this.check(TokenType.LBRACKET)) {
        const target = this.parseDieTarget();
        this.consume(TokenType.DOT, "Expected '.' after die target");
        this.consume(TokenType.DIE, "Expected 'DIE' after '.'");
        this.consume(TokenType.LPAREN, "Expected '(' after DIE");
        this.consume(TokenType.RPAREN, "Expected ')' after DIE(");
        this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
        return ast.createDieStmt(target, target.line, target.column);
      }
      const expr = this.parseExpression();
      if (expr.type === 'MemberExpr' && expr.member === 'DIE') {
        throw this.error("DIE must be called as ENTITY.DIE(), not used as expression");
      }
      if (this.check(TokenType.ASSIGN)) {
        this.advance();
        const value = this.parseExpression();
        this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
        return ast.createAssignment(expr, value, expr.line, expr.column);
      }
      if (expr.type === 'CallExpr' && expr.callee.type === 'MemberExpr' && expr.callee.member === 'DIE') {
        const obj = expr.callee.obj;
        if (obj.type === 'Identifier') {
          const target = ast.createDieIdent(obj.name, obj.line, obj.column);
          this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
          return ast.createDieStmt(target, expr.line, expr.column);
        }
        throw this.error("Invalid DIE target");
      }
      this.consume(TokenType.SEMICOLON, "Expected ';' after expression");
      return ast.createExprStmt(expr, expr.line, expr.column);
    }

    parseDieOrExpr() {
      const token = this.current();
      const expr = this.parseExpression();
      if (this.check(TokenType.ASSIGN)) {
        this.advance();
        const value = this.parseExpression();
        this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
        return ast.createAssignment(expr, value, token.line, token.column);
      }
      if (expr.type === 'CallExpr' && expr.callee.type === 'MemberExpr' && expr.callee.member === 'DIE') {
        const obj = expr.callee.obj;
        if (obj.type === 'Identifier' && obj.name === 'THIS') {
          const target = ast.createDieIdent('THIS', obj.line, obj.column);
          this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
          return ast.createDieStmt(target, token.line, token.column);
        }
      }
      this.consume(TokenType.SEMICOLON, "Expected ';' after expression");
      return ast.createExprStmt(expr, expr.line, expr.column);
    }

    parseDieTarget() {
      if (this.check(TokenType.LBRACKET)) {
        const token = this.advance();
        const left = this.parseDieTarget();
        this.consume(TokenType.COMMA, "Expected ',' in die target pair");
        const right = this.parseDieTarget();
        this.consume(TokenType.RBRACKET, "Expected ']' after die target pair");
        return ast.createDiePair(left, right, token.line, token.column);
      }
      if (this.check(TokenType.THIS)) {
        const token = this.advance();
        return ast.createDieIdent('THIS', token.line, token.column);
      }
      const token = this.consume(TokenType.IDENTIFIER, "Expected identifier in die target");
      return ast.createDieIdent(token.value, token.line, token.column);
    }

    parseEntityExpr() { return this.parseEntityOr(); }
    parseEntityOr() {
      let left = this.parseEntityAnd();
      while (this.match(TokenType.PIPEPIPE)) {
        const right = this.parseEntityAnd();
        left = ast.createEntityOr(left, right, left.line, left.column);
      }
      return left;
    }
    parseEntityAnd() {
      let left = this.parseEntityUnary();
      while (this.match(TokenType.AMPAMP)) {
        const right = this.parseEntityUnary();
        left = ast.createEntityAnd(left, right, left.line, left.column);
      }
      return left;
    }
    parseEntityUnary() {
      if (this.match(TokenType.BANG)) {
        const token = this.tokens[this.pos - 1];
        const operand = this.parseEntityUnary();
        return ast.createEntityNot(operand, token.line, token.column);
      }
      return this.parseEntityPrimary();
    }
    parseEntityPrimary() {
      if (this.match(TokenType.LPAREN)) {
        const expr = this.parseEntityExpr();
        this.consume(TokenType.RPAREN, "Expected ')' after entity expression");
        return expr;
      }
      if (this.check(TokenType.THIS)) {
        const token = this.advance();
        return ast.createEntityIdent('THIS', token.line, token.column);
      }
      const token = this.consume(TokenType.IDENTIFIER, "Expected entity identifier");
      return ast.createEntityIdent(token.value, token.line, token.column);
    }

    parseExpression() { return this.parseOr(); }
    parseOr() {
      let left = this.parseAnd();
      while (this.match(TokenType.OR)) {
        const token = this.tokens[this.pos - 1];
        const right = this.parseAnd();
        left = ast.createBinaryOp('OR', left, right, token.line, token.column);
      }
      return left;
    }
    parseAnd() {
      let left = this.parseEquality();
      while (this.match(TokenType.AND)) {
        const token = this.tokens[this.pos - 1];
        const right = this.parseEquality();
        left = ast.createBinaryOp('AND', left, right, token.line, token.column);
      }
      return left;
    }
    parseEquality() {
      let left = this.parseComparison();
      while (this.check(TokenType.EQ, TokenType.NE)) {
        const token = this.advance();
        const op = token.type === TokenType.EQ ? '==' : '!=';
        const right = this.parseComparison();
        left = ast.createBinaryOp(op, left, right, token.line, token.column);
      }
      return left;
    }
    parseComparison() {
      let left = this.parseTerm();
      while (this.check(TokenType.LT, TokenType.GT, TokenType.LE, TokenType.GE)) {
        const token = this.advance();
        const opMap = { [TokenType.LT]: '<', [TokenType.GT]: '>', [TokenType.LE]: '<=', [TokenType.GE]: '>=' };
        const right = this.parseTerm();
        left = ast.createBinaryOp(opMap[token.type], left, right, token.line, token.column);
      }
      return left;
    }
    parseTerm() {
      let left = this.parseFactor();
      while (this.check(TokenType.PLUS, TokenType.MINUS)) {
        const token = this.advance();
        const op = token.type === TokenType.PLUS ? '+' : '-';
        const right = this.parseFactor();
        left = ast.createBinaryOp(op, left, right, token.line, token.column);
      }
      return left;
    }
    parseFactor() {
      let left = this.parseUnary();
      while (this.check(TokenType.STAR, TokenType.SLASH, TokenType.PERCENT)) {
        const token = this.advance();
        const opMap = { [TokenType.STAR]: '*', [TokenType.SLASH]: '/', [TokenType.PERCENT]: '%' };
        const right = this.parseUnary();
        left = ast.createBinaryOp(opMap[token.type], left, right, token.line, token.column);
      }
      return left;
    }
    parseUnary() {
      if (this.match(TokenType.NOT)) {
        const token = this.tokens[this.pos - 1];
        const operand = this.parseUnary();
        return ast.createUnaryOp('NOT', operand, token.line, token.column);
      }
      if (this.match(TokenType.MINUS)) {
        const token = this.tokens[this.pos - 1];
        const operand = this.parseUnary();
        return ast.createUnaryOp('-', operand, token.line, token.column);
      }
      return this.parsePostfix();
    }
    parsePostfix() {
      let expr = this.parsePrimary();
      while (true) {
        if (this.match(TokenType.LBRACKET)) {
          const index = this.parseExpression();
          this.consume(TokenType.RBRACKET, "Expected ']' after index");
          expr = ast.createIndexExpr(expr, index, expr.line, expr.column);
        } else if (this.match(TokenType.DOT)) {
          if (this.check(TokenType.DIE)) {
            const dieToken = this.advance();
            if (this.check(TokenType.LPAREN)) {
              this.advance();
              this.consume(TokenType.RPAREN, "Expected ')' after DIE(");
              const memberExpr = ast.createMemberExpr(expr, 'DIE', dieToken.line, dieToken.column);
              expr = ast.createCallExpr(memberExpr, [], dieToken.line, dieToken.column);
            } else {
              throw this.error("Expected '(' after DIE");
            }
          } else {
            const member = this.consume(TokenType.IDENTIFIER, "Expected member name after '.'");
            expr = ast.createMemberExpr(expr, member.value, member.line, member.column);
          }
        } else if (this.match(TokenType.LPAREN)) {
          const args = [];
          if (!this.check(TokenType.RPAREN)) {
            args.push(this.parseExpression());
            while (this.match(TokenType.COMMA)) args.push(this.parseExpression());
          }
          this.consume(TokenType.RPAREN, "Expected ')' after arguments");
          expr = ast.createCallExpr(expr, args, expr.line, expr.column);
        } else {
          break;
        }
      }
      return expr;
    }
    parsePrimary() {
      const token = this.current();
      if (this.match(TokenType.INTEGER)) return ast.createLiteral(token.value, token.line, token.column);
      if (this.match(TokenType.FLOAT)) return ast.createLiteral(token.value, token.line, token.column);
      if (this.match(TokenType.STRING)) return ast.createLiteral(token.value, token.line, token.column);
      if (this.match(TokenType.ALIVE)) return ast.createLiteral(true, token.line, token.column);
      if (this.match(TokenType.DEAD)) return ast.createLiteral(false, token.line, token.column);
      if (this.match(TokenType.VOID)) return ast.createLiteral(null, token.line, token.column);
      if (this.match(TokenType.THIS)) return ast.createIdentifier('THIS', token.line, token.column);
      if (this.match(TokenType.IDENTIFIER)) return ast.createIdentifier(token.value, token.line, token.column);
      if (this.match(TokenType.LPAREN)) {
        const expr = this.parseExpression();
        this.consume(TokenType.RPAREN, "Expected ')' after expression");
        return expr;
      }
      if (this.match(TokenType.LBRACKET)) return this.parseArrayLiteral(token);
      if (this.match(TokenType.LBRACE)) return this.parseMapLiteral(token);
      throw this.error(`Unexpected token in expression: ${token.type}`);
    }
    parseArrayLiteral(startToken) {
      const elements = [];
      if (!this.check(TokenType.RBRACKET)) {
        elements.push(this.parseExpression());
        while (this.match(TokenType.COMMA)) {
          if (this.check(TokenType.RBRACKET)) break;
          elements.push(this.parseExpression());
        }
      }
      this.consume(TokenType.RBRACKET, "Expected ']' after array elements");
      return ast.createArrayLiteral(elements, startToken.line, startToken.column);
    }
    parseMapLiteral(startToken) {
      const entries = [];
      if (!this.check(TokenType.RBRACE)) {
        const key = this.parseMapKey();
        this.consume(TokenType.COLON, "Expected ':' after map key");
        const value = this.parseExpression();
        entries.push([key, value]);
        while (this.match(TokenType.COMMA)) {
          if (this.check(TokenType.RBRACE)) break;
          const k = this.parseMapKey();
          this.consume(TokenType.COLON, "Expected ':' after map key");
          const v = this.parseExpression();
          entries.push([k, v]);
        }
      }
      this.consume(TokenType.RBRACE, "Expected '}' after map entries");
      return ast.createMapLiteral(entries, startToken.line, startToken.column);
    }
    parseMapKey() {
      if (this.check(TokenType.STRING)) return this.advance().value;
      if (this.check(TokenType.IDENTIFIER)) return this.advance().value;
      throw this.error("Expected map key (identifier or string)");
    }
  }

  // ============ ENTITIES ============

  class DeathEvent {
    constructor() {
      this._resolved = false;
      this._promise = new Promise((resolve) => { this._resolve = resolve; });
    }
    set() { if (!this._resolved) { this._resolved = true; this._resolve(); } }
    isSet() { return this._resolved; }
    async wait() { return this._promise; }
  }

  class Entity {
    constructor(name) {
      this.name = name;
      this._dead = false;
      this._deathEvent = new DeathEvent();
      this._timeoutId = null;
    }
    get isDead() { return this._dead; }
    get isAlive() { return !this._dead; }
    die() {
      if (!this._dead) {
        this._dead = true;
        this._deathEvent.set();
        if (this._timeoutId !== null) { clearTimeout(this._timeoutId); this._timeoutId = null; }
      }
    }
    async waitForDeath() { await this._deathEvent.wait(); }
    async start() {}
  }

  class ThisEntity extends Entity {
    constructor() { super('THIS'); }
    async start() {}
  }

  class TimerEntity extends Entity {
    constructor(name, durationMs) {
      super(name);
      this.durationMs = durationMs;
      this._startResolve = null;
    }
    async start() {
      return new Promise((resolve) => {
        this._startResolve = resolve;
        this._timeoutId = setTimeout(() => {
          this._timeoutId = null;
          this._startResolve = null;
          this.die();
          resolve();
        }, this.durationMs);
      });
    }
    die() {
      const resolveStart = this._startResolve;
      this._startResolve = null;
      super.die();
      if (resolveStart) resolveStart();
    }
  }

  class BranchEntity extends Entity {
    constructor(name) {
      super(name);
      this._completeEvent = new DeathEvent();
    }
    async start() {}
    complete() { this._completeEvent.set(); this.die(); }
    async waitForCompletion() { await this._completeEvent.wait(); }
  }

  class CompositeEntity extends Entity {
    constructor(name, op, entities) {
      super(name);
      this.op = op;
      this.entities = entities;
    }
    async start() {
      try {
        if (this.op === 'AND') {
          await Promise.all(this.entities.map(e => e.waitForDeath()));
          this.die();
        } else if (this.op === 'OR') {
          await Promise.race(this.entities.map(e => e.waitForDeath()));
          this.die();
        } else if (this.op === 'NOT') {
          await Promise.resolve();
          this.die();
        }
      } catch (e) {}
    }
  }

  // ============ SCOPE ============

  class Scope {
    constructor(parent = null) {
      this.parent = parent;
      this.variables = new Map();
      this.constants = new Set();
    }
    define(name, value, constant = false) {
      this.variables.set(name, value);
      if (constant) this.constants.add(name);
    }
    get(name) {
      if (this.variables.has(name)) return this.variables.get(name);
      if (this.parent) return this.parent.get(name);
      throw new RuntimeError(`Undefined variable: ${name}`);
    }
    set(name, value) {
      if (this.variables.has(name)) {
        if (this.constants.has(name)) throw new RuntimeError(`Cannot reassign constant: ${name}`);
        this.variables.set(name, value);
        return;
      }
      if (this.parent) { this.parent.set(name, value); return; }
      throw new RuntimeError(`Undefined variable: ${name}`);
    }
    has(name) {
      if (this.variables.has(name)) return true;
      if (this.parent) return this.parent.has(name);
      return false;
    }
  }

  class UserRite {
    constructor(name, params, body, closure) {
      this.name = name;
      this.params = params;
      this.body = body;
      this.closure = closure;
    }
  }

  // ============ BUILTINS ============

  function stringify(value) {
    if (value === null || value === undefined) return 'VOID';
    if (typeof value === 'boolean') return value ? 'ALIVE' : 'DEAD';
    if (Array.isArray(value)) return '[' + value.map(stringify).join(', ') + ']';
    if (typeof value === 'object') {
      const entries = Object.entries(value).map(([k, v]) => `${k}: ${stringify(v)}`).join(', ');
      return '{' + entries + '}';
    }
    return String(value);
  }

  function typeName(value) {
    if (value === null || value === undefined) return 'VOID';
    if (typeof value === 'boolean') return 'BOOLEAN';
    if (typeof value === 'number') return Number.isInteger(value) ? 'INTEGER' : 'FLOAT';
    if (typeof value === 'string') return 'STRING';
    if (Array.isArray(value)) return 'ARRAY';
    if (typeof value === 'object') return 'MAP';
    return 'UNKNOWN';
  }

  function isTruthy(value) {
    if (value === null || value === undefined) return false;
    if (typeof value === 'boolean') return value;
    if (typeof value === 'number') return value !== 0;
    if (typeof value === 'string') return value.length > 0;
    if (Array.isArray(value)) return value.length > 0;
    if (typeof value === 'object') return Object.keys(value).length > 0;
    return true;
  }

  class Builtins {
    constructor(interpreter, options = {}) {
      this.interpreter = interpreter;
      this.onOutput = options.onOutput || ((text) => console.log(text));
      this.onInput = options.onInput || null;
      this.inputQueue = options.inputQueue || [];
      this._inputIndex = 0;
    }
    get(name) {
      const builtins = {
        'UTTER': (...args) => { const output = args.map(stringify).join(' '); this.onOutput(output); return null; },
        'HEED': () => {
          if (this.inputQueue.length > this._inputIndex) return this.inputQueue[this._inputIndex++];
          if (this.onInput) return this.onInput();
          return '';
        },
        'TYPEOF': (value) => typeName(value),
        'LENGTH': (value) => {
          if (typeof value === 'string' || Array.isArray(value)) return value.length;
          throw new RuntimeError(`LENGTH expects string or array, got ${typeName(value)}`);
        },
        'PARSE_INT': (value) => {
          if (typeof value !== 'string') throw new RuntimeError(`PARSE_INT expects string, got ${typeName(value)}`);
          if (value.includes('.')) throw new RuntimeError(`Cannot parse '${value}' as integer`);
          const result = parseInt(value, 10);
          if (isNaN(result)) throw new RuntimeError(`Cannot parse '${value}' as integer`);
          return result;
        },
        'PARSE_FLOAT': (value) => {
          if (typeof value !== 'string') throw new RuntimeError(`PARSE_FLOAT expects string, got ${typeName(value)}`);
          const result = parseFloat(value);
          if (isNaN(result)) throw new RuntimeError(`Cannot parse '${value}' as float`);
          return result;
        },
        'STRING': (value) => stringify(value),
        'INT': (value) => {
          if (typeof value === 'number') return Math.trunc(value);
          throw new RuntimeError(`INT expects number, got ${typeName(value)}`);
        },
        'FLOAT': (value) => {
          if (typeof value === 'number') return value;
          throw new RuntimeError(`FLOAT expects number, got ${typeName(value)}`);
        },
        'APPEND': (arr, value) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`APPEND expects array, got ${typeName(arr)}`);
          return [...arr, value];
        },
        'PREPEND': (arr, value) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`PREPEND expects array, got ${typeName(arr)}`);
          return [value, ...arr];
        },
        'SLICE': (arr, start, end) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`SLICE expects array, got ${typeName(arr)}`);
          return arr.slice(start, end);
        },
        'FIRST': (arr) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`FIRST expects array, got ${typeName(arr)}`);
          if (arr.length === 0) throw new RuntimeError('FIRST called on empty array');
          return arr[0];
        },
        'LAST': (arr) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`LAST expects array, got ${typeName(arr)}`);
          if (arr.length === 0) throw new RuntimeError('LAST called on empty array');
          return arr[arr.length - 1];
        },
        'CONCAT': (arr1, arr2) => {
          if (!Array.isArray(arr1) || !Array.isArray(arr2)) throw new RuntimeError('CONCAT expects two arrays');
          return [...arr1, ...arr2];
        },
        'KEYS': (m) => {
          if (typeof m !== 'object' || m === null || Array.isArray(m)) throw new RuntimeError(`KEYS expects map, got ${typeName(m)}`);
          return Object.keys(m);
        },
        'VALUES': (m) => {
          if (typeof m !== 'object' || m === null || Array.isArray(m)) throw new RuntimeError(`VALUES expects map, got ${typeName(m)}`);
          return Object.values(m);
        },
        'HAS': (m, key) => {
          if (typeof m !== 'object' || m === null || Array.isArray(m)) throw new RuntimeError(`HAS expects map, got ${typeName(m)}`);
          return key in m;
        },
        'SET': (m, key, value) => {
          if (typeof m !== 'object' || m === null || Array.isArray(m)) throw new RuntimeError(`SET expects map, got ${typeName(m)}`);
          return { ...m, [key]: value };
        },
        'DELETE': (m, key) => {
          if (typeof m !== 'object' || m === null || Array.isArray(m)) throw new RuntimeError(`DELETE expects map, got ${typeName(m)}`);
          const result = { ...m }; delete result[key]; return result;
        },
        'SPLIT': (s, delimiter) => {
          if (typeof s !== 'string' || typeof delimiter !== 'string') throw new RuntimeError('SPLIT expects two strings');
          if (delimiter === '') return [...s];
          return s.split(delimiter);
        },
        'JOIN': (arr, delimiter) => {
          if (!Array.isArray(arr)) throw new RuntimeError(`JOIN expects array, got ${typeName(arr)}`);
          if (typeof delimiter !== 'string') throw new RuntimeError(`JOIN expects string delimiter, got ${typeName(delimiter)}`);
          return arr.map(v => typeof v === 'string' ? v : stringify(v)).join(delimiter);
        },
        'SUBSTRING': (s, start, end) => {
          if (typeof s !== 'string') throw new RuntimeError(`SUBSTRING expects string, got ${typeName(s)}`);
          return s.slice(start, end);
        },
        'UPPERCASE': (s) => {
          if (typeof s !== 'string') throw new RuntimeError(`UPPERCASE expects string, got ${typeName(s)}`);
          return s.toUpperCase();
        },
        'LOWERCASE': (s) => {
          if (typeof s !== 'string') throw new RuntimeError(`LOWERCASE expects string, got ${typeName(s)}`);
          return s.toLowerCase();
        },
        'TRIM': (s) => {
          if (typeof s !== 'string') throw new RuntimeError(`TRIM expects string, got ${typeName(s)}`);
          return s.trim();
        },
        'REPLACE': (s, old, newStr) => {
          if (typeof s !== 'string' || typeof old !== 'string' || typeof newStr !== 'string') throw new RuntimeError('REPLACE expects three strings');
          return s.split(old).join(newStr);
        },
        'RANDOM': () => Math.random(),
        'RANDOM_INT': (min, max) => {
          if (typeof min !== 'number' || typeof max !== 'number') throw new RuntimeError('RANDOM_INT expects two integers');
          return Math.floor(Math.random() * (max - min + 1)) + min;
        },
        'TIME': () => Date.now(),
      };
      return builtins[name] || null;
    }
  }

  // ============ INTERPRETER ============

  class Interpreter {
    constructor(options = {}) {
      this.globalScope = new Scope();
      this.currentScope = this.globalScope;
      this.entities = new Map();
      this.branchEntities = new Set();
      this.builtins = new Builtins(this, options);
      this.thisEntity = null;
      this._pendingPromises = [];
    }

    async run(program) {
      this.thisEntity = new ThisEntity();
      this.entities.set('THIS', this.thisEntity);
      try {
        for (const stmt of program.statements) await this.execute(stmt);
        if (this._pendingPromises.length > 0) await Promise.allSettled(this._pendingPromises);
      } finally {
        for (const entity of this.entities.values()) entity.die();
      }
    }

    async execute(node) {
      switch (node.type) {
        case 'ImportStmt': return this.execImport(node);
        case 'BifurcateStmt': return this.execBifurcate(node);
        case 'AthLoop': return this.execAthLoop(node);
        case 'DieStmt': return this.execDie(node);
        case 'VarDecl': return this.execVarDecl(node);
        case 'ConstDecl': return this.execConstDecl(node);
        case 'Assignment': return this.execAssignment(node);
        case 'RiteDef': return this.execRiteDef(node);
        case 'Conditional': return this.execConditional(node);
        case 'AttemptSalvage': return this.execAttemptSalvage(node);
        case 'CondemnStmt': return this.execCondemn(node);
        case 'BequeathStmt': return this.execBequeath(node);
        case 'ExprStmt': return this.evaluate(node.expression);
        default: throw new RuntimeError(`Unknown statement type: ${node.type}`);
      }
    }

    async execImport(node) {
      const entityType = node.entityType, name = node.name;
      if (this.entities.has(name)) this.entities.get(name).die();
      let entity;
      if (entityType === 'timer') {
        const duration = node.args[0];
        if (duration.type !== 'Duration') throw new RuntimeError('Timer requires a duration', node.line, node.column);
        const ms = this._durationToMs(duration);
        entity = new TimerEntity(name, ms);
      } else {
        throw new RuntimeError(`${entityType} is not supported in browser environment`, node.line, node.column);
      }
      this.entities.set(name, entity);
      this._pendingPromises.push(entity.start());
    }

    _durationToMs(duration) {
      const value = duration.value, unit = duration.unit;
      if (unit === 'ms') return value;
      if (unit === 's') return value * 1000;
      if (unit === 'm') return value * 60 * 1000;
      if (unit === 'h') return value * 60 * 60 * 1000;
      return value;
    }

    async execBifurcate(node) {
      const entityName = node.entity, branch1Name = node.branch1, branch2Name = node.branch2;
      if (!this.entities.has(entityName)) throw new RuntimeError(`Cannot bifurcate unknown entity: ${entityName}`, node.line, node.column);
      const branch1 = new BranchEntity(branch1Name);
      const branch2 = new BranchEntity(branch2Name);
      this.entities.set(branch1Name, branch1);
      this.entities.set(branch2Name, branch2);
      this.branchEntities.add(branch1Name);
      this.branchEntities.add(branch2Name);
    }

    async execAthLoop(node) {
      const entityExpr = node.entityExpr;
      if (entityExpr.type === 'EntityIdent' && this.branchEntities.has(entityExpr.name)) {
        return this.execBranchMode(node, entityExpr.name);
      }
      const entity = await this.resolveEntityExpr(entityExpr);
      await entity.waitForDeath();
      await this.execStatements(node.execute);
    }

    async execBranchMode(node, branchName) {
      const branchEntity = this.entities.get(branchName);
      const runBranch = async () => {
        try {
          for (const stmt of node.body) await this.execute(stmt);
          await this.execStatements(node.execute);
          branchEntity.complete();
        } catch (e) { branchEntity.complete(); throw e; }
      };
      this._pendingPromises.push(runBranch());
      await new Promise(resolve => setTimeout(resolve, 0));
    }

    async resolveEntityExpr(expr) {
      switch (expr.type) {
        case 'EntityIdent': {
          const name = expr.name;
          if (!this.entities.has(name)) throw new RuntimeError(`Unknown entity: ${name}`, expr.line, expr.column);
          return this.entities.get(name);
        }
        case 'EntityAnd': {
          const left = await this.resolveEntityExpr(expr.left);
          const right = await this.resolveEntityExpr(expr.right);
          const composite = new CompositeEntity(`(${left.name} && ${right.name})`, 'AND', [left, right]);
          this._pendingPromises.push(composite.start());
          return composite;
        }
        case 'EntityOr': {
          const left = await this.resolveEntityExpr(expr.left);
          const right = await this.resolveEntityExpr(expr.right);
          const composite = new CompositeEntity(`(${left.name} || ${right.name})`, 'OR', [left, right]);
          this._pendingPromises.push(composite.start());
          return composite;
        }
        case 'EntityNot': {
          const inner = await this.resolveEntityExpr(expr.operand);
          const composite = new CompositeEntity(`(!${inner.name})`, 'NOT', [inner]);
          this._pendingPromises.push(composite.start());
          return composite;
        }
        default: throw new RuntimeError('Unknown entity expression type', expr.line, expr.column);
      }
    }

    async execDie(node) { await this._killTarget(node.target); }
    async _killTarget(target) {
      if (target.type === 'DieIdent') {
        const name = target.name;
        if (!this.entities.has(name)) throw new RuntimeError(`Unknown entity: ${name}`, target.line, target.column);
        this.entities.get(name).die();
      } else if (target.type === 'DiePair') {
        await this._killTarget(target.left);
        await this._killTarget(target.right);
      }
    }

    async execVarDecl(node) {
      const value = await this.evaluate(node.value);
      this.currentScope.define(node.name, value, false);
    }

    async execConstDecl(node) {
      const value = await this.evaluate(node.value);
      this.currentScope.define(node.name, value, true);
    }

    async execAssignment(node) {
      const value = await this.evaluate(node.value);
      const target = node.target;
      if (target.type === 'Identifier') {
        this.currentScope.set(target.name, value);
      } else if (target.type === 'IndexExpr') {
        const obj = await this.evaluate(target.obj);
        const index = await this.evaluate(target.index);
        if (Array.isArray(obj)) obj[index] = value;
        else if (typeof obj === 'object' && obj !== null) obj[String(index)] = value;
        else throw new RuntimeError('Cannot index non-collection', node.line, node.column);
      } else if (target.type === 'MemberExpr') {
        const obj = await this.evaluate(target.obj);
        if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) obj[target.member] = value;
        else throw new RuntimeError('Cannot access member of non-map', node.line, node.column);
      } else {
        throw new RuntimeError('Invalid assignment target', node.line, node.column);
      }
    }

    async execRiteDef(node) {
      const rite = new UserRite(node.name, node.params, node.body, this.currentScope);
      this.currentScope.define(node.name, rite, true);
    }

    async execConditional(node) {
      const condition = await this.evaluate(node.condition);
      if (isTruthy(condition)) await this.execStatements(node.thenBranch);
      else if (node.elseBranch) await this.execStatements(node.elseBranch);
    }

    async execAttemptSalvage(node) {
      try {
        await this.execStatements(node.attemptBody);
      } catch (e) {
        if (e instanceof RuntimeError || e instanceof CondemnError) {
          const oldScope = this.currentScope;
          this.currentScope = new Scope(oldScope);
          this.currentScope.define(node.errorName, e.tildeAthMessage || String(e));
          try { await this.execStatements(node.salvageBody); }
          finally { this.currentScope = oldScope; }
        } else { throw e; }
      }
    }

    async execCondemn(node) {
      const message = await this.evaluate(node.message);
      throw new CondemnError(stringify(message), node.line, node.column);
    }

    async execBequeath(node) {
      let value = null;
      if (node.value) value = await this.evaluate(node.value);
      throw new BequeathError(value);
    }

    async execStatements(statements) {
      for (const stmt of statements) await this.execute(stmt);
    }

    async evaluate(node) {
      switch (node.type) {
        case 'Literal': return node.value;
        case 'Identifier': {
          const name = node.name;
          if (name === 'THIS') return this.thisEntity;
          const builtin = this.builtins.get(name);
          if (builtin) return builtin;
          return this.currentScope.get(name);
        }
        case 'BinaryOp': return this.evalBinaryOp(node);
        case 'UnaryOp': return this.evalUnaryOp(node);
        case 'CallExpr': return this.evalCall(node);
        case 'IndexExpr': return this.evalIndex(node);
        case 'MemberExpr': return this.evalMember(node);
        case 'ArrayLiteral': {
          const elements = [];
          for (const e of node.elements) elements.push(await this.evaluate(e));
          return elements;
        }
        case 'MapLiteral': {
          const result = {};
          for (const [key, value] of node.entries) result[key] = await this.evaluate(value);
          return result;
        }
        default: throw new RuntimeError(`Unknown expression type: ${node.type}`);
      }
    }

    async evalBinaryOp(node) {
      const op = node.operator;
      if (op === 'AND') {
        const left = await this.evaluate(node.left);
        if (!isTruthy(left)) return left;
        return this.evaluate(node.right);
      }
      if (op === 'OR') {
        const left = await this.evaluate(node.left);
        if (isTruthy(left)) return left;
        return this.evaluate(node.right);
      }
      const left = await this.evaluate(node.left);
      const right = await this.evaluate(node.right);
      switch (op) {
        case '+':
          if (typeof left === 'string' || typeof right === 'string') return stringify(left) + stringify(right);
          if (typeof left === 'number' && typeof right === 'number') return left + right;
          throw new RuntimeError(`Cannot add ${stringify(left)} and ${stringify(right)}`, node.line, node.column);
        case '-':
          if (typeof left === 'number' && typeof right === 'number') return left - right;
          throw new RuntimeError(`Cannot subtract`, node.line, node.column);
        case '*':
          if (typeof left === 'number' && typeof right === 'number') return left * right;
          throw new RuntimeError(`Cannot multiply`, node.line, node.column);
        case '/':
          if (typeof left === 'number' && typeof right === 'number') {
            if (right === 0) throw new RuntimeError('Division by zero', node.line, node.column);
            if (Number.isInteger(left) && Number.isInteger(right)) return Math.trunc(left / right);
            return left / right;
          }
          throw new RuntimeError(`Cannot divide`, node.line, node.column);
        case '%':
          if (Number.isInteger(left) && Number.isInteger(right)) {
            if (right === 0) throw new RuntimeError('Modulo by zero', node.line, node.column);
            return left % right;
          }
          throw new RuntimeError(`Cannot modulo`, node.line, node.column);
        case '==': return left === right;
        case '!=': return left !== right;
        case '<': return left < right;
        case '>': return left > right;
        case '<=': return left <= right;
        case '>=': return left >= right;
        default: throw new RuntimeError(`Unknown operator: ${op}`, node.line, node.column);
      }
    }

    async evalUnaryOp(node) {
      const operand = await this.evaluate(node.operand);
      if (node.operator === 'NOT') return !isTruthy(operand);
      if (node.operator === '-') {
        if (typeof operand === 'number') return -operand;
        throw new RuntimeError(`Cannot negate ${stringify(operand)}`, node.line, node.column);
      }
      throw new RuntimeError(`Unknown unary operator: ${node.operator}`, node.line, node.column);
    }

    async evalCall(node) {
      const callee = await this.evaluate(node.callee);
      const args = [];
      for (const arg of node.args) args.push(await this.evaluate(arg));
      if (typeof callee === 'function') {
        try { return callee(...args); }
        catch (e) {
          if (e instanceof RuntimeError) throw e;
          throw new RuntimeError(String(e), node.line, node.column);
        }
      }
      if (callee instanceof UserRite) return this.callRite(callee, args, node);
      throw new RuntimeError(`Cannot call ${stringify(callee)}`, node.line, node.column);
    }

    async callRite(rite, args, node) {
      if (args.length !== rite.params.length) {
        throw new RuntimeError(`Rite '${rite.name}' expects ${rite.params.length} arguments, got ${args.length}`, node.line, node.column);
      }
      const oldScope = this.currentScope;
      this.currentScope = new Scope(rite.closure);
      for (let i = 0; i < rite.params.length; i++) this.currentScope.define(rite.params[i], args[i]);
      try {
        for (const stmt of rite.body) await this.execute(stmt);
        return null;
      } catch (e) {
        if (e instanceof BequeathError) return e.value;
        throw e;
      } finally {
        this.currentScope = oldScope;
      }
    }

    async evalIndex(node) {
      const obj = await this.evaluate(node.obj);
      const index = await this.evaluate(node.index);
      if (Array.isArray(obj)) {
        if (typeof index !== 'number' || !Number.isInteger(index)) throw new RuntimeError('Array index must be an integer', node.line, node.column);
        if (index < 0 || index >= obj.length) throw new RuntimeError(`Array index out of bounds: ${index}`, node.line, node.column);
        return obj[index];
      }
      if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
        const key = String(index);
        if (!(key in obj)) throw new RuntimeError(`Key not found in map: ${key}`, node.line, node.column);
        return obj[key];
      }
      if (typeof obj === 'string') {
        if (typeof index !== 'number' || !Number.isInteger(index)) throw new RuntimeError('String index must be an integer', node.line, node.column);
        if (index < 0 || index >= obj.length) throw new RuntimeError(`String index out of bounds: ${index}`, node.line, node.column);
        return obj[index];
      }
      throw new RuntimeError(`Cannot index ${stringify(obj)}`, node.line, node.column);
    }

    async evalMember(node) {
      const obj = await this.evaluate(node.obj);
      if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
        if (!(node.member in obj)) throw new RuntimeError(`Key not found in map: ${node.member}`, node.line, node.column);
        return obj[node.member];
      }
      throw new RuntimeError(`Cannot access member of ${stringify(obj)}`, node.line, node.column);
    }
  }

  // ============ PUBLIC API ============

  class TildeAth {
    constructor(options = {}) {
      this.options = {
        onOutput: options.onOutput || ((text) => console.log(text)),
        onInput: options.onInput || null,
        inputQueue: options.inputQueue || [],
      };
    }

    parse(source) {
      const lexer = new Lexer(source);
      const tokens = lexer.tokenize();
      const parser = new Parser(tokens);
      return parser.parse();
    }

    async run(source) {
      const program = this.parse(source);
      const interpreter = new Interpreter(this.options);
      await interpreter.run(program);
    }
  }

  // Export to global
  global.TildeAth = TildeAth;
  global.TildeAthError = TildeAthError;
  global.LexerError = LexerError;
  global.ParseError = ParseError;
  global.RuntimeError = RuntimeError;
  global.CondemnError = CondemnError;

})(typeof window !== 'undefined' ? window : this);
