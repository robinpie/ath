/**
 * Edge case tests for the !~ATH interpreter.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import { TildeAth } from '../src/index.js';
import { RuntimeError, CondemnError, ParseError } from '../src/errors.js';

async function runProgram(source) {
  const output = [];
  const runtime = new TildeAth({
    onOutput: (text) => output.push(text),
  });
  await runtime.run(source);
  return output.join('\n');
}

describe('Edge Cases: Empty Structures', () => {
  it('empty array operations', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [];
        UTTER(LENGTH(arr));
        arr = APPEND(arr, 1);
        UTTER(LENGTH(arr));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['0', '1']);
  });

  it('empty map operations', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {};
        UTTER(LENGTH(KEYS(m)));
        m = SET(m, "a", 1);
        UTTER(LENGTH(KEYS(m)));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['0', '1']);
  });

  it('empty string operations', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH s WITH "";
        UTTER(LENGTH(s));
        s = s + "hello";
        UTTER(LENGTH(s));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['0', '5']);
  });
});

describe('Edge Cases: Nested Structures', () => {
  it('deeply nested array', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
        UTTER(arr[0][1][0]);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '3');
  });

  it('nested map access', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {outer: {inner: {value: 42}}};
        UTTER(m.outer.inner.value);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '42');
  });

  it('array of maps', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [{name: "a"}, {name: "b"}];
        UTTER(arr[1].name);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'b');
  });

  it('map with array values', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {nums: [1, 2, 3]};
        UTTER(m.nums[1]);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '2');
  });
});

describe('Edge Cases: Recursion', () => {
  it('mutual recursion', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE isEven(n) {
          SHOULD n == 0 { BEQUEATH ALIVE; }
          BEQUEATH isOdd(n - 1);
        }
        RITE isOdd(n) {
          SHOULD n == 0 { BEQUEATH DEAD; }
          BEQUEATH isEven(n - 1);
        }
        SHOULD isEven(4) { UTTER("4 is even"); }
        SHOULD isOdd(5) { UTTER("5 is odd"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['4 is even', '5 is odd']);
  });

  it('fibonacci', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE fib(n) {
          SHOULD n <= 1 { BEQUEATH n; }
          BEQUEATH fib(n - 1) + fib(n - 2);
        }
        UTTER(fib(10));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '55');
  });
});

describe('Edge Cases: Scoping', () => {
  it('shadowing', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 1;
        RITE test() {
          BIRTH x WITH 2;
          BEQUEATH x;
        }
        UTTER(test());
        UTTER(x);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['2', '1']);
  });
});

describe('Edge Cases: Timer Chaining', () => {
  it('rapid timer chain', async () => {
    const source = `
      BIRTH count WITH 0;
      RITE increment() {
        count = count + 1;
        SHOULD count < 5 {
          import timer T(1ms);
          ~ATH(T) { } EXECUTE(increment());
        } LEST {
          UTTER(count);
        }
      }
      increment();
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '5');
  });

  it('timer in conditional', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH flag WITH ALIVE;
        SHOULD flag {
          import timer T2(1ms);
          ~ATH(T2) { } EXECUTE(UTTER("conditional timer"));
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'conditional timer');
  });
});

describe('Edge Cases: Error Handling', () => {
  it('error in rite', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE mayFail(x) {
          SHOULD x < 0 {
            CONDEMN "negative value";
          }
          BEQUEATH x * 2;
        }
        ATTEMPT {
          UTTER(mayFail(-5));
        } SALVAGE err {
          UTTER("Error: " + err);
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Error: negative value');
  });

  it('error propagation', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE outer() {
          inner();
        }
        RITE inner() {
          CONDEMN "inner error";
        }
        ATTEMPT {
          outer();
        } SALVAGE err {
          UTTER("Caught: " + err);
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Caught: inner error');
  });

  it('error in execute', async () => {
    const source = `
      import timer T1(1ms);
      ~ATH(T1) { } EXECUTE(
        ATTEMPT {
          import timer T2(1ms);
          ~ATH(T2) { } EXECUTE(
            CONDEMN "nested error";
          );
        } SALVAGE err {
          UTTER("Outer caught: " + err);
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Outer caught: nested error');
  });
});

describe('Edge Cases: Type Coercion', () => {
  it('string concat with numbers', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        UTTER("Value: " + 42 + " and " + 3.14);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Value: 42 and 3.14');
  });

  it('string concat with bool', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        UTTER("Is alive: " + ALIVE);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Is alive: ALIVE');
  });

  it('truthiness of numbers', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 1 { UTTER("1 is truthy"); }
        SHOULD 0 { UTTER("0 is truthy"); } LEST { UTTER("0 is falsy"); }
        SHOULD 0.0 { UTTER("0.0 is truthy"); } LEST { UTTER("0.0 is falsy"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['1 is truthy', '0 is falsy', '0.0 is falsy']);
  });
});

describe('Edge Cases: Comments', () => {
  it('comment after code', async () => {
    const source = `
      import timer T(1ms); // This is a comment
      ~ATH(T) { } EXECUTE(UTTER("hello")); // Another comment
      THIS.DIE(); // Final comment
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'hello');
  });

  it('comment with keywords', async () => {
    const source = `
      // import timer FAKE(1ms);
      import timer T(1ms);
      // ~ATH(FAKE) { } EXECUTE(UTTER("fake"));
      ~ATH(T) { } EXECUTE(UTTER("real"));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'real');
  });
});

describe('Edge Cases: Special Values', () => {
  it('VOID in array', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, VOID, 3];
        UTTER(arr);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[1, VOID, 3]');
  });

  it('VOID in map', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {a: VOID};
        UTTER(m);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.ok(output.includes('a: VOID'));
  });
});

describe('Edge Cases: Operator Precedence', () => {
  it('complex arithmetic', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        UTTER(2 + 3 * 4 - 5 / 5);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    // 2 + 12 - 1 = 13
    assert.strictEqual(output.trim(), '13');
  });

  it('comparison in logical', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 1 < 2 AND 3 > 2 {
          UTTER("yes");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('negation precedence', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        UTTER(-2 * 3);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '-6');
  });
});

describe('Edge Cases: Index Bounds', () => {
  it('string index', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH s WITH "hello";
        UTTER(s[0]);
        UTTER(s[4]);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['h', 'o']);
  });

  it('array index out of bounds', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, 2, 3];
        ATTEMPT {
          UTTER(arr[10]);
        } SALVAGE err {
          UTTER("out of bounds");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'out of bounds');
  });
});

describe('Edge Cases: Entity Management', () => {
  it('DIE already dead', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER("timer died"));
      T.DIE();  // Already dead, should be no-op
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'timer died');
  });

  it('kill THIS early', async () => {
    const source = `
      THIS.DIE();
      ~ATH(THIS) { } EXECUTE(UTTER("THIS died"));
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'THIS died');
  });
});

describe('Edge Cases: Bifurcation', () => {
  it('bifurcate shared variable', async () => {
    const source = `
      BIRTH counter WITH 0;
      bifurcate THIS[LEFT, RIGHT];

      ~ATH(LEFT) {
        import timer T1(1ms);
        ~ATH(T1) { } EXECUTE(
          counter = counter + 1;
        );
      } EXECUTE(VOID);

      ~ATH(RIGHT) {
        import timer T2(2ms);
        ~ATH(T2) { } EXECUTE(
          counter = counter + 10;
        );
      } EXECUTE(VOID);

      // Wait for both to complete
      import timer wait(10ms);
      ~ATH(wait) { } EXECUTE(UTTER(counter));

      [LEFT, RIGHT].DIE();
    `;
    const output = await runProgram(source);
    // Both branches modify counter
    assert.strictEqual(output.trim(), '11');
  });
});
