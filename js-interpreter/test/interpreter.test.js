/**
 * Tests for the !~ATH interpreter.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import { TildeAth } from '../src/index.js';
import { RuntimeError, CondemnError } from '../src/errors.js';

async function runProgram(source) {
  const output = [];
  const runtime = new TildeAth({
    onOutput: (text) => output.push(text),
  });
  await runtime.run(source);
  return output.join('\n');
}

describe('Interpreter Basics', () => {
  it('hello world', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER("Hello, world!"));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Hello, world!');
  });

  it('empty program', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(VOID);
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '');
  });
});

describe('Interpreter Variables', () => {
  it('BIRTH and UTTER', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 42;
        UTTER(x);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '42');
  });

  it('variable reassignment', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 5;
        x = 10;
        UTTER(x);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '10');
  });

  it('ENTOMB constant', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        ENTOMB PI WITH 3.14159;
        UTTER(PI);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '3.14159');
  });

  it('multiple variables', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 5;
        BIRTH y WITH 10;
        BIRTH z WITH x + y;
        UTTER(z);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '15');
  });
});

describe('Interpreter Arithmetic', () => {
  it('addition', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(5 + 3));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '8');
  });

  it('subtraction', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(10 - 4));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '6');
  });

  it('multiplication', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(7 * 6));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '42');
  });

  it('division', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(20 / 4));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '5');
  });

  it('modulo', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(17 % 5));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '2');
  });

  it('negative numbers', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(-5 + 3));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '-2');
  });

  it('float arithmetic', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(3.5 + 1.5));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '5');
  });

  it('operator precedence', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(2 + 3 * 4));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '14');
  });

  it('parentheses', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER((2 + 3) * 4));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '20');
  });
});

describe('Interpreter Comparison', () => {
  it('equal', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 5 == 5 { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('not equal', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 5 != 3 { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('less than', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 3 < 5 { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('greater than', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD 5 > 3 { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });
});

describe('Interpreter Logical', () => {
  it('AND true', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD ALIVE AND ALIVE { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('AND false', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD ALIVE AND DEAD { UTTER("yes"); } LEST { UTTER("no"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'no');
  });

  it('OR true', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD DEAD OR ALIVE { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });

  it('NOT', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD NOT DEAD { UTTER("yes"); }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'yes');
  });
});

describe('Interpreter Strings', () => {
  it('string concatenation', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER("Hello, " + "world!"));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Hello, world!');
  });

  it('string number concat', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER("Value: " + 42));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Value: 42');
  });
});

describe('Interpreter Arrays', () => {
  it('array literal', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, 2, 3];
        UTTER(arr);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[1, 2, 3]');
  });

  it('array index', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [10, 20, 30];
        UTTER(arr[1]);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '20');
  });

  it('array index assignment', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, 2, 3];
        arr[1] = 99;
        UTTER(arr);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[1, 99, 3]');
  });
});

describe('Interpreter Maps', () => {
  it('map literal', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {x: 1, y: 2};
        UTTER(m);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.ok(output.includes('x: 1'));
    assert.ok(output.includes('y: 2'));
  });

  it('map member access', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {name: "Karkat"};
        UTTER(m.name);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Karkat');
  });

  it('map index access', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {name: "Karkat"};
        UTTER(m["name"]);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Karkat');
  });
});

describe('Interpreter Conditionals', () => {
  it('SHOULD true', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD ALIVE {
          UTTER("true branch");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'true branch');
  });

  it('SHOULD LEST', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        SHOULD DEAD {
          UTTER("true");
        } LEST {
          UTTER("false");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'false');
  });

  it('chained SHOULD', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 2;
        SHOULD x == 1 {
          UTTER("one");
        } LEST SHOULD x == 2 {
          UTTER("two");
        } LEST {
          UTTER("other");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'two');
  });
});

describe('Interpreter Rites', () => {
  it('simple rite', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE greet() {
          UTTER("Hello!");
        }
        greet();
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Hello!');
  });

  it('rite with params', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE add(a, b) {
          BEQUEATH a + b;
        }
        UTTER(add(3, 4));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '7');
  });

  it('rite with BEQUEATH', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE double(x) {
          BEQUEATH x * 2;
        }
        BIRTH result WITH double(21);
        UTTER(result);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '42');
  });

  it('recursive rite', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        RITE factorial(n) {
          SHOULD n <= 1 {
            BEQUEATH 1;
          }
          BEQUEATH n * factorial(n - 1);
        }
        UTTER(factorial(5));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '120');
  });
});

describe('Interpreter Error Handling', () => {
  it('CONDEMN caught', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        ATTEMPT {
          CONDEMN "test error";
        } SALVAGE err {
          UTTER("Caught: " + err);
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Caught: test error');
  });

  it('runtime error caught', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        ATTEMPT {
          BIRTH x WITH PARSE_INT("not a number");
        } SALVAGE err {
          UTTER("Caught error");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'Caught error');
  });
});

describe('Interpreter Timers', () => {
  it('timer chain', async () => {
    const source = `
      import timer T1(1ms);
      ~ATH(T1) { } EXECUTE(
        UTTER("first");
        import timer T2(1ms);
        ~ATH(T2) { } EXECUTE(
          UTTER("second");
        );
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['first', 'second']);
  });

  it('timer reuse name', async () => {
    const source = `
      RITE count(n) {
        SHOULD n > 0 {
          UTTER(n);
          import timer T(1ms);
          ~ATH(T) { } EXECUTE(count(n - 1));
        }
      }
      count(3);
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['3', '2', '1']);
  });
});

describe('Interpreter Entity Combinations', () => {
  it('entity OR', async () => {
    const source = `
      import timer T1(10ms);
      import timer T2(1ms);
      ~ATH(T1 || T2) { } EXECUTE(UTTER("done"));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'done');
  });

  it('entity AND', async () => {
    const source = `
      import timer T1(1ms);
      import timer T2(1ms);
      ~ATH(T1 && T2) { } EXECUTE(UTTER("both done"));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'both done');
  });

  it('entity NOT', async () => {
    const source = `
      import timer T(1s);
      ~ATH(!T) { } EXECUTE(UTTER("timer exists"));
      T.DIE();
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'timer exists');
  });
});

describe('Interpreter Bifurcation', () => {
  it('simple bifurcate', async () => {
    const source = `
      bifurcate THIS[LEFT, RIGHT];

      ~ATH(LEFT) {
        import timer T1(1ms);
        ~ATH(T1) { } EXECUTE(UTTER("left"));
      } EXECUTE(VOID);

      ~ATH(RIGHT) {
        import timer T2(1ms);
        ~ATH(T2) { } EXECUTE(UTTER("right"));
      } EXECUTE(VOID);

      [LEFT, RIGHT].DIE();
    `;
    const output = await runProgram(source);
    const lines = new Set(output.trim().split('\n'));
    assert.deepStrictEqual(lines, new Set(['left', 'right']));
  });
});

describe('Interpreter Builtins', () => {
  it('UTTER multiple args', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(1, 2, 3));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '1 2 3');
  });

  it('TYPEOF', async () => {
    const tests = [
      ['42', 'INTEGER'],
      ['3.14', 'FLOAT'],
      ['"hello"', 'STRING'],
      ['ALIVE', 'BOOLEAN'],
      ['VOID', 'VOID'],
      ['[1, 2]', 'ARRAY'],
      ['{x: 1}', 'MAP'],
    ];
    for (const [value, expected] of tests) {
      const source = `
        import timer T(1ms);
        ~ATH(T) { } EXECUTE(UTTER(TYPEOF(${value})));
        THIS.DIE();
      `;
      const output = await runProgram(source);
      assert.strictEqual(output.trim(), expected, `Failed for ${value}`);
    }
  });

  it('LENGTH string', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(LENGTH("hello")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '5');
  });

  it('LENGTH array', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(LENGTH([1, 2, 3])));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '3');
  });

  it('PARSE_INT', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(PARSE_INT("42")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '42');
  });

  it('APPEND', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, 2];
        arr = APPEND(arr, 3);
        UTTER(arr);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[1, 2, 3]');
  });

  it('SLICE', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH arr WITH [1, 2, 3, 4, 5];
        UTTER(SLICE(arr, 1, 4));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[2, 3, 4]');
  });

  it('KEYS', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH m WITH {a: 1, b: 2};
        UTTER(KEYS(m));
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.ok(output.includes('a'));
    assert.ok(output.includes('b'));
  });

  it('SPLIT', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(SPLIT("a,b,c", ",")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '[a, b, c]');
  });

  it('JOIN', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(JOIN(["a", "b", "c"], "-")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'a-b-c');
  });

  it('UPPERCASE', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(UPPERCASE("hello")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'HELLO');
  });

  it('REPLACE', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(REPLACE("hello", "l", "w")));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'hewwo');
  });

  it('RANDOM range', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH r WITH RANDOM();
        SHOULD r >= 0 AND r < 1 {
          UTTER("valid");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'valid');
  });

  it('TIME', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH t WITH TIME();
        SHOULD t > 0 {
          UTTER("valid");
        }
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), 'valid');
  });
});

describe('Interpreter Scoping', () => {
  it('global scope', async () => {
    const source = `
      BIRTH x WITH 10;
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(UTTER(x));
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '10');
  });

  it('local scope', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 5;
        RITE test() {
          BIRTH x WITH 10;
          BEQUEATH x;
        }
        UTTER(test());
        UTTER(x);
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    const lines = output.trim().split('\n');
    assert.deepStrictEqual(lines, ['10', '5']);
  });

  it('closure scope', async () => {
    const source = `
      import timer T(1ms);
      ~ATH(T) { } EXECUTE(
        BIRTH x WITH 5;
        RITE inner() {
          BEQUEATH x;
        }
        UTTER(inner());
      );
      THIS.DIE();
    `;
    const output = await runProgram(source);
    assert.strictEqual(output.trim(), '5');
  });
});
