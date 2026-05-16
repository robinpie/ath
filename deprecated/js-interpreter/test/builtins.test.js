/**
 * Tests for the !~ATH built-in rites.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import { Builtins, stringify, typeName, isTruthy } from '../src/builtins.js';
import { RuntimeError } from '../src/errors.js';

describe('stringify', () => {
  it('stringify null', () => {
    assert.strictEqual(stringify(null), 'VOID');
  });

  it('stringify true', () => {
    assert.strictEqual(stringify(true), 'ALIVE');
  });

  it('stringify false', () => {
    assert.strictEqual(stringify(false), 'DEAD');
  });

  it('stringify int', () => {
    assert.strictEqual(stringify(42), '42');
  });

  it('stringify float', () => {
    assert.strictEqual(stringify(3.14), '3.14');
  });

  it('stringify string', () => {
    assert.strictEqual(stringify('hello'), 'hello');
  });

  it('stringify empty array', () => {
    assert.strictEqual(stringify([]), '[]');
  });

  it('stringify array', () => {
    assert.strictEqual(stringify([1, 2, 3]), '[1, 2, 3]');
  });

  it('stringify nested array', () => {
    assert.strictEqual(stringify([[1, 2], [3, 4]]), '[[1, 2], [3, 4]]');
  });

  it('stringify empty map', () => {
    assert.strictEqual(stringify({}), '{}');
  });

  it('stringify map', () => {
    const result = stringify({ a: 1, b: 2 });
    assert.ok(result.includes('a: 1'));
    assert.ok(result.includes('b: 2'));
  });

  it('stringify mixed array', () => {
    const result = stringify([1, 'two', true, null]);
    assert.strictEqual(result, '[1, two, ALIVE, VOID]');
  });
});

describe('typeName', () => {
  it('typeName VOID', () => {
    assert.strictEqual(typeName(null), 'VOID');
  });

  it('typeName BOOLEAN', () => {
    assert.strictEqual(typeName(true), 'BOOLEAN');
    assert.strictEqual(typeName(false), 'BOOLEAN');
  });

  it('typeName INTEGER', () => {
    assert.strictEqual(typeName(42), 'INTEGER');
  });

  it('typeName FLOAT', () => {
    assert.strictEqual(typeName(3.14), 'FLOAT');
  });

  it('typeName STRING', () => {
    assert.strictEqual(typeName('hello'), 'STRING');
  });

  it('typeName ARRAY', () => {
    assert.strictEqual(typeName([1, 2, 3]), 'ARRAY');
  });

  it('typeName MAP', () => {
    assert.strictEqual(typeName({ a: 1 }), 'MAP');
  });
});

describe('isTruthy', () => {
  it('truthy true', () => {
    assert.strictEqual(isTruthy(true), true);
  });

  it('truthy false', () => {
    assert.strictEqual(isTruthy(false), false);
  });

  it('truthy null', () => {
    assert.strictEqual(isTruthy(null), false);
  });

  it('truthy zero', () => {
    assert.strictEqual(isTruthy(0), false);
  });

  it('truthy nonzero', () => {
    assert.strictEqual(isTruthy(1), true);
    assert.strictEqual(isTruthy(-1), true);
  });

  it('truthy empty string', () => {
    assert.strictEqual(isTruthy(''), false);
  });

  it('truthy nonempty string', () => {
    assert.strictEqual(isTruthy('hello'), true);
  });

  it('truthy empty array', () => {
    assert.strictEqual(isTruthy([]), false);
  });

  it('truthy nonempty array', () => {
    assert.strictEqual(isTruthy([1]), true);
  });

  it('truthy empty map', () => {
    assert.strictEqual(isTruthy({}), false);
  });

  it('truthy nonempty map', () => {
    assert.strictEqual(isTruthy({ a: 1 }), true);
  });
});

describe('Builtin Type Operations', () => {
  const builtins = new Builtins(null);

  it('typeof', () => {
    assert.strictEqual(builtins.typeof_(42), 'INTEGER');
    assert.strictEqual(builtins.typeof_(3.14), 'FLOAT');
    assert.strictEqual(builtins.typeof_('hello'), 'STRING');
    assert.strictEqual(builtins.typeof_(true), 'BOOLEAN');
    assert.strictEqual(builtins.typeof_(null), 'VOID');
    assert.strictEqual(builtins.typeof_([1, 2]), 'ARRAY');
    assert.strictEqual(builtins.typeof_({ a: 1 }), 'MAP');
  });

  it('length string', () => {
    assert.strictEqual(builtins.length('hello'), 5);
    assert.strictEqual(builtins.length(''), 0);
  });

  it('length array', () => {
    assert.strictEqual(builtins.length([1, 2, 3]), 3);
    assert.strictEqual(builtins.length([]), 0);
  });

  it('length invalid', () => {
    assert.throws(() => builtins.length(42), RuntimeError);
  });

  it('parseIntFunc', () => {
    assert.strictEqual(builtins.parseIntFunc('42'), 42);
    assert.strictEqual(builtins.parseIntFunc('-7'), -7);
  });

  it('parseIntFunc invalid', () => {
    assert.throws(() => builtins.parseIntFunc('not a number'), RuntimeError);
    assert.throws(() => builtins.parseIntFunc('3.14'), RuntimeError);
  });

  it('parseFloatFunc', () => {
    assert.ok(Math.abs(builtins.parseFloatFunc('3.14') - 3.14) < 0.001);
    assert.ok(Math.abs(builtins.parseFloatFunc('42') - 42.0) < 0.001);
  });

  it('parseFloatFunc invalid', () => {
    assert.throws(() => builtins.parseFloatFunc('not a number'), RuntimeError);
  });

  it('string conversion', () => {
    assert.strictEqual(builtins.string(42), '42');
    assert.strictEqual(builtins.string(true), 'ALIVE');
    assert.strictEqual(builtins.string([1, 2]), '[1, 2]');
  });

  it('int conversion', () => {
    assert.strictEqual(builtins.int(3.7), 3);
    assert.strictEqual(builtins.int(-2.9), -2);
    assert.strictEqual(builtins.int(42), 42);
  });

  it('int conversion invalid', () => {
    assert.throws(() => builtins.int('not a number'), RuntimeError);
  });

  it('float conversion', () => {
    assert.strictEqual(builtins.float(42), 42.0);
    assert.strictEqual(builtins.float(3.14), 3.14);
  });
});

describe('Builtin Array Operations', () => {
  const builtins = new Builtins(null);

  it('append', () => {
    const result = builtins.append([1, 2], 3);
    assert.deepStrictEqual(result, [1, 2, 3]);
  });

  it('append immutable', () => {
    const original = [1, 2];
    const result = builtins.append(original, 3);
    assert.deepStrictEqual(original, [1, 2]);  // Original unchanged
  });

  it('prepend', () => {
    const result = builtins.prepend([2, 3], 1);
    assert.deepStrictEqual(result, [1, 2, 3]);
  });

  it('slice', () => {
    const result = builtins.slice([1, 2, 3, 4, 5], 1, 4);
    assert.deepStrictEqual(result, [2, 3, 4]);
  });

  it('slice full', () => {
    const result = builtins.slice([1, 2, 3], 0, 3);
    assert.deepStrictEqual(result, [1, 2, 3]);
  });

  it('first', () => {
    assert.strictEqual(builtins.first([10, 20, 30]), 10);
  });

  it('first empty', () => {
    assert.throws(() => builtins.first([]), RuntimeError);
  });

  it('last', () => {
    assert.strictEqual(builtins.last([10, 20, 30]), 30);
  });

  it('last empty', () => {
    assert.throws(() => builtins.last([]), RuntimeError);
  });

  it('concat', () => {
    const result = builtins.concat([1, 2], [3, 4]);
    assert.deepStrictEqual(result, [1, 2, 3, 4]);
  });

  it('concat empty', () => {
    const result = builtins.concat([], [1, 2]);
    assert.deepStrictEqual(result, [1, 2]);
  });
});

describe('Builtin Map Operations', () => {
  const builtins = new Builtins(null);

  it('keys', () => {
    const result = builtins.keys({ a: 1, b: 2 });
    assert.deepStrictEqual(new Set(result), new Set(['a', 'b']));
  });

  it('keys empty', () => {
    const result = builtins.keys({});
    assert.deepStrictEqual(result, []);
  });

  it('values', () => {
    const result = builtins.values({ a: 1, b: 2 });
    assert.deepStrictEqual(new Set(result), new Set([1, 2]));
  });

  it('has true', () => {
    assert.strictEqual(builtins.has({ a: 1 }, 'a'), true);
  });

  it('has false', () => {
    assert.strictEqual(builtins.has({ a: 1 }, 'b'), false);
  });

  it('set', () => {
    const result = builtins.set({ a: 1 }, 'b', 2);
    assert.deepStrictEqual(result, { a: 1, b: 2 });
  });

  it('set overwrite', () => {
    const result = builtins.set({ a: 1 }, 'a', 99);
    assert.deepStrictEqual(result, { a: 99 });
  });

  it('set immutable', () => {
    const original = { a: 1 };
    const result = builtins.set(original, 'b', 2);
    assert.deepStrictEqual(original, { a: 1 });
  });

  it('delete', () => {
    const result = builtins.delete({ a: 1, b: 2 }, 'a');
    assert.deepStrictEqual(result, { b: 2 });
  });

  it('delete nonexistent', () => {
    const result = builtins.delete({ a: 1 }, 'b');
    assert.deepStrictEqual(result, { a: 1 });
  });
});

describe('Builtin String Operations', () => {
  const builtins = new Builtins(null);

  it('split', () => {
    const result = builtins.split('a,b,c', ',');
    assert.deepStrictEqual(result, ['a', 'b', 'c']);
  });

  it('split empty delimiter', () => {
    const result = builtins.split('hello', '');
    assert.deepStrictEqual(result, ['h', 'e', 'l', 'l', 'o']);
  });

  it('join', () => {
    const result = builtins.join(['a', 'b', 'c'], ',');
    assert.strictEqual(result, 'a,b,c');
  });

  it('join empty', () => {
    const result = builtins.join([], ',');
    assert.strictEqual(result, '');
  });

  it('substring', () => {
    const result = builtins.substring('hello', 1, 4);
    assert.strictEqual(result, 'ell');
  });

  it('uppercase', () => {
    const result = builtins.uppercase('hello');
    assert.strictEqual(result, 'HELLO');
  });

  it('lowercase', () => {
    const result = builtins.lowercase('HELLO');
    assert.strictEqual(result, 'hello');
  });

  it('trim', () => {
    const result = builtins.trim('  hello  ');
    assert.strictEqual(result, 'hello');
  });

  it('trim tabs newlines', () => {
    const result = builtins.trim('\t\nhello\t\n');
    assert.strictEqual(result, 'hello');
  });

  it('replace', () => {
    const result = builtins.replace('hello', 'l', 'w');
    assert.strictEqual(result, 'hewwo');
  });

  it('replace no match', () => {
    const result = builtins.replace('hello', 'x', 'y');
    assert.strictEqual(result, 'hello');
  });
});

describe('Builtin Utility', () => {
  const builtins = new Builtins(null);

  it('random range', () => {
    for (let i = 0; i < 100; i++) {
      const result = builtins.random();
      assert.ok(result >= 0 && result < 1);
    }
  });

  it('randomInt range', () => {
    for (let i = 0; i < 100; i++) {
      const result = builtins.randomInt(1, 6);
      assert.ok(result >= 1 && result <= 6);
    }
  });

  it('randomInt same', () => {
    const result = builtins.randomInt(5, 5);
    assert.strictEqual(result, 5);
  });

  it('time positive', () => {
    const result = builtins.time();
    assert.ok(result > 0);
  });

  it('time is integer', () => {
    const result = builtins.time();
    assert.ok(Number.isInteger(result));
  });
});

describe('Builtin Bitwise Conversions', () => {
  const builtins = new Builtins(null);

  it('char', () => {
    assert.strictEqual(builtins.char(65), 'A');
    assert.strictEqual(builtins.char(9786), '☺');
  });

  it('char invalid', () => {
    assert.throws(() => builtins.char('A'), RuntimeError);
  });

  it('code', () => {
    assert.strictEqual(builtins.code('A'), 65);
    assert.strictEqual(builtins.code('☺'), 9786);
  });

  it('code invalid', () => {
    assert.throws(() => builtins.code(65), RuntimeError);
    assert.throws(() => builtins.code(''), RuntimeError);
  });

  it('bin', () => {
    assert.strictEqual(builtins.bin(10), '1010');
    assert.strictEqual(builtins.bin(0), '0');
  });

  it('bin invalid', () => {
    assert.throws(() => builtins.bin('10'), RuntimeError);
  });

  it('hex', () => {
    assert.strictEqual(builtins.hex(255), 'FF');
    assert.strictEqual(builtins.hex(10), 'A');
  });

  it('hex invalid', () => {
    assert.throws(() => builtins.hex('255'), RuntimeError);
  });
});
