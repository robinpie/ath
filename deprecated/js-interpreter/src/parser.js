/**
 * Parser for the !~ATH language.
 */

import { ParseError } from './errors.js';
import { TokenType } from './tokens.js';
import * as ast from './ast.js';

export class Parser {
  constructor(tokens) {
    this.tokens = tokens;
    this.pos = 0;
  }

  error(message, token = null) {
    if (token === null) {
      token = this.current();
    }
    return new ParseError(message, token.line, token.column);
  }

  current() {
    if (this.pos >= this.tokens.length) {
      return this.tokens[this.tokens.length - 1];  // EOF
    }
    return this.tokens[this.pos];
  }

  peek(offset = 0) {
    const pos = this.pos + offset;
    if (pos >= this.tokens.length) {
      return this.tokens[this.tokens.length - 1];
    }
    return this.tokens[pos];
  }

  check(...types) {
    return types.includes(this.current().type);
  }

  advance() {
    const token = this.current();
    if (this.pos < this.tokens.length) {
      this.pos++;
    }
    return token;
  }

  consume(tokenType, message) {
    if (this.check(tokenType)) {
      return this.advance();
    }
    throw this.error(message);
  }

  match(...types) {
    if (this.check(...types)) {
      this.advance();
      return true;
    }
    return false;
  }

  // ============ Program ============

  parse() {
    const statements = [];
    while (!this.check(TokenType.EOF)) {
      statements.push(this.parseStatement());
    }
    return ast.createProgram(statements);
  }

  // ============ Statements ============

  parseStatement() {
    const token = this.current();

    if (this.check(TokenType.IMPORT)) {
      return this.parseImport();
    }
    if (this.check(TokenType.BIFURCATE)) {
      return this.parseBifurcate();
    }
    if (this.check(TokenType.TILDE_ATH)) {
      return this.parseAthLoop();
    }
    if (this.check(TokenType.BIRTH)) {
      return this.parseVarDecl();
    }
    if (this.check(TokenType.ENTOMB)) {
      return this.parseConstDecl();
    }
    if (this.check(TokenType.RITE)) {
      return this.parseRiteDef();
    }
    if (this.check(TokenType.SHOULD)) {
      return this.parseConditional();
    }
    if (this.check(TokenType.ATTEMPT)) {
      return this.parseAttemptSalvage();
    }
    if (this.check(TokenType.CONDEMN)) {
      return this.parseCondemn();
    }
    if (this.check(TokenType.BEQUEATH)) {
      return this.parseBequeath();
    }

    // Check for DIE statement: IDENTIFIER.DIE() or [targets].DIE()
    if (this.check(TokenType.IDENTIFIER) || this.check(TokenType.LBRACKET)) {
      return this.parseDieOrAssignmentOrExpr();
    }

    // Check for THIS.DIE()
    if (this.check(TokenType.THIS)) {
      return this.parseDieOrExpr();
    }

    throw this.error(`Unexpected token: ${token.type}`);
  }

  parseImport() {
    const token = this.advance();  // consume 'import'
    const line = token.line;
    const col = token.column;

    // Entity type
    if (!this.check(TokenType.TIMER, TokenType.PROCESS, TokenType.CONNECTION, TokenType.WATCHER)) {
      throw this.error('Expected entity type (timer, process, connection, watcher)');
    }

    const entityType = this.advance().value;

    // Identifier
    const nameToken = this.consume(TokenType.IDENTIFIER, "Expected entity name");
    const name = nameToken.value;

    this.consume(TokenType.LPAREN, "Expected '(' after entity name");

    // Parse arguments
    const args = [];
    if (entityType === 'timer') {
      // Timer expects a duration
      if (this.check(TokenType.DURATION)) {
        const durToken = this.advance();
        args.push(ast.createDuration(durToken.value.unit, durToken.value.value, durToken.line, durToken.column));
      } else if (this.check(TokenType.INTEGER)) {
        // Plain integer = milliseconds
        const intToken = this.advance();
        args.push(ast.createDuration('ms', intToken.value, intToken.line, intToken.column));
      } else {
        throw this.error('Expected duration for timer');
      }
    } else {
      // Other entities expect expressions
      if (!this.check(TokenType.RPAREN)) {
        args.push(this.parseExpression());
        while (this.match(TokenType.COMMA)) {
          args.push(this.parseExpression());
        }
      }
    }

    this.consume(TokenType.RPAREN, "Expected ')' after import arguments");
    this.consume(TokenType.SEMICOLON, "Expected ';' after import statement");

    return ast.createImportStmt(entityType, name, args, line, col);
  }

  parseBifurcate() {
    const token = this.advance();  // consume 'bifurcate'
    const line = token.line;
    const col = token.column;

    // Entity being bifurcated
    let entity;
    if (this.check(TokenType.THIS)) {
      entity = this.advance().value;
    } else {
      entity = this.consume(TokenType.IDENTIFIER, "Expected entity to bifurcate").value;
    }

    this.consume(TokenType.LBRACKET, "Expected '[' after entity");

    const branch1 = this.consume(TokenType.IDENTIFIER, "Expected first branch name").value;
    this.consume(TokenType.COMMA, "Expected ',' between branch names");
    const branch2 = this.consume(TokenType.IDENTIFIER, "Expected second branch name").value;

    this.consume(TokenType.RBRACKET, "Expected ']' after branch names");
    this.consume(TokenType.SEMICOLON, "Expected ';' after bifurcate statement");

    return ast.createBifurcateStmt(entity, branch1, branch2, line, col);
  }

  parseAthLoop() {
    const token = this.advance();  // consume '~ATH'
    const line = token.line;
    const col = token.column;

    this.consume(TokenType.LPAREN, "Expected '(' after ~ATH");
    const entityExpr = this.parseEntityExpr();
    this.consume(TokenType.RPAREN, "Expected ')' after entity expression");

    this.consume(TokenType.LBRACE, "Expected '{' for ~ATH body");

    // Parse body (can contain any statements in branch mode, only ~ATH in wait mode)
    // We'll do semantic checking later
    const body = [];
    while (!this.check(TokenType.RBRACE)) {
      body.push(this.parseStatement());
    }

    this.consume(TokenType.RBRACE, "Expected '}' after ~ATH body");
    this.consume(TokenType.EXECUTE, "Expected 'EXECUTE' after ~ATH body");
    this.consume(TokenType.LPAREN, "Expected '(' after EXECUTE");

    // Parse execute body
    const execute = this.parseExecuteBody();

    this.consume(TokenType.RPAREN, "Expected ')' after EXECUTE body");
    this.consume(TokenType.SEMICOLON, "Expected ';' after ~ATH loop");

    return ast.createAthLoop(entityExpr, body, execute, line, col);
  }

  parseExecuteBody() {
    const statements = [];

    while (!this.check(TokenType.RPAREN)) {
      // Try to parse a statement
      const stmt = this.parseExecuteStatement();
      if (stmt !== null) {
        statements.push(stmt);
      }

      // Check if we have a trailing expression without semicolon
      if (this.check(TokenType.RPAREN)) {
        break;
      }
    }

    return statements;
  }

  parseExecuteStatement() {
    const token = this.current();

    if (this.check(TokenType.IMPORT)) {
      return this.parseImport();
    }
    if (this.check(TokenType.TILDE_ATH)) {
      return this.parseAthLoop();
    }
    if (this.check(TokenType.BIRTH)) {
      return this.parseVarDecl();
    }
    if (this.check(TokenType.ENTOMB)) {
      return this.parseConstDecl();
    }
    if (this.check(TokenType.RITE)) {
      return this.parseRiteDef();
    }
    if (this.check(TokenType.SHOULD)) {
      return this.parseConditional();
    }
    if (this.check(TokenType.ATTEMPT)) {
      return this.parseAttemptSalvage();
    }
    if (this.check(TokenType.CONDEMN)) {
      return this.parseCondemn();
    }
    if (this.check(TokenType.BEQUEATH)) {
      return this.parseBequeath();
    }

    // Check for VOID literal as no-op
    if (this.check(TokenType.VOID)) {
      const voidToken = this.advance();
      // Optional semicolon
      this.match(TokenType.SEMICOLON);
      return ast.createExprStmt(
        ast.createLiteral(null, voidToken.line, voidToken.column),
        voidToken.line,
        voidToken.column
      );
    }

    // Expression (possibly with assignment)
    const expr = this.parseExpression();

    // Check for assignment
    if (this.check(TokenType.ASSIGN)) {
      this.advance();
      const value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
      return ast.createAssignment(expr, value, expr.line, expr.column);
    }

    // Expression statement
    // Semicolon is optional for the last statement in EXECUTE
    this.match(TokenType.SEMICOLON);
    return ast.createExprStmt(expr, expr.line, expr.column);
  }

  parseVarDecl() {
    const token = this.advance();  // consume 'BIRTH'
    const line = token.line;
    const col = token.column;

    const name = this.consume(TokenType.IDENTIFIER, "Expected variable name").value;
    this.consume(TokenType.WITH, "Expected 'WITH' after variable name");
    const value = this.parseExpression();
    this.consume(TokenType.SEMICOLON, "Expected ';' after variable declaration");

    return ast.createVarDecl(name, value, line, col);
  }

  parseConstDecl() {
    const token = this.advance();  // consume 'ENTOMB'
    const line = token.line;
    const col = token.column;

    const name = this.consume(TokenType.IDENTIFIER, "Expected constant name").value;
    this.consume(TokenType.WITH, "Expected 'WITH' after constant name");
    const value = this.parseExpression();
    this.consume(TokenType.SEMICOLON, "Expected ';' after constant declaration");

    return ast.createConstDecl(name, value, line, col);
  }

  parseRiteDef() {
    const token = this.advance();  // consume 'RITE'
    const line = token.line;
    const col = token.column;

    const name = this.consume(TokenType.IDENTIFIER, "Expected rite name").value;
    this.consume(TokenType.LPAREN, "Expected '(' after rite name");

    // Parse parameters
    const params = [];
    if (!this.check(TokenType.RPAREN)) {
      params.push(this.consume(TokenType.IDENTIFIER, "Expected parameter name").value);
      while (this.match(TokenType.COMMA)) {
        params.push(this.consume(TokenType.IDENTIFIER, "Expected parameter name").value);
      }
    }

    this.consume(TokenType.RPAREN, "Expected ')' after parameters");
    this.consume(TokenType.LBRACE, "Expected '{' for rite body");

    // Parse body
    const body = [];
    while (!this.check(TokenType.RBRACE)) {
      body.push(this.parseExecuteStatement());
    }

    this.consume(TokenType.RBRACE, "Expected '}' after rite body");

    return ast.createRiteDef(name, params, body, line, col);
  }

  parseConditional() {
    const token = this.advance();  // consume 'SHOULD'
    const line = token.line;
    const col = token.column;

    const condition = this.parseExpression();
    this.consume(TokenType.LBRACE, "Expected '{' after condition");

    const thenBranch = [];
    while (!this.check(TokenType.RBRACE)) {
      thenBranch.push(this.parseExecuteStatement());
    }

    this.consume(TokenType.RBRACE, "Expected '}' after then branch");

    let elseBranch = null;
    if (this.match(TokenType.LEST)) {
      if (this.check(TokenType.SHOULD)) {
        // Chained conditional
        elseBranch = [this.parseConditional()];
      } else {
        this.consume(TokenType.LBRACE, "Expected '{' after LEST");
        elseBranch = [];
        while (!this.check(TokenType.RBRACE)) {
          elseBranch.push(this.parseExecuteStatement());
        }
        this.consume(TokenType.RBRACE, "Expected '}' after else branch");
      }
    }

    return ast.createConditional(condition, thenBranch, elseBranch, line, col);
  }

  parseAttemptSalvage() {
    const token = this.advance();  // consume 'ATTEMPT'
    const line = token.line;
    const col = token.column;

    this.consume(TokenType.LBRACE, "Expected '{' after ATTEMPT");

    const attemptBody = [];
    while (!this.check(TokenType.RBRACE)) {
      attemptBody.push(this.parseExecuteStatement());
    }

    this.consume(TokenType.RBRACE, "Expected '}' after ATTEMPT body");
    this.consume(TokenType.SALVAGE, "Expected 'SALVAGE' after ATTEMPT block");

    const errorName = this.consume(TokenType.IDENTIFIER, "Expected error variable name").value;

    this.consume(TokenType.LBRACE, "Expected '{' after error variable");

    const salvageBody = [];
    while (!this.check(TokenType.RBRACE)) {
      salvageBody.push(this.parseExecuteStatement());
    }

    this.consume(TokenType.RBRACE, "Expected '}' after SALVAGE body");

    return ast.createAttemptSalvage(attemptBody, errorName, salvageBody, line, col);
  }

  parseCondemn() {
    const token = this.advance();  // consume 'CONDEMN'
    const line = token.line;
    const col = token.column;

    const message = this.parseExpression();
    this.consume(TokenType.SEMICOLON, "Expected ';' after CONDEMN");

    return ast.createCondemnStmt(message, line, col);
  }

  parseBequeath() {
    const token = this.advance();  // consume 'BEQUEATH'
    const line = token.line;
    const col = token.column;

    let value = null;
    if (!this.check(TokenType.SEMICOLON)) {
      value = this.parseExpression();
    }

    this.consume(TokenType.SEMICOLON, "Expected ';' after BEQUEATH");

    return ast.createBequeathStmt(value, line, col);
  }

  parseDieOrAssignmentOrExpr() {
    // Check for [targets].DIE() pattern
    if (this.check(TokenType.LBRACKET)) {
      const target = this.parseDieTarget();
      this.consume(TokenType.DOT, "Expected '.' after die target");
      this.consume(TokenType.DIE, "Expected 'DIE' after '.'");
      this.consume(TokenType.LPAREN, "Expected '(' after DIE");
      this.consume(TokenType.RPAREN, "Expected ')' after DIE(");
      this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
      return ast.createDieStmt(target, target.line, target.column);
    }

    // Must be identifier - could be assignment, DIE, or expression
    const expr = this.parseExpression();

    // Check for .DIE()
    if (expr.type === 'MemberExpr' && expr.member === 'DIE') {
      throw this.error("DIE must be called as ENTITY.DIE(), not used as expression");
    }

    // Check for assignment
    if (this.check(TokenType.ASSIGN)) {
      this.advance();
      const value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
      return ast.createAssignment(expr, value, expr.line, expr.column);
    }

    // Check for DIE call
    if (expr.type === 'CallExpr') {
      if (expr.callee.type === 'MemberExpr' && expr.callee.member === 'DIE') {
        // Convert to DieStmt
        const obj = expr.callee.obj;
        if (obj.type === 'Identifier') {
          const target = ast.createDieIdent(obj.name, obj.line, obj.column);
          this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
          return ast.createDieStmt(target, expr.line, expr.column);
        } else {
          throw this.error("Invalid DIE target");
        }
      }
    }

    // Expression statement
    this.consume(TokenType.SEMICOLON, "Expected ';' after expression");
    return ast.createExprStmt(expr, expr.line, expr.column);
  }

  parseDieOrExpr() {
    const token = this.current();
    const line = token.line;
    const col = token.column;

    const expr = this.parseExpression();

    // Check for assignment (shouldn't happen with THIS but handle it)
    if (this.check(TokenType.ASSIGN)) {
      this.advance();
      const value = this.parseExpression();
      this.consume(TokenType.SEMICOLON, "Expected ';' after assignment");
      return ast.createAssignment(expr, value, line, col);
    }

    // Check for DIE call
    if (expr.type === 'CallExpr') {
      if (expr.callee.type === 'MemberExpr' && expr.callee.member === 'DIE') {
        const obj = expr.callee.obj;
        if (obj.type === 'Identifier' && obj.name === 'THIS') {
          const target = ast.createDieIdent('THIS', obj.line, obj.column);
          this.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement");
          return ast.createDieStmt(target, line, col);
        }
      }
    }

    // Expression statement
    this.consume(TokenType.SEMICOLON, "Expected ';' after expression");
    return ast.createExprStmt(expr, line, col);
  }

  parseDieTarget() {
    if (this.check(TokenType.LBRACKET)) {
      const token = this.advance();
      const line = token.line;
      const col = token.column;
      const left = this.parseDieTarget();
      this.consume(TokenType.COMMA, "Expected ',' in die target pair");
      const right = this.parseDieTarget();
      this.consume(TokenType.RBRACKET, "Expected ']' after die target pair");
      return ast.createDiePair(left, right, line, col);
    } else {
      if (this.check(TokenType.THIS)) {
        const token = this.advance();
        return ast.createDieIdent('THIS', token.line, token.column);
      }
      const token = this.consume(TokenType.IDENTIFIER, "Expected identifier in die target");
      return ast.createDieIdent(token.value, token.line, token.column);
    }
  }

  // ============ Entity Expressions ============

  parseEntityExpr() {
    return this.parseEntityOr();
  }

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

  // ============ Expressions ============

  parseExpression() {
    return this.parseOr();
  }

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
    let left = this.parseBitwiseOr();

    while (this.check(TokenType.LT, TokenType.GT, TokenType.LE, TokenType.GE)) {
      const token = this.advance();
      const opMap = {
        [TokenType.LT]: '<',
        [TokenType.GT]: '>',
        [TokenType.LE]: '<=',
        [TokenType.GE]: '>=',
      };
      const right = this.parseBitwiseOr();
      left = ast.createBinaryOp(opMap[token.type], left, right, token.line, token.column);
    }

    return left;
  }

  parseBitwiseOr() {
    let left = this.parseBitwiseXor();

    while (this.match(TokenType.PIPE)) {
      const token = this.tokens[this.pos - 1];
      const right = this.parseBitwiseXor();
      left = ast.createBinaryOp('|', left, right, token.line, token.column);
    }

    return left;
  }

  parseBitwiseXor() {
    let left = this.parseBitwiseAnd();

    while (this.match(TokenType.CARET)) {
      const token = this.tokens[this.pos - 1];
      const right = this.parseBitwiseAnd();
      left = ast.createBinaryOp('^', left, right, token.line, token.column);
    }

    return left;
  }

  parseBitwiseAnd() {
    let left = this.parseShift();

    while (this.match(TokenType.AMP)) {
      const token = this.tokens[this.pos - 1];
      const right = this.parseShift();
      left = ast.createBinaryOp('&', left, right, token.line, token.column);
    }

    return left;
  }

  parseShift() {
    let left = this.parseTerm();

    while (this.check(TokenType.LSHIFT, TokenType.RSHIFT)) {
      const token = this.advance();
      const op = token.type === TokenType.LSHIFT ? '<<' : '>>';
      const right = this.parseTerm();
      left = ast.createBinaryOp(op, left, right, token.line, token.column);
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
      const opMap = {
        [TokenType.STAR]: '*',
        [TokenType.SLASH]: '/',
        [TokenType.PERCENT]: '%',
      };
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

    if (this.match(TokenType.TILDE)) {
      const token = this.tokens[this.pos - 1];
      const operand = this.parseUnary();
      return ast.createUnaryOp('~', operand, token.line, token.column);
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
        // Special handling for .DIE (DIE is a keyword, not identifier)
        if (this.check(TokenType.DIE)) {
          const dieToken = this.advance();
          // Must be followed by ()
          if (this.check(TokenType.LPAREN)) {
            this.advance();  // consume (
            this.consume(TokenType.RPAREN, "Expected ')' after DIE(");
            // Create a call expression for DIE
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
        // Function call
        const args = [];
        if (!this.check(TokenType.RPAREN)) {
          args.push(this.parseExpression());
          while (this.match(TokenType.COMMA)) {
            args.push(this.parseExpression());
          }
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

    if (this.match(TokenType.INTEGER)) {
      return ast.createLiteral(token.value, token.line, token.column);
    }

    if (this.match(TokenType.FLOAT)) {
      return ast.createLiteral(token.value, token.line, token.column);
    }

    if (this.match(TokenType.STRING)) {
      return ast.createLiteral(token.value, token.line, token.column);
    }

    if (this.match(TokenType.ALIVE)) {
      return ast.createLiteral(true, token.line, token.column);
    }

    if (this.match(TokenType.DEAD)) {
      return ast.createLiteral(false, token.line, token.column);
    }

    if (this.match(TokenType.VOID)) {
      return ast.createLiteral(null, token.line, token.column);
    }

    if (this.match(TokenType.THIS)) {
      return ast.createIdentifier('THIS', token.line, token.column);
    }

    if (this.match(TokenType.IDENTIFIER)) {
      return ast.createIdentifier(token.value, token.line, token.column);
    }

    if (this.match(TokenType.LPAREN)) {
      const expr = this.parseExpression();
      this.consume(TokenType.RPAREN, "Expected ')' after expression");
      return expr;
    }

    if (this.match(TokenType.LBRACKET)) {
      return this.parseArrayLiteral(token);
    }

    if (this.match(TokenType.LBRACE)) {
      return this.parseMapLiteral(token);
    }

    throw this.error(`Unexpected token in expression: ${token.type}`);
  }

  parseArrayLiteral(startToken) {
    const elements = [];
    if (!this.check(TokenType.RBRACKET)) {
      elements.push(this.parseExpression());
      while (this.match(TokenType.COMMA)) {
        if (this.check(TokenType.RBRACKET)) {
          break;  // Trailing comma
        }
        elements.push(this.parseExpression());
      }
    }
    this.consume(TokenType.RBRACKET, "Expected ']' after array elements");
    return ast.createArrayLiteral(elements, startToken.line, startToken.column);
  }

  parseMapLiteral(startToken) {
    const entries = [];
    if (!this.check(TokenType.RBRACE)) {
      // Parse first entry
      const key = this.parseMapKey();
      this.consume(TokenType.COLON, "Expected ':' after map key");
      const value = this.parseExpression();
      entries.push([key, value]);

      while (this.match(TokenType.COMMA)) {
        if (this.check(TokenType.RBRACE)) {
          break;  // Trailing comma
        }
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
    if (this.check(TokenType.STRING)) {
      return this.advance().value;
    }
    if (this.check(TokenType.IDENTIFIER)) {
      return this.advance().value;
    }
    throw this.error("Expected map key (identifier or string)");
  }
}
