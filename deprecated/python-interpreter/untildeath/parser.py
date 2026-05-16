"""Parser for the !~ATH language."""

from typing import List, Optional

from .lexer import Token, TokenType
from .ast_nodes import (
    Program, ImportStmt, BifurcateStmt, AthLoop, DieStmt,
    VarDecl, ConstDecl, Assignment, RiteDef, Conditional,
    AttemptSalvage, CondemnStmt, BequeathStmt, ExprStmt,
    EntityAnd, EntityOr, EntityNot, EntityIdent,
    DieIdent, DiePair,
    Literal, Identifier, BinaryOp, UnaryOp, CallExpr,
    IndexExpr, MemberExpr, ArrayLiteral, MapLiteral, Duration
)
from .errors import ParseError


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def error(self, message: str, token: Token = None) -> ParseError:
        if token is None:
            token = self.current()
        return ParseError(message, token.line, token.column)

    def current(self) -> Token:
        if self.pos >= len(self.tokens):
            return self.tokens[-1]  # EOF
        return self.tokens[self.pos]

    def peek(self, offset: int = 0) -> Token:
        pos = self.pos + offset
        if pos >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[pos]

    def check(self, *types: TokenType) -> bool:
        return self.current().type in types

    def advance(self) -> Token:
        token = self.current()
        if self.pos < len(self.tokens):
            self.pos += 1
        return token

    def consume(self, token_type: TokenType, message: str) -> Token:
        if self.check(token_type):
            return self.advance()
        raise self.error(message)

    def match(self, *types: TokenType) -> bool:
        if self.check(*types):
            self.advance()
            return True
        return False

    # ============ Program ============

    def parse(self) -> Program:
        statements = []
        while not self.check(TokenType.EOF):
            statements.append(self.parse_statement())
        return Program(statements=statements)

    # ============ Statements ============

    def parse_statement(self):
        token = self.current()

        if self.check(TokenType.IMPORT):
            return self.parse_import()
        if self.check(TokenType.BIFURCATE):
            return self.parse_bifurcate()
        if self.check(TokenType.TILDE_ATH):
            return self.parse_ath_loop()
        if self.check(TokenType.BIRTH):
            return self.parse_var_decl()
        if self.check(TokenType.ENTOMB):
            return self.parse_const_decl()
        if self.check(TokenType.RITE):
            return self.parse_rite_def()
        if self.check(TokenType.SHOULD):
            return self.parse_conditional()
        if self.check(TokenType.ATTEMPT):
            return self.parse_attempt_salvage()
        if self.check(TokenType.CONDEMN):
            return self.parse_condemn()
        if self.check(TokenType.BEQUEATH):
            return self.parse_bequeath()

        # Check for DIE statement: IDENTIFIER.DIE() or [targets].DIE()
        if self.check(TokenType.IDENTIFIER) or self.check(TokenType.LBRACKET):
            return self.parse_die_or_assignment_or_expr()

        # Check for THIS.DIE()
        if self.check(TokenType.THIS):
            return self.parse_die_or_expr()

        raise self.error(f"Unexpected token: {token.type.name}")

    def parse_import(self):
        token = self.advance()  # consume 'import'
        line, col = token.line, token.column

        # Entity type
        if not self.check(TokenType.TIMER, TokenType.PROCESS,
                          TokenType.CONNECTION, TokenType.WATCHER):
            raise self.error("Expected entity type (timer, process, connection, watcher)")

        entity_type = self.advance().value

        # Identifier
        name_token = self.consume(TokenType.IDENTIFIER, "Expected entity name")
        name = name_token.value

        self.consume(TokenType.LPAREN, "Expected '(' after entity name")

        # Parse arguments
        args = []
        if entity_type == 'timer':
            # Timer expects a duration
            if self.check(TokenType.DURATION):
                dur_token = self.advance()
                unit, value = dur_token.value
                args.append(Duration(unit=unit, value=value, line=dur_token.line, column=dur_token.column))
            elif self.check(TokenType.INTEGER):
                # Plain integer = milliseconds
                int_token = self.advance()
                args.append(Duration(unit='ms', value=int_token.value, line=int_token.line, column=int_token.column))
            else:
                raise self.error("Expected duration for timer")
        else:
            # Other entities expect expressions
            if not self.check(TokenType.RPAREN):
                args.append(self.parse_expression())
                while self.match(TokenType.COMMA):
                    args.append(self.parse_expression())

        self.consume(TokenType.RPAREN, "Expected ')' after import arguments")
        self.consume(TokenType.SEMICOLON, "Expected ';' after import statement")

        return ImportStmt(entity_type=entity_type, name=name, args=args, line=line, column=col)

    def parse_bifurcate(self):
        token = self.advance()  # consume 'bifurcate'
        line, col = token.line, token.column

        # Entity being bifurcated
        if self.check(TokenType.THIS):
            entity = self.advance().value
        else:
            entity = self.consume(TokenType.IDENTIFIER, "Expected entity to bifurcate").value

        self.consume(TokenType.LBRACKET, "Expected '[' after entity")

        branch1 = self.consume(TokenType.IDENTIFIER, "Expected first branch name").value
        self.consume(TokenType.COMMA, "Expected ',' between branch names")
        branch2 = self.consume(TokenType.IDENTIFIER, "Expected second branch name").value

        self.consume(TokenType.RBRACKET, "Expected ']' after branch names")
        self.consume(TokenType.SEMICOLON, "Expected ';' after bifurcate statement")

        return BifurcateStmt(entity=entity, branch1=branch1, branch2=branch2, line=line, column=col)

    def parse_ath_loop(self):
        token = self.advance()  # consume '~ATH'
        line, col = token.line, token.column

        self.consume(TokenType.LPAREN, "Expected '(' after ~ATH")
        entity_expr = self.parse_entity_expr()
        self.consume(TokenType.RPAREN, "Expected ')' after entity expression")

        self.consume(TokenType.LBRACE, "Expected '{' for ~ATH body")

        # Parse body (can contain any statements in branch mode, only ~ATH in wait mode)
        # We'll do semantic checking later
        body = []
        while not self.check(TokenType.RBRACE):
            body.append(self.parse_statement())

        self.consume(TokenType.RBRACE, "Expected '}' after ~ATH body")
        self.consume(TokenType.EXECUTE, "Expected 'EXECUTE' after ~ATH body")
        self.consume(TokenType.LPAREN, "Expected '(' after EXECUTE")

        # Parse execute body
        execute = self.parse_execute_body()

        self.consume(TokenType.RPAREN, "Expected ')' after EXECUTE body")
        self.consume(TokenType.SEMICOLON, "Expected ';' after ~ATH loop")

        return AthLoop(entity_expr=entity_expr, body=body, execute=execute, line=line, column=col)

    def parse_execute_body(self) -> List:
        """Parse the body of an EXECUTE clause."""
        statements = []

        while not self.check(TokenType.RPAREN):
            # Try to parse a statement
            stmt = self.parse_execute_statement()
            if stmt is not None:
                statements.append(stmt)

            # Check if we have a trailing expression without semicolon
            if self.check(TokenType.RPAREN):
                break

        return statements

    def parse_execute_statement(self):
        """Parse a single statement inside EXECUTE."""
        token = self.current()

        if self.check(TokenType.IMPORT):
            return self.parse_import()
        if self.check(TokenType.TILDE_ATH):
            return self.parse_ath_loop()
        if self.check(TokenType.BIRTH):
            return self.parse_var_decl()
        if self.check(TokenType.ENTOMB):
            return self.parse_const_decl()
        if self.check(TokenType.RITE):
            return self.parse_rite_def()
        if self.check(TokenType.SHOULD):
            return self.parse_conditional()
        if self.check(TokenType.ATTEMPT):
            return self.parse_attempt_salvage()
        if self.check(TokenType.CONDEMN):
            return self.parse_condemn()
        if self.check(TokenType.BEQUEATH):
            return self.parse_bequeath()

        # Check for VOID literal as no-op
        if self.check(TokenType.VOID):
            void_token = self.advance()
            # Optional semicolon
            self.match(TokenType.SEMICOLON)
            return ExprStmt(
                expression=Literal(value=None, line=void_token.line, column=void_token.column),
                line=void_token.line,
                column=void_token.column
            )

        # Expression (possibly with assignment)
        expr = self.parse_expression()

        # Check for assignment
        if self.check(TokenType.ASSIGN):
            self.advance()
            value = self.parse_expression()
            self.consume(TokenType.SEMICOLON, "Expected ';' after assignment")
            return Assignment(target=expr, value=value, line=expr.line, column=expr.column)

        # Expression statement
        # Semicolon is optional for the last statement in EXECUTE
        self.match(TokenType.SEMICOLON)
        return ExprStmt(expression=expr, line=expr.line, column=expr.column)

    def parse_var_decl(self):
        token = self.advance()  # consume 'BIRTH'
        line, col = token.line, token.column

        name = self.consume(TokenType.IDENTIFIER, "Expected variable name").value
        self.consume(TokenType.WITH, "Expected 'WITH' after variable name")
        value = self.parse_expression()
        self.consume(TokenType.SEMICOLON, "Expected ';' after variable declaration")

        return VarDecl(name=name, value=value, line=line, column=col)

    def parse_const_decl(self):
        token = self.advance()  # consume 'ENTOMB'
        line, col = token.line, token.column

        name = self.consume(TokenType.IDENTIFIER, "Expected constant name").value
        self.consume(TokenType.WITH, "Expected 'WITH' after constant name")
        value = self.parse_expression()
        self.consume(TokenType.SEMICOLON, "Expected ';' after constant declaration")

        return ConstDecl(name=name, value=value, line=line, column=col)

    def parse_rite_def(self):
        token = self.advance()  # consume 'RITE'
        line, col = token.line, token.column

        name = self.consume(TokenType.IDENTIFIER, "Expected rite name").value
        self.consume(TokenType.LPAREN, "Expected '(' after rite name")

        # Parse parameters
        params = []
        if not self.check(TokenType.RPAREN):
            params.append(self.consume(TokenType.IDENTIFIER, "Expected parameter name").value)
            while self.match(TokenType.COMMA):
                params.append(self.consume(TokenType.IDENTIFIER, "Expected parameter name").value)

        self.consume(TokenType.RPAREN, "Expected ')' after parameters")
        self.consume(TokenType.LBRACE, "Expected '{' for rite body")

        # Parse body
        body = []
        while not self.check(TokenType.RBRACE):
            body.append(self.parse_execute_statement())

        self.consume(TokenType.RBRACE, "Expected '}' after rite body")

        return RiteDef(name=name, params=params, body=body, line=line, column=col)

    def parse_conditional(self):
        token = self.advance()  # consume 'SHOULD'
        line, col = token.line, token.column

        condition = self.parse_expression()
        self.consume(TokenType.LBRACE, "Expected '{' after condition")

        then_branch = []
        while not self.check(TokenType.RBRACE):
            then_branch.append(self.parse_execute_statement())

        self.consume(TokenType.RBRACE, "Expected '}' after then branch")

        else_branch = None
        if self.match(TokenType.LEST):
            if self.check(TokenType.SHOULD):
                # Chained conditional
                else_branch = [self.parse_conditional()]
            else:
                self.consume(TokenType.LBRACE, "Expected '{' after LEST")
                else_branch = []
                while not self.check(TokenType.RBRACE):
                    else_branch.append(self.parse_execute_statement())
                self.consume(TokenType.RBRACE, "Expected '}' after else branch")

        return Conditional(condition=condition, then_branch=then_branch,
                           else_branch=else_branch, line=line, column=col)

    def parse_attempt_salvage(self):
        token = self.advance()  # consume 'ATTEMPT'
        line, col = token.line, token.column

        self.consume(TokenType.LBRACE, "Expected '{' after ATTEMPT")

        attempt_body = []
        while not self.check(TokenType.RBRACE):
            attempt_body.append(self.parse_execute_statement())

        self.consume(TokenType.RBRACE, "Expected '}' after ATTEMPT body")
        self.consume(TokenType.SALVAGE, "Expected 'SALVAGE' after ATTEMPT block")

        error_name = self.consume(TokenType.IDENTIFIER, "Expected error variable name").value

        self.consume(TokenType.LBRACE, "Expected '{' after error variable")

        salvage_body = []
        while not self.check(TokenType.RBRACE):
            salvage_body.append(self.parse_execute_statement())

        self.consume(TokenType.RBRACE, "Expected '}' after SALVAGE body")

        return AttemptSalvage(attempt_body=attempt_body, error_name=error_name,
                              salvage_body=salvage_body, line=line, column=col)

    def parse_condemn(self):
        token = self.advance()  # consume 'CONDEMN'
        line, col = token.line, token.column

        message = self.parse_expression()
        self.consume(TokenType.SEMICOLON, "Expected ';' after CONDEMN")

        return CondemnStmt(message=message, line=line, column=col)

    def parse_bequeath(self):
        token = self.advance()  # consume 'BEQUEATH'
        line, col = token.line, token.column

        value = None
        if not self.check(TokenType.SEMICOLON):
            value = self.parse_expression()

        self.consume(TokenType.SEMICOLON, "Expected ';' after BEQUEATH")

        return BequeathStmt(value=value, line=line, column=col)

    def parse_die_or_assignment_or_expr(self):
        """Parse a statement starting with identifier or bracket (could be DIE, assignment, or expr)."""

        # Check for [targets].DIE() pattern
        if self.check(TokenType.LBRACKET):
            target = self.parse_die_target()
            self.consume(TokenType.DOT, "Expected '.' after die target")
            self.consume(TokenType.DIE, "Expected 'DIE' after '.'")
            self.consume(TokenType.LPAREN, "Expected '(' after DIE")
            self.consume(TokenType.RPAREN, "Expected ')' after DIE(")
            self.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement")
            return DieStmt(target=target, line=target.line, column=target.column)

        # Must be identifier - could be assignment, DIE, or expression
        expr = self.parse_expression()

        # Check for .DIE()
        if isinstance(expr, MemberExpr) and expr.member == 'DIE':
            raise self.error("DIE must be called as ENTITY.DIE(), not used as expression")

        # Check for assignment
        if self.check(TokenType.ASSIGN):
            self.advance()
            value = self.parse_expression()
            self.consume(TokenType.SEMICOLON, "Expected ';' after assignment")
            return Assignment(target=expr, value=value, line=expr.line, column=expr.column)

        # Check for DIE call
        if isinstance(expr, CallExpr):
            if isinstance(expr.callee, MemberExpr) and expr.callee.member == 'DIE':
                # Convert to DieStmt
                obj = expr.callee.obj
                if isinstance(obj, Identifier):
                    target = DieIdent(name=obj.name, line=obj.line, column=obj.column)
                else:
                    raise self.error("Invalid DIE target", token=self.current())
                self.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement")
                return DieStmt(target=target, line=expr.line, column=expr.column)

        # Expression statement
        self.consume(TokenType.SEMICOLON, "Expected ';' after expression")
        return ExprStmt(expression=expr, line=expr.line, column=expr.column)

    def parse_die_or_expr(self):
        """Parse a statement starting with THIS."""
        token = self.current()
        line, col = token.line, token.column

        expr = self.parse_expression()

        # Check for assignment (shouldn't happen with THIS but handle it)
        if self.check(TokenType.ASSIGN):
            self.advance()
            value = self.parse_expression()
            self.consume(TokenType.SEMICOLON, "Expected ';' after assignment")
            return Assignment(target=expr, value=value, line=line, column=col)

        # Check for DIE call
        if isinstance(expr, CallExpr):
            if isinstance(expr.callee, MemberExpr) and expr.callee.member == 'DIE':
                obj = expr.callee.obj
                if isinstance(obj, Identifier) and obj.name == 'THIS':
                    target = DieIdent(name='THIS', line=obj.line, column=obj.column)
                    self.consume(TokenType.SEMICOLON, "Expected ';' after DIE statement")
                    return DieStmt(target=target, line=line, column=col)

        # Expression statement
        self.consume(TokenType.SEMICOLON, "Expected ';' after expression")
        return ExprStmt(expression=expr, line=line, column=col)

    def parse_die_target(self):
        """Parse a DIE target (identifier or [pair])."""
        if self.check(TokenType.LBRACKET):
            token = self.advance()
            line, col = token.line, token.column
            left = self.parse_die_target()
            self.consume(TokenType.COMMA, "Expected ',' in die target pair")
            right = self.parse_die_target()
            self.consume(TokenType.RBRACKET, "Expected ']' after die target pair")
            return DiePair(left=left, right=right, line=line, column=col)
        else:
            if self.check(TokenType.THIS):
                token = self.advance()
                return DieIdent(name='THIS', line=token.line, column=token.column)
            token = self.consume(TokenType.IDENTIFIER, "Expected identifier in die target")
            return DieIdent(name=token.value, line=token.line, column=token.column)

    # ============ Entity Expressions ============

    def parse_entity_expr(self):
        """Parse entity expression (for ~ATH)."""
        return self.parse_entity_or()

    def parse_entity_or(self):
        left = self.parse_entity_and()

        while self.match(TokenType.PIPEPIPE):
            right = self.parse_entity_and()
            left = EntityOr(left=left, right=right, line=left.line, column=left.column)

        return left

    def parse_entity_and(self):
        left = self.parse_entity_unary()

        while self.match(TokenType.AMPAMP):
            right = self.parse_entity_unary()
            left = EntityAnd(left=left, right=right, line=left.line, column=left.column)

        return left

    def parse_entity_unary(self):
        if self.match(TokenType.BANG):
            token = self.tokens[self.pos - 1]
            operand = self.parse_entity_unary()
            return EntityNot(operand=operand, line=token.line, column=token.column)

        return self.parse_entity_primary()

    def parse_entity_primary(self):
        if self.match(TokenType.LPAREN):
            expr = self.parse_entity_expr()
            self.consume(TokenType.RPAREN, "Expected ')' after entity expression")
            return expr

        if self.check(TokenType.THIS):
            token = self.advance()
            return EntityIdent(name='THIS', line=token.line, column=token.column)

        token = self.consume(TokenType.IDENTIFIER, "Expected entity identifier")
        return EntityIdent(name=token.value, line=token.line, column=token.column)

    # ============ Expressions ============

    def parse_expression(self):
        return self.parse_or()

    def parse_or(self):
        left = self.parse_and()

        while self.match(TokenType.OR):
            token = self.tokens[self.pos - 1]
            right = self.parse_and()
            left = BinaryOp(operator='OR', left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_and(self):
        left = self.parse_equality()

        while self.match(TokenType.AND):
            token = self.tokens[self.pos - 1]
            right = self.parse_equality()
            left = BinaryOp(operator='AND', left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_equality(self):
        left = self.parse_comparison()

        while self.check(TokenType.EQ, TokenType.NE):
            token = self.advance()
            op = '==' if token.type == TokenType.EQ else '!='
            right = self.parse_comparison()
            left = BinaryOp(operator=op, left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_comparison(self):
        left = self.parse_bitwise_or()

        while self.check(TokenType.LT, TokenType.GT, TokenType.LE, TokenType.GE):
            token = self.advance()
            op_map = {
                TokenType.LT: '<',
                TokenType.GT: '>',
                TokenType.LE: '<=',
                TokenType.GE: '>='
            }
            right = self.parse_bitwise_or()
            left = BinaryOp(operator=op_map[token.type], left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_bitwise_or(self):
        left = self.parse_bitwise_xor()

        while self.match(TokenType.PIPE):
            token = self.tokens[self.pos - 1]
            right = self.parse_bitwise_xor()
            left = BinaryOp(operator='|', left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_bitwise_xor(self):
        left = self.parse_bitwise_and()

        while self.match(TokenType.CARET):
            token = self.tokens[self.pos - 1]
            right = self.parse_bitwise_and()
            left = BinaryOp(operator='^', left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_bitwise_and(self):
        left = self.parse_shift()

        while self.match(TokenType.AMP):
            token = self.tokens[self.pos - 1]
            right = self.parse_shift()
            left = BinaryOp(operator='&', left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_shift(self):
        left = self.parse_term()

        while self.check(TokenType.LSHIFT, TokenType.RSHIFT):
            token = self.advance()
            op = '<<' if token.type == TokenType.LSHIFT else '>>'
            right = self.parse_term()
            left = BinaryOp(operator=op, left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_term(self):
        left = self.parse_factor()

        while self.check(TokenType.PLUS, TokenType.MINUS):
            token = self.advance()
            op = '+' if token.type == TokenType.PLUS else '-'
            right = self.parse_factor()
            left = BinaryOp(operator=op, left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_factor(self):
        left = self.parse_unary()

        while self.check(TokenType.STAR, TokenType.SLASH, TokenType.PERCENT):
            token = self.advance()
            op_map = {
                TokenType.STAR: '*',
                TokenType.SLASH: '/',
                TokenType.PERCENT: '%'
            }
            right = self.parse_unary()
            left = BinaryOp(operator=op_map[token.type], left=left, right=right,
                            line=token.line, column=token.column)

        return left

    def parse_unary(self):
        if self.match(TokenType.NOT):
            token = self.tokens[self.pos - 1]
            operand = self.parse_unary()
            return UnaryOp(operator='NOT', operand=operand, line=token.line, column=token.column)

        if self.match(TokenType.MINUS):
            token = self.tokens[self.pos - 1]
            operand = self.parse_unary()
            return UnaryOp(operator='-', operand=operand, line=token.line, column=token.column)
            
        if self.match(TokenType.TILDE):
            token = self.tokens[self.pos - 1]
            operand = self.parse_unary()
            return UnaryOp(operator='~', operand=operand, line=token.line, column=token.column)

        return self.parse_postfix()

    def parse_postfix(self):
        expr = self.parse_primary()

        while True:
            if self.match(TokenType.LBRACKET):
                index = self.parse_expression()
                self.consume(TokenType.RBRACKET, "Expected ']' after index")
                expr = IndexExpr(obj=expr, index=index, line=expr.line, column=expr.column)
            elif self.match(TokenType.DOT):
                # Special handling for .DIE (DIE is a keyword, not identifier)
                if self.check(TokenType.DIE):
                    die_token = self.advance()
                    # Must be followed by ()
                    if self.check(TokenType.LPAREN):
                        self.advance()  # consume (
                        self.consume(TokenType.RPAREN, "Expected ')' after DIE(")
                        # Create a call expression for DIE
                        member_expr = MemberExpr(obj=expr, member='DIE',
                                                 line=die_token.line, column=die_token.column)
                        expr = CallExpr(callee=member_expr, args=[],
                                        line=die_token.line, column=die_token.column)
                    else:
                        raise self.error("Expected '(' after DIE")
                else:
                    member = self.consume(TokenType.IDENTIFIER, "Expected member name after '.'")
                    expr = MemberExpr(obj=expr, member=member.value,
                                      line=member.line, column=member.column)
            elif self.match(TokenType.LPAREN):
                # Function call
                args = []
                if not self.check(TokenType.RPAREN):
                    args.append(self.parse_expression())
                    while self.match(TokenType.COMMA):
                        args.append(self.parse_expression())
                self.consume(TokenType.RPAREN, "Expected ')' after arguments")
                expr = CallExpr(callee=expr, args=args, line=expr.line, column=expr.column)
            else:
                break

        return expr

    def parse_primary(self):
        token = self.current()

        if self.match(TokenType.INTEGER):
            return Literal(value=token.value, line=token.line, column=token.column)

        if self.match(TokenType.FLOAT):
            return Literal(value=token.value, line=token.line, column=token.column)

        if self.match(TokenType.STRING):
            return Literal(value=token.value, line=token.line, column=token.column)

        if self.match(TokenType.ALIVE):
            return Literal(value=True, line=token.line, column=token.column)

        if self.match(TokenType.DEAD):
            return Literal(value=False, line=token.line, column=token.column)

        if self.match(TokenType.VOID):
            return Literal(value=None, line=token.line, column=token.column)

        if self.match(TokenType.THIS):
            return Identifier(name='THIS', line=token.line, column=token.column)

        if self.match(TokenType.IDENTIFIER):
            return Identifier(name=token.value, line=token.line, column=token.column)

        if self.match(TokenType.LPAREN):
            expr = self.parse_expression()
            self.consume(TokenType.RPAREN, "Expected ')' after expression")
            return expr

        if self.match(TokenType.LBRACKET):
            return self.parse_array_literal(token)

        if self.match(TokenType.LBRACE):
            return self.parse_map_literal(token)

        raise self.error(f"Unexpected token in expression: {token.type.name}")

    def parse_array_literal(self, start_token):
        elements = []
        if not self.check(TokenType.RBRACKET):
            elements.append(self.parse_expression())
            while self.match(TokenType.COMMA):
                if self.check(TokenType.RBRACKET):
                    break  # Trailing comma
                elements.append(self.parse_expression())
        self.consume(TokenType.RBRACKET, "Expected ']' after array elements")
        return ArrayLiteral(elements=elements, line=start_token.line, column=start_token.column)

    def parse_map_literal(self, start_token):
        entries = []
        if not self.check(TokenType.RBRACE):
            # Parse first entry
            key = self.parse_map_key()
            self.consume(TokenType.COLON, "Expected ':' after map key")
            value = self.parse_expression()
            entries.append((key, value))

            while self.match(TokenType.COMMA):
                if self.check(TokenType.RBRACE):
                    break  # Trailing comma
                key = self.parse_map_key()
                self.consume(TokenType.COLON, "Expected ':' after map key")
                value = self.parse_expression()
                entries.append((key, value))

        self.consume(TokenType.RBRACE, "Expected '}' after map entries")
        return MapLiteral(entries=entries, line=start_token.line, column=start_token.column)

    def parse_map_key(self) -> str:
        if self.check(TokenType.STRING):
            return self.advance().value
        if self.check(TokenType.IDENTIFIER):
            return self.advance().value
        raise self.error("Expected map key (identifier or string)")
