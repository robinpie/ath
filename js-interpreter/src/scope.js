/**
 * Scope and UserRite classes for !~ATH.
 */

import { RuntimeError } from './errors.js';

/**
 * Variable scope with parent chain for closures.
 */
export class Scope {
  constructor(parent = null) {
    this.parent = parent;
    this.variables = new Map();
    this.constants = new Set();
  }

  define(name, value, constant = false) {
    this.variables.set(name, value);
    if (constant) {
      this.constants.add(name);
    }
  }

  get(name) {
    if (this.variables.has(name)) {
      return this.variables.get(name);
    }
    if (this.parent) {
      return this.parent.get(name);
    }
    throw new RuntimeError(`Undefined variable: ${name}`);
  }

  set(name, value) {
    if (this.variables.has(name)) {
      if (this.constants.has(name)) {
        throw new RuntimeError(`Cannot reassign constant: ${name}`);
      }
      this.variables.set(name, value);
      return;
    }
    if (this.parent) {
      this.parent.set(name, value);
      return;
    }
    throw new RuntimeError(`Undefined variable: ${name}`);
  }

  has(name) {
    if (this.variables.has(name)) {
      return true;
    }
    if (this.parent) {
      return this.parent.has(name);
    }
    return false;
  }
}

/**
 * User-defined rite (function).
 */
export class UserRite {
  constructor(name, params, body, closure) {
    this.name = name;
    this.params = params;
    this.body = body;
    this.closure = closure;
  }
}
