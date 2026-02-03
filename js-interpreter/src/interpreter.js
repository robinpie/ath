/**
 * Interpreter for the !~ATH language.
 */

import { RuntimeError, CondemnError, BequeathError } from './errors.js';
import { Entity, ThisEntity, TimerEntity, BranchEntity, CompositeEntity } from './entities.js';
import { Scope, UserRite } from './scope.js';
import { Builtins, isTruthy, stringify } from './builtins.js';

export class Interpreter {
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
    // Create THIS entity
    this.thisEntity = new ThisEntity();
    this.entities.set('THIS', this.thisEntity);

    try {
      // Execute all statements
      for (const stmt of program.statements) {
        await this.execute(stmt);
      }

      // Wait for all pending async operations
      if (this._pendingPromises.length > 0) {
        await Promise.allSettled(this._pendingPromises);
      }
    } catch (e) {
      if (e instanceof CondemnError) {
        throw e;
      }
      throw e;
    } finally {
      // Clean up all entities (kill timers, etc.)
      for (const entity of this.entities.values()) {
        entity.die();
      }
    }
  }

  async execute(node) {
    switch (node.type) {
      case 'ImportStmt':
        return this.execImport(node);
      case 'BifurcateStmt':
        return this.execBifurcate(node);
      case 'AthLoop':
        return this.execAthLoop(node);
      case 'DieStmt':
        return this.execDie(node);
      case 'VarDecl':
        return this.execVarDecl(node);
      case 'ConstDecl':
        return this.execConstDecl(node);
      case 'Assignment':
        return this.execAssignment(node);
      case 'RiteDef':
        return this.execRiteDef(node);
      case 'Conditional':
        return this.execConditional(node);
      case 'AttemptSalvage':
        return this.execAttemptSalvage(node);
      case 'CondemnStmt':
        return this.execCondemn(node);
      case 'BequeathStmt':
        return this.execBequeath(node);
      case 'ExprStmt':
        return this.evaluate(node.expression);
      default:
        throw new RuntimeError(`Unknown statement type: ${node.type}`);
    }
  }

  async execImport(node) {
    const entityType = node.entityType;
    const name = node.name;

    // If entity already exists, kill it and replace
    if (this.entities.has(name)) {
      const oldEntity = this.entities.get(name);
      oldEntity.die();
    }

    let entity;

    if (entityType === 'timer') {
      const duration = node.args[0];
      if (duration.type !== 'Duration') {
        throw new RuntimeError('Timer requires a duration', node.line, node.column);
      }
      const ms = this._durationToMs(duration);
      entity = new TimerEntity(name, ms);
    } else if (entityType === 'process') {
      throw new RuntimeError('ProcessEntity is not supported in browser environment', node.line, node.column);
    } else if (entityType === 'connection') {
      throw new RuntimeError('ConnectionEntity is not supported in browser environment', node.line, node.column);
    } else if (entityType === 'watcher') {
      throw new RuntimeError('WatcherEntity is not supported in browser environment', node.line, node.column);
    } else {
      throw new RuntimeError(`Unknown entity type: ${entityType}`, node.line, node.column);
    }

    this.entities.set(name, entity);

    // Start the entity's lifecycle
    const promise = entity.start();
    this._pendingPromises.push(promise);
  }

  _durationToMs(duration) {
    const value = duration.value;
    const unit = duration.unit;
    let ms;
    switch (unit) {
      case 'ms':
        ms = value;
        break;
      case 's':
        ms = value * 1000;
        break;
      case 'm':
        ms = value * 60 * 1000;
        break;
      case 'h':
        ms = value * 60 * 60 * 1000;
        break;
      default:
        ms = value;  // Default to ms
    }

    // Enforce minimum duration of 1ms
    if (ms < 1) {
      throw new RuntimeError(`Timer duration must be at least 1ms (got ${ms}ms)`, duration.line, duration.column);
    }

    return ms;
  }

  async execBifurcate(node) {
    const entityName = node.entity;
    const branch1Name = node.branch1;
    const branch2Name = node.branch2;

    if (!this.entities.has(entityName)) {
      throw new RuntimeError(`Cannot bifurcate unknown entity: ${entityName}`, node.line, node.column);
    }

    // Create branch entities
    const branch1 = new BranchEntity(branch1Name);
    const branch2 = new BranchEntity(branch2Name);

    this.entities.set(branch1Name, branch1);
    this.entities.set(branch2Name, branch2);
    this.branchEntities.add(branch1Name);
    this.branchEntities.add(branch2Name);
  }

  async execAthLoop(node) {
    const entityExpr = node.entityExpr;

    // Check if this is branch mode
    if (entityExpr.type === 'EntityIdent' && this.branchEntities.has(entityExpr.name)) {
      return this.execBranchMode(node, entityExpr.name);
    }

    // Wait mode - wait for entity to die, then execute
    const entity = await this.resolveEntityExpr(entityExpr);

    // Wait for entity death
    await entity.waitForDeath();

    // Execute the EXECUTE clause
    await this.execStatements(node.execute);
  }

  async execBranchMode(node, branchName) {
    const branchEntity = this.entities.get(branchName);
    if (!(branchEntity instanceof BranchEntity)) {
      throw new RuntimeError(`${branchName} is not a branch entity`, node.line, node.column);
    }

    const runBranch = async () => {
      try {
        // Execute body
        for (const stmt of node.body) {
          await this.execute(stmt);
        }

        // Execute EXECUTE clause
        await this.execStatements(node.execute);

        // Mark branch as complete
        branchEntity.complete();
      } catch (e) {
        branchEntity.complete();
        throw e;
      }
    };

    // Schedule branch to run
    const promise = runBranch();
    this._pendingPromises.push(promise);

    // Give other branches a chance to start
    await new Promise(resolve => setTimeout(resolve, 0));
  }

  async resolveEntityExpr(expr) {
    switch (expr.type) {
      case 'EntityIdent': {
        const name = expr.name;
        if (!this.entities.has(name)) {
          throw new RuntimeError(`Unknown entity: ${name}`, expr.line, expr.column);
        }
        return this.entities.get(name);
      }
      case 'EntityAnd': {
        const left = await this.resolveEntityExpr(expr.left);
        const right = await this.resolveEntityExpr(expr.right);
        const composite = new CompositeEntity(`(${left.name} && ${right.name})`, 'AND', [left, right]);
        const promise = composite.start();
        this._pendingPromises.push(promise);
        return composite;
      }
      case 'EntityOr': {
        const left = await this.resolveEntityExpr(expr.left);
        const right = await this.resolveEntityExpr(expr.right);
        const composite = new CompositeEntity(`(${left.name} || ${right.name})`, 'OR', [left, right]);
        const promise = composite.start();
        this._pendingPromises.push(promise);
        return composite;
      }
      case 'EntityNot': {
        const inner = await this.resolveEntityExpr(expr.operand);
        const composite = new CompositeEntity(`(!${inner.name})`, 'NOT', [inner]);
        const promise = composite.start();
        this._pendingPromises.push(promise);
        return composite;
      }
      default:
        throw new RuntimeError('Unknown entity expression type', expr.line, expr.column);
    }
  }

  async execDie(node) {
    await this._killTarget(node.target);
  }

  async _killTarget(target) {
    switch (target.type) {
      case 'DieIdent': {
        const name = target.name;
        if (!this.entities.has(name)) {
          throw new RuntimeError(`Unknown entity: ${name}`, target.line, target.column);
        }
        this.entities.get(name).die();
        break;
      }
      case 'DiePair': {
        await this._killTarget(target.left);
        await this._killTarget(target.right);
        break;
      }
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

    switch (target.type) {
      case 'Identifier':
        this.currentScope.set(target.name, value);
        break;
      case 'IndexExpr': {
        const obj = await this.evaluate(target.obj);
        const index = await this.evaluate(target.index);
        if (Array.isArray(obj)) {
          if (typeof index !== 'number' || !Number.isInteger(index)) {
            throw new RuntimeError('Array index must be an integer', node.line, node.column);
          }
          if (index < 0 || index >= obj.length) {
            throw new RuntimeError(`Array index out of bounds: ${index}`, node.line, node.column);
          }
          obj[index] = value;
        } else if (typeof obj === 'object' && obj !== null) {
          obj[String(index)] = value;
        } else {
          throw new RuntimeError('Cannot index non-collection', node.line, node.column);
        }
        break;
      }
      case 'MemberExpr': {
        const obj = await this.evaluate(target.obj);
        if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
          obj[target.member] = value;
        } else {
          throw new RuntimeError('Cannot access member of non-map', node.line, node.column);
        }
        break;
      }
      default:
        throw new RuntimeError('Invalid assignment target', node.line, node.column);
    }
  }

  async execRiteDef(node) {
    const rite = new UserRite(node.name, node.params, node.body, this.currentScope);
    this.currentScope.define(node.name, rite, true);
  }

  async execConditional(node) {
    const condition = await this.evaluate(node.condition);

    if (isTruthy(condition)) {
      await this.execStatements(node.thenBranch);
    } else if (node.elseBranch) {
      await this.execStatements(node.elseBranch);
    }
  }

  async execAttemptSalvage(node) {
    try {
      await this.execStatements(node.attemptBody);
    } catch (e) {
      if (e instanceof RuntimeError || e instanceof CondemnError) {
        // Create new scope for salvage block with error variable
        const oldScope = this.currentScope;
        this.currentScope = new Scope(oldScope);
        this.currentScope.define(node.errorName, e.tildeAthMessage || String(e));
        try {
          await this.execStatements(node.salvageBody);
        } finally {
          this.currentScope = oldScope;
        }
      } else {
        throw e;
      }
    }
  }

  async execCondemn(node) {
    const message = await this.evaluate(node.message);
    throw new CondemnError(stringify(message), node.line, node.column);
  }

  async execBequeath(node) {
    let value = null;
    if (node.value) {
      value = await this.evaluate(node.value);
    }
    throw new BequeathError(value);
  }

  async execStatements(statements) {
    for (const stmt of statements) {
      await this.execute(stmt);
    }
  }

  // ============ Expression Evaluation ============

  async evaluate(node) {
    switch (node.type) {
      case 'Literal':
        return node.value;

      case 'Identifier': {
        const name = node.name;
        // Check for THIS
        if (name === 'THIS') {
          return this.thisEntity;
        }
        // Check for built-in rite
        const builtin = this.builtins.get(name);
        if (builtin) {
          return builtin;
        }
        // Check scope
        return this.currentScope.get(name);
      }

      case 'BinaryOp':
        return this.evalBinaryOp(node);

      case 'UnaryOp':
        return this.evalUnaryOp(node);

      case 'CallExpr':
        return this.evalCall(node);

      case 'IndexExpr':
        return this.evalIndex(node);

      case 'MemberExpr':
        return this.evalMember(node);

      case 'ArrayLiteral': {
        const elements = [];
        for (const e of node.elements) {
          elements.push(await this.evaluate(e));
        }
        return elements;
      }

      case 'MapLiteral': {
        const result = {};
        for (const [key, value] of node.entries) {
          result[key] = await this.evaluate(value);
        }
        return result;
      }

      default:
        throw new RuntimeError(`Unknown expression type: ${node.type}`);
    }
  }

  async evalBinaryOp(node) {
    const op = node.operator;

    // Short-circuit operators
    if (op === 'AND') {
      const left = await this.evaluate(node.left);
      if (!isTruthy(left)) {
        return left;
      }
      return this.evaluate(node.right);
    }

    if (op === 'OR') {
      const left = await this.evaluate(node.left);
      if (isTruthy(left)) {
        return left;
      }
      return this.evaluate(node.right);
    }

    // Evaluate both operands
    const left = await this.evaluate(node.left);
    const right = await this.evaluate(node.right);

    switch (op) {
      case '+':
        if (typeof left === 'string' || typeof right === 'string') {
          return stringify(left) + stringify(right);
        }
        if (typeof left === 'number' && typeof right === 'number') {
          return left + right;
        }
        throw new RuntimeError(`Cannot add ${stringify(left)} and ${stringify(right)}`, node.line, node.column);

      case '-':
        if (typeof left === 'number' && typeof right === 'number') {
          return left - right;
        }
        throw new RuntimeError(`Cannot subtract ${stringify(right)} from ${stringify(left)}`, node.line, node.column);

      case '*':
        if (typeof left === 'number' && typeof right === 'number') {
          return left * right;
        }
        throw new RuntimeError(`Cannot multiply ${stringify(left)} by ${stringify(right)}`, node.line, node.column);

      case '/':
        if (typeof left === 'number' && typeof right === 'number') {
          if (right === 0) {
            throw new RuntimeError('Division by zero', node.line, node.column);
          }
          if (Number.isInteger(left) && Number.isInteger(right)) {
            return Math.trunc(left / right);  // Integer division
          }
          return left / right;
        }
        throw new RuntimeError(`Cannot divide ${stringify(left)} by ${stringify(right)}`, node.line, node.column);

      case '%':
        if (Number.isInteger(left) && Number.isInteger(right)) {
          if (right === 0) {
            throw new RuntimeError('Modulo by zero', node.line, node.column);
          }
          return left % right;
        }
        throw new RuntimeError(`Cannot modulo ${stringify(left)} by ${stringify(right)}`, node.line, node.column);

      case '==':
        return left === right;

      case '!=':
        return left !== right;

      case '<':
        return left < right;

      case '>':
        return left > right;

      case '<=':
        return left <= right;

      case '>=':
        return left >= right;

      default:
        throw new RuntimeError(`Unknown operator: ${op}`, node.line, node.column);
    }
  }

  async evalUnaryOp(node) {
    const operand = await this.evaluate(node.operand);

    if (node.operator === 'NOT') {
      return !isTruthy(operand);
    }

    if (node.operator === '-') {
      if (typeof operand === 'number') {
        return -operand;
      }
      throw new RuntimeError(`Cannot negate ${stringify(operand)}`, node.line, node.column);
    }

    throw new RuntimeError(`Unknown unary operator: ${node.operator}`, node.line, node.column);
  }

  async evalCall(node) {
    const callee = await this.evaluate(node.callee);
    const args = [];
    for (const arg of node.args) {
      args.push(await this.evaluate(arg));
    }

    // Built-in function
    if (typeof callee === 'function') {
      try {
        return callee(...args);
      } catch (e) {
        if (e instanceof RuntimeError) {
          throw e;
        }
        throw new RuntimeError(String(e), node.line, node.column);
      }
    }

    // User-defined rite
    if (callee instanceof UserRite) {
      return this.callRite(callee, args, node);
    }

    throw new RuntimeError(`Cannot call ${stringify(callee)}`, node.line, node.column);
  }

  async callRite(rite, args, node) {
    if (args.length !== rite.params.length) {
      throw new RuntimeError(
        `Rite '${rite.name}' expects ${rite.params.length} arguments, got ${args.length}`,
        node.line,
        node.column
      );
    }

    // Create new scope
    const oldScope = this.currentScope;
    this.currentScope = new Scope(rite.closure);

    // Bind parameters
    for (let i = 0; i < rite.params.length; i++) {
      this.currentScope.define(rite.params[i], args[i]);
    }

    try {
      // Execute body
      for (const stmt of rite.body) {
        await this.execute(stmt);
      }
      return null;  // No BEQUEATH reached
    } catch (e) {
      if (e instanceof BequeathError) {
        return e.value;
      }
      throw e;
    } finally {
      this.currentScope = oldScope;
    }
  }

  async evalIndex(node) {
    const obj = await this.evaluate(node.obj);
    const index = await this.evaluate(node.index);

    if (Array.isArray(obj)) {
      if (typeof index !== 'number' || !Number.isInteger(index)) {
        throw new RuntimeError('Array index must be an integer', node.line, node.column);
      }
      if (index < 0 || index >= obj.length) {
        throw new RuntimeError(`Array index out of bounds: ${index}`, node.line, node.column);
      }
      return obj[index];
    }

    if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
      const key = String(index);
      if (!(key in obj)) {
        throw new RuntimeError(`Key not found in map: ${key}`, node.line, node.column);
      }
      return obj[key];
    }

    if (typeof obj === 'string') {
      if (typeof index !== 'number' || !Number.isInteger(index)) {
        throw new RuntimeError('String index must be an integer', node.line, node.column);
      }
      if (index < 0 || index >= obj.length) {
        throw new RuntimeError(`String index out of bounds: ${index}`, node.line, node.column);
      }
      return obj[index];
    }

    throw new RuntimeError(`Cannot index ${stringify(obj)}`, node.line, node.column);
  }

  async evalMember(node) {
    const obj = await this.evaluate(node.obj);

    if (typeof obj === 'object' && obj !== null && !Array.isArray(obj)) {
      if (!(node.member in obj)) {
        throw new RuntimeError(`Key not found in map: ${node.member}`, node.line, node.column);
      }
      return obj[node.member];
    }

    throw new RuntimeError(`Cannot access member of ${stringify(obj)}`, node.line, node.column);
  }
}
