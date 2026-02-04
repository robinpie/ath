"""Interpreter for the !~ATH language."""

import asyncio
from typing import Any, Dict, List, Optional, Set

from .ast_nodes import (
    Program, ImportStmt, BifurcateStmt, AthLoop, DieStmt,
    VarDecl, ConstDecl, Assignment, RiteDef, Conditional,
    AttemptSalvage, CondemnStmt, BequeathStmt, ExprStmt,
    EntityAnd, EntityOr, EntityNot, EntityIdent,
    DieIdent, DiePair,
    Literal, Identifier, BinaryOp, UnaryOp, CallExpr,
    IndexExpr, MemberExpr, ArrayLiteral, MapLiteral, Duration
)
from .entities import (
    Entity, ThisEntity, TimerEntity, ProcessEntity,
    ConnectionEntity, WatcherEntity, BranchEntity, CompositeEntity
)
from .builtins import Builtins, is_truthy, stringify
from .errors import RuntimeError, CondemnError, BequeathError


class Scope:
    """Variable scope."""

    def __init__(self, parent: Optional['Scope'] = None):
        self.parent = parent
        self.variables: Dict[str, Any] = {}
        self.constants: Set[str] = set()

    def define(self, name: str, value: Any, constant: bool = False):
        self.variables[name] = value
        if constant:
            self.constants.add(name)

    def get(self, name: str) -> Any:
        if name in self.variables:
            return self.variables[name]
        if self.parent:
            return self.parent.get(name)
        raise RuntimeError(f"Undefined variable: {name}")

    def set(self, name: str, value: Any):
        if name in self.variables:
            if name in self.constants:
                raise RuntimeError(f"Cannot reassign constant: {name}")
            self.variables[name] = value
            return
        if self.parent:
            self.parent.set(name, value)
            return
        raise RuntimeError(f"Undefined variable: {name}")

    def has(self, name: str) -> bool:
        if name in self.variables:
            return True
        if self.parent:
            return self.parent.has(name)
        return False


class UserRite:
    """User-defined rite (function)."""

    def __init__(self, name: str, params: List[str], body: List, closure: Scope):
        self.name = name
        self.params = params
        self.body = body
        self.closure = closure


class Interpreter:
    """!~ATH interpreter with async execution."""

    def __init__(self):
        self.global_scope = Scope()
        self.current_scope = self.global_scope
        self.entities: Dict[str, Entity] = {}
        self.branch_entities: Set[str] = set()  # Track which identifiers are branches
        self.builtins = Builtins(self)
        self.this_entity: Optional[ThisEntity] = None
        self._pending_tasks: List[asyncio.Task] = []

    async def run(self, program: Program):
        """Execute a program."""
        # Create THIS entity
        self.this_entity = ThisEntity()
        self.entities['THIS'] = self.this_entity

        try:
            # Execute all statements
            for stmt in program.statements:
                await self.execute(stmt)

            # Wait for all pending async operations
            if self._pending_tasks:
                await asyncio.gather(*self._pending_tasks, return_exceptions=True)

        except CondemnError as e:
            print(f"Uncaught error: {e.message}")
            raise

    async def execute(self, node):
        """Execute a statement or expression."""
        if isinstance(node, ImportStmt):
            return await self.exec_import(node)
        if isinstance(node, BifurcateStmt):
            return await self.exec_bifurcate(node)
        if isinstance(node, AthLoop):
            return await self.exec_ath_loop(node)
        if isinstance(node, DieStmt):
            return await self.exec_die(node)
        if isinstance(node, VarDecl):
            return await self.exec_var_decl(node)
        if isinstance(node, ConstDecl):
            return await self.exec_const_decl(node)
        if isinstance(node, Assignment):
            return await self.exec_assignment(node)
        if isinstance(node, RiteDef):
            return await self.exec_rite_def(node)
        if isinstance(node, Conditional):
            return await self.exec_conditional(node)
        if isinstance(node, AttemptSalvage):
            return await self.exec_attempt_salvage(node)
        if isinstance(node, CondemnStmt):
            return await self.exec_condemn(node)
        if isinstance(node, BequeathStmt):
            return await self.exec_bequeath(node)
        if isinstance(node, ExprStmt):
            return await self.evaluate(node.expression)

        raise RuntimeError(f"Unknown statement type: {type(node).__name__}")

    async def exec_import(self, node: ImportStmt):
        """Execute an import statement."""
        entity_type = node.entity_type
        name = node.name

        # If entity already exists, kill it and replace
        if name in self.entities:
            old_entity = self.entities[name]
            old_entity.die()

        if entity_type == 'timer':
            # Get duration
            duration = node.args[0]
            if isinstance(duration, Duration):
                ms = self._duration_to_ms(duration)
            else:
                raise RuntimeError("Timer requires a duration", node.line, node.column)

            entity = TimerEntity(name, ms)

        elif entity_type == 'process':
            # Get command and args
            args = [await self.evaluate(arg) for arg in node.args]
            if not args:
                raise RuntimeError("Process requires at least a command", node.line, node.column)
            command = str(args[0])
            proc_args = [str(a) for a in args[1:]]

            entity = ProcessEntity(name, command, proc_args)

        elif entity_type == 'connection':
            # Get host and port
            if len(node.args) != 2:
                raise RuntimeError("Connection requires host and port", node.line, node.column)
            host = await self.evaluate(node.args[0])
            port = await self.evaluate(node.args[1])
            if not isinstance(host, str):
                raise RuntimeError("Connection host must be a string", node.line, node.column)
            if not isinstance(port, int):
                raise RuntimeError("Connection port must be an integer", node.line, node.column)

            entity = ConnectionEntity(name, host, port)

        elif entity_type == 'watcher':
            # Get filepath
            if len(node.args) != 1:
                raise RuntimeError("Watcher requires a filepath", node.line, node.column)
            filepath = await self.evaluate(node.args[0])
            if not isinstance(filepath, str):
                raise RuntimeError("Watcher filepath must be a string", node.line, node.column)

            entity = WatcherEntity(name, filepath)

        else:
            raise RuntimeError(f"Unknown entity type: {entity_type}", node.line, node.column)

        self.entities[name] = entity

        # Start the entity's lifecycle
        task = asyncio.create_task(entity.start())
        self._pending_tasks.append(task)

    def _duration_to_ms(self, duration: Duration) -> int:
        """Convert a duration to milliseconds."""
        value = duration.value
        unit = duration.unit
        if unit == 'ms':
            ms = value
        elif unit == 's':
            ms = value * 1000
        elif unit == 'm':
            ms = value * 60 * 1000
        elif unit == 'h':
            ms = value * 60 * 60 * 1000
        else:
            ms = value  # Default to ms

        # Enforce minimum duration of 1ms
        if ms < 1:
            raise RuntimeError(f"Timer duration must be at least 1ms (got {ms}ms)", duration.line, duration.column)

        return ms

    async def exec_bifurcate(self, node: BifurcateStmt):
        """Execute a bifurcation statement."""
        entity_name = node.entity
        branch1_name = node.branch1
        branch2_name = node.branch2

        if entity_name not in self.entities:
            raise RuntimeError(f"Cannot bifurcate unknown entity: {entity_name}",
                               node.line, node.column)

        # Create branch entities
        branch1 = BranchEntity(branch1_name)
        branch2 = BranchEntity(branch2_name)

        self.entities[branch1_name] = branch1
        self.entities[branch2_name] = branch2
        self.branch_entities.add(branch1_name)
        self.branch_entities.add(branch2_name)

    async def exec_ath_loop(self, node: AthLoop):
        """Execute a ~ATH loop."""
        # Get the entity name(s) from the entity expression
        entity_expr = node.entity_expr

        # Check if this is branch mode
        if isinstance(entity_expr, EntityIdent) and entity_expr.name in self.branch_entities:
            # Branch mode - execute body as the branch code
            return await self.exec_branch_mode(node, entity_expr.name)

        # Wait mode - wait for entity to die, then execute
        entity = await self.resolve_entity_expr(entity_expr)

        # Wait for entity death
        await entity.wait_for_death()

        # Execute the EXECUTE clause in a new task to prevent stack overflow
        # when chaining timers recursively. Using create_task resets the C call
        # stack while preserving the same execution semantics.
        task = asyncio.create_task(self.exec_statements(node.execute))
        await task

    async def exec_branch_mode(self, node: AthLoop, branch_name: str):
        """Execute branch mode ~ATH."""
        branch_entity = self.entities[branch_name]
        if not isinstance(branch_entity, BranchEntity):
            raise RuntimeError(f"{branch_name} is not a branch entity", node.line, node.column)

        async def run_branch():
            try:
                # Execute body
                for stmt in node.body:
                    await self.execute(stmt)

                # Execute EXECUTE clause
                await self.exec_statements(node.execute)

                # Mark branch as complete
                branch_entity.complete()
            except Exception as e:
                branch_entity.complete()
                raise

        # Schedule branch to run
        task = asyncio.create_task(run_branch())
        self._pending_tasks.append(task)

        # Give other branches a chance to start
        await asyncio.sleep(0)

    async def resolve_entity_expr(self, expr) -> Entity:
        """Resolve an entity expression to an Entity object."""
        if isinstance(expr, EntityIdent):
            name = expr.name
            if name not in self.entities:
                raise RuntimeError(f"Unknown entity: {name}", expr.line, expr.column)
            return self.entities[name]

        if isinstance(expr, EntityAnd):
            left = await self.resolve_entity_expr(expr.left)
            right = await self.resolve_entity_expr(expr.right)
            composite = CompositeEntity(f"({left.name} && {right.name})", 'AND', [left, right])
            task = asyncio.create_task(composite.start())
            self._pending_tasks.append(task)
            return composite

        if isinstance(expr, EntityOr):
            left = await self.resolve_entity_expr(expr.left)
            right = await self.resolve_entity_expr(expr.right)
            composite = CompositeEntity(f"({left.name} || {right.name})", 'OR', [left, right])
            task = asyncio.create_task(composite.start())
            self._pending_tasks.append(task)
            return composite

        if isinstance(expr, EntityNot):
            inner = await self.resolve_entity_expr(expr.operand)
            composite = CompositeEntity(f"(!{inner.name})", 'NOT', [inner])
            task = asyncio.create_task(composite.start())
            self._pending_tasks.append(task)
            return composite

        raise RuntimeError(f"Unknown entity expression type", expr.line, expr.column)

    async def exec_die(self, node: DieStmt):
        """Execute a DIE statement."""
        await self._kill_target(node.target)

    async def _kill_target(self, target):
        """Recursively kill a die target."""
        if isinstance(target, DieIdent):
            name = target.name
            if name not in self.entities:
                raise RuntimeError(f"Unknown entity: {name}", target.line, target.column)
            self.entities[name].die()
        elif isinstance(target, DiePair):
            await self._kill_target(target.left)
            await self._kill_target(target.right)

    async def exec_var_decl(self, node: VarDecl):
        """Execute a variable declaration."""
        value = await self.evaluate(node.value)
        self.current_scope.define(node.name, value, constant=False)

    async def exec_const_decl(self, node: ConstDecl):
        """Execute a constant declaration."""
        value = await self.evaluate(node.value)
        self.current_scope.define(node.name, value, constant=True)

    async def exec_assignment(self, node: Assignment):
        """Execute an assignment."""
        value = await self.evaluate(node.value)
        target = node.target

        if isinstance(target, Identifier):
            self.current_scope.set(target.name, value)
        elif isinstance(target, IndexExpr):
            obj = await self.evaluate(target.obj)
            index = await self.evaluate(target.index)
            if isinstance(obj, list):
                if not isinstance(index, int):
                    raise RuntimeError("Array index must be an integer", node.line, node.column)
                if index < 0 or index >= len(obj):
                    raise RuntimeError(f"Array index out of bounds: {index}", node.line, node.column)
                obj[index] = value
            elif isinstance(obj, dict):
                obj[str(index)] = value
            else:
                raise RuntimeError("Cannot index non-collection", node.line, node.column)
        elif isinstance(target, MemberExpr):
            obj = await self.evaluate(target.obj)
            if isinstance(obj, dict):
                obj[target.member] = value
            else:
                raise RuntimeError("Cannot access member of non-map", node.line, node.column)
        else:
            raise RuntimeError("Invalid assignment target", node.line, node.column)

    async def exec_rite_def(self, node: RiteDef):
        """Execute a rite definition."""
        rite = UserRite(node.name, node.params, node.body, self.current_scope)
        self.current_scope.define(node.name, rite, constant=True)

    async def exec_conditional(self, node: Conditional):
        """Execute a conditional statement."""
        condition = await self.evaluate(node.condition)

        if is_truthy(condition):
            await self.exec_statements(node.then_branch)
        elif node.else_branch:
            await self.exec_statements(node.else_branch)

    async def exec_attempt_salvage(self, node: AttemptSalvage):
        """Execute an attempt-salvage block."""
        try:
            await self.exec_statements(node.attempt_body)
        except (RuntimeError, CondemnError) as e:
            # Create new scope for salvage block with error variable
            old_scope = self.current_scope
            self.current_scope = Scope(old_scope)
            self.current_scope.define(node.error_name, str(e.message if hasattr(e, 'message') else e))
            try:
                await self.exec_statements(node.salvage_body)
            finally:
                self.current_scope = old_scope

    async def exec_condemn(self, node: CondemnStmt):
        """Execute a CONDEMN statement."""
        message = await self.evaluate(node.message)
        raise CondemnError(stringify(message), node.line, node.column)

    async def exec_bequeath(self, node: BequeathStmt):
        """Execute a BEQUEATH statement."""
        value = None
        if node.value:
            value = await self.evaluate(node.value)
        raise BequeathError(value)

    async def exec_statements(self, statements: List):
        """Execute a list of statements."""
        for stmt in statements:
            await self.execute(stmt)

    # ============ Expression Evaluation ============

    async def evaluate(self, node) -> Any:
        """Evaluate an expression."""
        if isinstance(node, Literal):
            return node.value

        if isinstance(node, Identifier):
            name = node.name
            # Check for THIS
            if name == 'THIS':
                return self.this_entity
            # Check for built-in rite
            builtin = self.builtins.get(name)
            if builtin:
                return builtin
            # Check scope
            return self.current_scope.get(name)

        if isinstance(node, BinaryOp):
            return await self.eval_binary_op(node)

        if isinstance(node, UnaryOp):
            return await self.eval_unary_op(node)

        if isinstance(node, CallExpr):
            return await self.eval_call(node)

        if isinstance(node, IndexExpr):
            return await self.eval_index(node)

        if isinstance(node, MemberExpr):
            return await self.eval_member(node)

        if isinstance(node, ArrayLiteral):
            return [await self.evaluate(e) for e in node.elements]

        if isinstance(node, MapLiteral):
            result = {}
            for key, value in node.entries:
                result[key] = await self.evaluate(value)
            return result

        raise RuntimeError(f"Unknown expression type: {type(node).__name__}")

    async def eval_binary_op(self, node: BinaryOp) -> Any:
        """Evaluate a binary operation."""
        op = node.operator

        # Short-circuit operators
        if op == 'AND':
            left = await self.evaluate(node.left)
            if not is_truthy(left):
                return left
            return await self.evaluate(node.right)

        if op == 'OR':
            left = await self.evaluate(node.left)
            if is_truthy(left):
                return left
            return await self.evaluate(node.right)

        # Evaluate both operands
        left = await self.evaluate(node.left)
        right = await self.evaluate(node.right)

        if op == '+':
            if isinstance(left, str) or isinstance(right, str):
                return stringify(left) + stringify(right)
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                return left + right
            raise RuntimeError(f"Cannot add {stringify(left)} and {stringify(right)}",
                               node.line, node.column)

        if op == '-':
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                return left - right
            raise RuntimeError(f"Cannot subtract {stringify(right)} from {stringify(left)}",
                               node.line, node.column)

        if op == '*':
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                return left * right
            raise RuntimeError(f"Cannot multiply {stringify(left)} by {stringify(right)}",
                               node.line, node.column)

        if op == '/':
            if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                if right == 0:
                    raise RuntimeError("Division by zero", node.line, node.column)
                if isinstance(left, int) and isinstance(right, int):
                    return left // right  # Integer division
                return left / right
            raise RuntimeError(f"Cannot divide {stringify(left)} by {stringify(right)}",
                               node.line, node.column)

        if op == '%':
            if isinstance(left, int) and isinstance(right, int):
                if right == 0:
                    raise RuntimeError("Modulo by zero", node.line, node.column)
                return left % right
            raise RuntimeError(f"Cannot modulo {stringify(left)} by {stringify(right)}",
                               node.line, node.column)

        if op == '==':
            return left == right

        if op == '!=':
            return left != right

        if op == '<':
            return left < right

        if op == '>':
            return left > right

        if op == '<=':
            return left <= right

        if op == '>=':
            return left >= right

        raise RuntimeError(f"Unknown operator: {op}", node.line, node.column)

    async def eval_unary_op(self, node: UnaryOp) -> Any:
        """Evaluate a unary operation."""
        operand = await self.evaluate(node.operand)

        if node.operator == 'NOT':
            return not is_truthy(operand)

        if node.operator == '-':
            if isinstance(operand, (int, float)):
                return -operand
            raise RuntimeError(f"Cannot negate {stringify(operand)}", node.line, node.column)

        raise RuntimeError(f"Unknown unary operator: {node.operator}", node.line, node.column)

    async def eval_call(self, node: CallExpr) -> Any:
        """Evaluate a function call."""
        callee = await self.evaluate(node.callee)
        args = [await self.evaluate(arg) for arg in node.args]

        # Built-in function
        if callable(callee) and not isinstance(callee, UserRite):
            try:
                return callee(*args)
            except RuntimeError:
                raise
            except Exception as e:
                raise RuntimeError(str(e), node.line, node.column)

        # User-defined rite
        if isinstance(callee, UserRite):
            return await self.call_rite(callee, args, node)

        raise RuntimeError(f"Cannot call {stringify(callee)}", node.line, node.column)

    async def call_rite(self, rite: UserRite, args: List[Any], node) -> Any:
        """Call a user-defined rite."""
        if len(args) != len(rite.params):
            raise RuntimeError(
                f"Rite '{rite.name}' expects {len(rite.params)} arguments, got {len(args)}",
                node.line, node.column
            )

        # Create new scope
        old_scope = self.current_scope
        self.current_scope = Scope(rite.closure)

        # Bind parameters
        for param, arg in zip(rite.params, args):
            self.current_scope.define(param, arg)

        try:
            # Execute body
            for stmt in rite.body:
                await self.execute(stmt)
            return None  # No BEQUEATH reached
        except BequeathError as e:
            return e.value
        finally:
            self.current_scope = old_scope

    async def eval_index(self, node: IndexExpr) -> Any:
        """Evaluate an index expression."""
        obj = await self.evaluate(node.obj)
        index = await self.evaluate(node.index)

        if isinstance(obj, list):
            if not isinstance(index, int):
                raise RuntimeError("Array index must be an integer", node.line, node.column)
            if index < 0 or index >= len(obj):
                raise RuntimeError(f"Array index out of bounds: {index}", node.line, node.column)
            return obj[index]

        if isinstance(obj, dict):
            key = str(index)
            if key not in obj:
                raise RuntimeError(f"Key not found in map: {key}", node.line, node.column)
            return obj[key]

        if isinstance(obj, str):
            if not isinstance(index, int):
                raise RuntimeError("String index must be an integer", node.line, node.column)
            if index < 0 or index >= len(obj):
                raise RuntimeError(f"String index out of bounds: {index}", node.line, node.column)
            return obj[index]

        raise RuntimeError(f"Cannot index {stringify(obj)}", node.line, node.column)

    async def eval_member(self, node: MemberExpr) -> Any:
        """Evaluate a member expression."""
        obj = await self.evaluate(node.obj)

        if isinstance(obj, dict):
            if node.member not in obj:
                raise RuntimeError(f"Key not found in map: {node.member}", node.line, node.column)
            return obj[node.member]

        raise RuntimeError(f"Cannot access member of {stringify(obj)}", node.line, node.column)
