/**
 * Built-in rites (functions) for !~ATH.
 */

import { RuntimeError } from './errors.js';

/**
 * Convert a value to its string representation.
 */
export function stringify(value) {
  if (value === null || value === undefined) {
    return 'VOID';
  }
  if (typeof value === 'boolean') {
    return value ? 'ALIVE' : 'DEAD';
  }
  if (Array.isArray(value)) {
    return '[' + value.map(stringify).join(', ') + ']';
  }
  if (typeof value === 'object') {
    const entries = Object.entries(value)
      .map(([k, v]) => `${k}: ${stringify(v)}`)
      .join(', ');
    return '{' + entries + '}';
  }
  return String(value);
}

/**
 * Get the type name of a value.
 */
export function typeName(value) {
  if (value === null || value === undefined) {
    return 'VOID';
  }
  if (typeof value === 'boolean') {
    return 'BOOLEAN';
  }
  if (typeof value === 'number') {
    return Number.isInteger(value) ? 'INTEGER' : 'FLOAT';
  }
  if (typeof value === 'string') {
    return 'STRING';
  }
  if (Array.isArray(value)) {
    return 'ARRAY';
  }
  if (typeof value === 'object') {
    return 'MAP';
  }
  return 'UNKNOWN';
}

/**
 * Determine if a value is truthy.
 */
export function isTruthy(value) {
  if (value === null || value === undefined) {
    return false;
  }
  if (typeof value === 'boolean') {
    return value;
  }
  if (typeof value === 'number') {
    return value !== 0;
  }
  if (typeof value === 'string') {
    return value.length > 0;
  }
  if (Array.isArray(value)) {
    return value.length > 0;
  }
  if (typeof value === 'object') {
    return Object.keys(value).length > 0;
  }
  return true;
}

/**
 * Container for all built-in rites.
 */
export class Builtins {
  constructor(interpreter, options = {}) {
    this.interpreter = interpreter;
    this.onOutput = options.onOutput || ((text) => console.log(text));
    this.onInput = options.onInput || null;
    this.inputQueue = options.inputQueue || [];
    this._inputIndex = 0;
    this.scryQueue = options.scryQueue || [];
    this._scryIndex = 0;
  }

  get(name) {
    const builtins = {
      // I/O
      'UTTER': (...args) => this.utter(...args),
      'HEED': () => this.heed(),
      'SCRY': (path) => this.scry(path),

      // Type operations
      'TYPEOF': (value) => this.typeof_(value),
      'LENGTH': (value) => this.length(value),
      'PARSE_INT': (value) => this.parseIntFunc(value),
      'PARSE_FLOAT': (value) => this.parseFloatFunc(value),
      'STRING': (value) => this.string(value),
      'INT': (value) => this.int(value),
      'FLOAT': (value) => this.float(value),
      'CHAR': (value) => this.char(value),
      'CODE': (value) => this.code(value),
      'BIN': (value) => this.bin(value),
      'HEX': (value) => this.hex(value),

      // Array operations
      'APPEND': (arr, value) => this.append(arr, value),
      'PREPEND': (arr, value) => this.prepend(arr, value),
      'SLICE': (arr, start, end) => this.slice(arr, start, end),
      'FIRST': (arr) => this.first(arr),
      'LAST': (arr) => this.last(arr),
      'CONCAT': (arr1, arr2) => this.concat(arr1, arr2),

      // Map operations
      'KEYS': (m) => this.keys(m),
      'VALUES': (m) => this.values(m),
      'HAS': (m, key) => this.has(m, key),
      'SET': (m, key, value) => this.set(m, key, value),
      'DELETE': (m, key) => this.delete(m, key),

      // String operations
      'SPLIT': (s, delimiter) => this.split(s, delimiter),
      'JOIN': (arr, delimiter) => this.join(arr, delimiter),
      'SUBSTRING': (s, start, end) => this.substring(s, start, end),
      'UPPERCASE': (s) => this.uppercase(s),
      'LOWERCASE': (s) => this.lowercase(s),
      'TRIM': (s) => this.trim(s),
      'REPLACE': (s, old, newStr) => this.replace(s, old, newStr),

      // Utility
      'RANDOM': () => this.random(),
      'RANDOM_INT': (min, max) => this.randomInt(min, max),
      'TIME': () => this.time(),
    };
    return builtins[name] || null;
  }

  // ============ I/O ============

  utter(...args) {
    const output = args.map(stringify).join(' ');
    this.onOutput(output);
    return null;
  }

  heed() {
    if (this.inputQueue.length > this._inputIndex) {
      return this.inputQueue[this._inputIndex++];
    }
    if (this.onInput) {
      return this.onInput();
    }
    return '';
  }

  scry(path) {
    if (path !== null && path !== undefined) {
      throw new RuntimeError(`SCRY expects VOID (for stdin), got ${typeName(path)}`);
    }
    if (this.scryQueue.length > this._scryIndex) {
      return this.scryQueue[this._scryIndex++];
    }
    return '';
  }

  // ============ Type Operations ============

  typeof_(value) {
    return typeName(value);
  }

  length(value) {
    if (typeof value === 'string' || Array.isArray(value)) {
      return value.length;
    }
    throw new RuntimeError(`LENGTH expects string or array, got ${typeName(value)}`);
  }

  parseIntFunc(value) {
    if (typeof value !== 'string') {
      throw new RuntimeError(`PARSE_INT expects string, got ${typeName(value)}`);
    }
    if (value.includes('.')) {
      throw new RuntimeError(`Cannot parse '${value}' as integer`);
    }
    const result = parseInt(value, 10);
    if (isNaN(result)) {
      throw new RuntimeError(`Cannot parse '${value}' as integer`);
    }
    return result;
  }

  parseFloatFunc(value) {
    if (typeof value !== 'string') {
      throw new RuntimeError(`PARSE_FLOAT expects string, got ${typeName(value)}`);
    }
    const result = parseFloat(value);
    if (isNaN(result)) {
      throw new RuntimeError(`Cannot parse '${value}' as float`);
    }
    return result;
  }

  string(value) {
    return stringify(value);
  }

  int(value) {
    if (typeof value === 'number') {
      return Math.trunc(value);
    }
    throw new RuntimeError(`INT expects number, got ${typeName(value)}`);
  }

  float(value) {
    if (typeof value === 'number') {
      return value;
    }
    throw new RuntimeError(`FLOAT expects number, got ${typeName(value)}`);
  }

  char(value) {
    if (!Number.isInteger(value)) {
      throw new RuntimeError(`CHAR expects integer, got ${typeName(value)}`);
    }
    try {
      return String.fromCodePoint(value);
    } catch (e) {
      throw new RuntimeError(`Invalid code point: ${value}`);
    }
  }

  code(value) {
    if (typeof value !== 'string') {
      throw new RuntimeError(`CODE expects string, got ${typeName(value)}`);
    }
    if (value.length === 0) {
      throw new RuntimeError('CODE called on empty string');
    }
    return value.codePointAt(0);
  }

  bin(value) {
    if (!Number.isInteger(value)) {
      throw new RuntimeError(`BIN expects integer, got ${typeName(value)}`);
    }
    return (value >>> 0).toString(2); // Unsigned binary string representation
  }

  hex(value) {
    if (!Number.isInteger(value)) {
      throw new RuntimeError(`HEX expects integer, got ${typeName(value)}`);
    }
    return (value >>> 0).toString(16).toUpperCase(); // Unsigned hex string
  }

  // ============ Array Operations ============

  append(arr, value) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`APPEND expects array, got ${typeName(arr)}`);
    }
    return [...arr, value];
  }

  prepend(arr, value) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`PREPEND expects array, got ${typeName(arr)}`);
    }
    return [value, ...arr];
  }

  slice(arr, start, end) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`SLICE expects array, got ${typeName(arr)}`);
    }
    if (typeof start !== 'number' || typeof end !== 'number') {
      throw new RuntimeError('SLICE expects integer indices');
    }
    return arr.slice(start, end);
  }

  first(arr) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`FIRST expects array, got ${typeName(arr)}`);
    }
    if (arr.length === 0) {
      throw new RuntimeError('FIRST called on empty array');
    }
    return arr[0];
  }

  last(arr) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`LAST expects array, got ${typeName(arr)}`);
    }
    if (arr.length === 0) {
      throw new RuntimeError('LAST called on empty array');
    }
    return arr[arr.length - 1];
  }

  concat(arr1, arr2) {
    if (!Array.isArray(arr1) || !Array.isArray(arr2)) {
      throw new RuntimeError('CONCAT expects two arrays');
    }
    return [...arr1, ...arr2];
  }

  // ============ Map Operations ============

  keys(m) {
    if (typeof m !== 'object' || m === null || Array.isArray(m)) {
      throw new RuntimeError(`KEYS expects map, got ${typeName(m)}`);
    }
    return Object.keys(m);
  }

  values(m) {
    if (typeof m !== 'object' || m === null || Array.isArray(m)) {
      throw new RuntimeError(`VALUES expects map, got ${typeName(m)}`);
    }
    return Object.values(m);
  }

  has(m, key) {
    if (typeof m !== 'object' || m === null || Array.isArray(m)) {
      throw new RuntimeError(`HAS expects map, got ${typeName(m)}`);
    }
    return key in m;
  }

  set(m, key, value) {
    if (typeof m !== 'object' || m === null || Array.isArray(m)) {
      throw new RuntimeError(`SET expects map, got ${typeName(m)}`);
    }
    return { ...m, [key]: value };
  }

  delete(m, key) {
    if (typeof m !== 'object' || m === null || Array.isArray(m)) {
      throw new RuntimeError(`DELETE expects map, got ${typeName(m)}`);
    }
    const result = { ...m };
    delete result[key];
    return result;
  }

  // ============ String Operations ============

  split(s, delimiter) {
    if (typeof s !== 'string' || typeof delimiter !== 'string') {
      throw new RuntimeError('SPLIT expects two strings');
    }
    if (delimiter === '') {
      return [...s];
    }
    return s.split(delimiter);
  }

  join(arr, delimiter) {
    if (!Array.isArray(arr)) {
      throw new RuntimeError(`JOIN expects array, got ${typeName(arr)}`);
    }
    if (typeof delimiter !== 'string') {
      throw new RuntimeError(`JOIN expects string delimiter, got ${typeName(delimiter)}`);
    }
    return arr.map(v => typeof v === 'string' ? v : stringify(v)).join(delimiter);
  }

  substring(s, start, end) {
    if (typeof s !== 'string') {
      throw new RuntimeError(`SUBSTRING expects string, got ${typeName(s)}`);
    }
    if (typeof start !== 'number' || typeof end !== 'number') {
      throw new RuntimeError('SUBSTRING expects integer indices');
    }
    return s.slice(start, end);
  }

  uppercase(s) {
    if (typeof s !== 'string') {
      throw new RuntimeError(`UPPERCASE expects string, got ${typeName(s)}`);
    }
    return s.toUpperCase();
  }

  lowercase(s) {
    if (typeof s !== 'string') {
      throw new RuntimeError(`LOWERCASE expects string, got ${typeName(s)}`);
    }
    return s.toLowerCase();
  }

  trim(s) {
    if (typeof s !== 'string') {
      throw new RuntimeError(`TRIM expects string, got ${typeName(s)}`);
    }
    return s.trim();
  }

  replace(s, old, newStr) {
    if (typeof s !== 'string' || typeof old !== 'string' || typeof newStr !== 'string') {
      throw new RuntimeError('REPLACE expects three strings');
    }
    return s.split(old).join(newStr);
  }

  // ============ Utility ============

  random() {
    return Math.random();
  }

  randomInt(min, max) {
    if (typeof min !== 'number' || typeof max !== 'number') {
      throw new RuntimeError('RANDOM_INT expects two integers');
    }
    return Math.floor(Math.random() * (max - min + 1)) + min;
  }

  time() {
    return Date.now();
  }
}
