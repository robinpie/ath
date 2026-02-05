/**
 * Tests for the !~ATH parser.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import { Lexer } from '../src/lexer.js';
import { Parser } from '../src/parser.js';
import { ParseError } from '../src/errors.js';

function parse(source) {
  const lexer = new Lexer(source);
  const tokens = lexer.tokenize();
  const parser = new Parser(tokens);
  return parser.parse();
}

describe('Parser Import', () => {
  it('import timer with ms', () => {
    const program = parse('import timer T(100ms);');
    assert.strictEqual(program.statements.length, 1);
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'ImportStmt');
    assert.strictEqual(stmt.entityType, 'timer');
    assert.strictEqual(stmt.name, 'T');
    assert.strictEqual(stmt.args[0].type, 'Duration');
    assert.strictEqual(stmt.args[0].unit, 'ms');
    assert.strictEqual(stmt.args[0].value, 100);
  });

  it('import timer with seconds', () => {
    const program = parse('import timer delay(5s);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.args[0].unit, 's');
    assert.strictEqual(stmt.args[0].value, 5);
  });

  it('import timer with minutes', () => {
    const program = parse('import timer wait(2m);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.args[0].unit, 'm');
    assert.strictEqual(stmt.args[0].value, 2);
  });

  it('import timer with hours', () => {
    const program = parse('import timer long(1h);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.args[0].unit, 'h');
    assert.strictEqual(stmt.args[0].value, 1);
  });

  it('import timer with no unit defaults to ms', () => {
    const program = parse('import timer T(100);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.args[0].unit, 'ms');
    assert.strictEqual(stmt.args[0].value, 100);
  });

  it('import process', () => {
    const program = parse('import process P("./script.sh");');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityType, 'process');
    assert.strictEqual(stmt.name, 'P');
    assert.strictEqual(stmt.args[0].type, 'Literal');
    assert.strictEqual(stmt.args[0].value, './script.sh');
  });

  it('import process with args', () => {
    const program = parse('import process P("python", "script.py", "--verbose");');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.args.length, 3);
    assert.strictEqual(stmt.args[0].value, 'python');
    assert.strictEqual(stmt.args[1].value, 'script.py');
    assert.strictEqual(stmt.args[2].value, '--verbose');
  });

  it('import connection', () => {
    const program = parse('import connection C("localhost", 8080);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityType, 'connection');
    assert.strictEqual(stmt.name, 'C');
    assert.strictEqual(stmt.args.length, 2);
    assert.strictEqual(stmt.args[0].value, 'localhost');
    assert.strictEqual(stmt.args[1].value, 8080);
  });

  it('import watcher', () => {
    const program = parse('import watcher W("./config.txt");');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityType, 'watcher');
    assert.strictEqual(stmt.name, 'W');
    assert.strictEqual(stmt.args[0].value, './config.txt');
  });
});

describe('Parser Bifurcate', () => {
  it('bifurcate THIS', () => {
    const program = parse('bifurcate THIS[LEFT, RIGHT];');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'BifurcateStmt');
    assert.strictEqual(stmt.entity, 'THIS');
    assert.strictEqual(stmt.branch1, 'LEFT');
    assert.strictEqual(stmt.branch2, 'RIGHT');
  });

  it('bifurcate identifier', () => {
    const program = parse('bifurcate myEntity[A, B];');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entity, 'myEntity');
    assert.strictEqual(stmt.branch1, 'A');
    assert.strictEqual(stmt.branch2, 'B');
  });
});

describe('Parser ~ATH Loop', () => {
  it('simple ~ATH loop', () => {
    const program = parse('~ATH(T) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'AthLoop');
    assert.strictEqual(stmt.entityExpr.type, 'EntityIdent');
    assert.strictEqual(stmt.entityExpr.name, 'T');
    assert.strictEqual(stmt.body.length, 0);
  });

  it('~ATH with THIS', () => {
    const program = parse('~ATH(THIS) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityExpr.name, 'THIS');
  });

  it('~ATH with AND', () => {
    const program = parse('~ATH(T1 && T2) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityExpr.type, 'EntityAnd');
    assert.strictEqual(stmt.entityExpr.left.name, 'T1');
    assert.strictEqual(stmt.entityExpr.right.name, 'T2');
  });

  it('~ATH with OR', () => {
    const program = parse('~ATH(T1 || T2) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityExpr.type, 'EntityOr');
  });

  it('~ATH with NOT', () => {
    const program = parse('~ATH(!T) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityExpr.type, 'EntityNot');
    assert.strictEqual(stmt.entityExpr.operand.name, 'T');
  });

  it('~ATH with complex entity expression', () => {
    const program = parse('~ATH((T1 && T2) || T3) { } EXECUTE(VOID);');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.entityExpr.type, 'EntityOr');
    assert.strictEqual(stmt.entityExpr.left.type, 'EntityAnd');
  });

  it('~ATH with execute expression', () => {
    const program = parse('~ATH(T) { } EXECUTE(UTTER("hello"));');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.execute.length, 1);
    assert.strictEqual(stmt.execute[0].type, 'ExprStmt');
  });
});

describe('Parser DIE', () => {
  it('THIS.DIE()', () => {
    const program = parse('THIS.DIE();');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'DieStmt');
    assert.strictEqual(stmt.target.type, 'DieIdent');
    assert.strictEqual(stmt.target.name, 'THIS');
  });

  it('identifier.DIE()', () => {
    const program = parse('myTimer.DIE();');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.target.name, 'myTimer');
  });

  it('[LEFT, RIGHT].DIE()', () => {
    const program = parse('[LEFT, RIGHT].DIE();');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.target.type, 'DiePair');
    assert.strictEqual(stmt.target.left.name, 'LEFT');
    assert.strictEqual(stmt.target.right.name, 'RIGHT');
  });

  it('nested pair DIE', () => {
    const program = parse('[A, [B1, B2]].DIE();');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.target.right.type, 'DiePair');
  });
});

describe('Parser Variable Declaration', () => {
  it('BIRTH with integer', () => {
    const program = parse('BIRTH x WITH 5;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'VarDecl');
    assert.strictEqual(stmt.name, 'x');
    assert.strictEqual(stmt.value.type, 'Literal');
    assert.strictEqual(stmt.value.value, 5);
  });

  it('BIRTH with string', () => {
    const program = parse('BIRTH name WITH "Karkat";');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.value.value, 'Karkat');
  });

  it('BIRTH with array', () => {
    const program = parse('BIRTH arr WITH [1, 2, 3];');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.value.type, 'ArrayLiteral');
    assert.strictEqual(stmt.value.elements.length, 3);
  });

  it('BIRTH with map', () => {
    const program = parse('BIRTH obj WITH {x: 1, y: 2};');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.value.type, 'MapLiteral');
    assert.strictEqual(stmt.value.entries.length, 2);
  });
});

describe('Parser Constant Declaration', () => {
  it('ENTOMB', () => {
    const program = parse('ENTOMB PI WITH 3.14159;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'ConstDecl');
    assert.strictEqual(stmt.name, 'PI');
    assert.ok(Math.abs(stmt.value.value - 3.14159) < 0.00001);
  });
});

describe('Parser Assignment', () => {
  it('simple assignment', () => {
    const program = parse('x = 10;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'Assignment');
    assert.strictEqual(stmt.target.name, 'x');
    assert.strictEqual(stmt.value.value, 10);
  });

  it('index assignment', () => {
    const program = parse('arr[0] = 5;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.target.type, 'IndexExpr');
  });

  it('member assignment', () => {
    const program = parse('obj.x = 5;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.target.type, 'MemberExpr');
  });
});

describe('Parser Rite Definition', () => {
  it('simple rite', () => {
    const program = parse('RITE greet() { UTTER("hello"); }');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'RiteDef');
    assert.strictEqual(stmt.name, 'greet');
    assert.strictEqual(stmt.params.length, 0);
  });

  it('rite with params', () => {
    const program = parse('RITE add(a, b) { BEQUEATH a + b; }');
    const stmt = program.statements[0];
    assert.deepStrictEqual(stmt.params, ['a', 'b']);
  });

  it('rite with bequeath', () => {
    const program = parse('RITE identity(x) { BEQUEATH x; }');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.body.length, 1);
    assert.strictEqual(stmt.body[0].type, 'BequeathStmt');
  });
});

describe('Parser Conditional', () => {
  it('SHOULD only', () => {
    const program = parse('SHOULD ALIVE { UTTER(1); }');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'Conditional');
    assert.strictEqual(stmt.elseBranch, null);
  });

  it('SHOULD LEST', () => {
    const program = parse('SHOULD ALIVE { UTTER(1); } LEST { UTTER(2); }');
    const stmt = program.statements[0];
    assert.notStrictEqual(stmt.elseBranch, null);
  });

  it('chained SHOULD', () => {
    const program = parse('SHOULD x == 1 { } LEST SHOULD x == 2 { } LEST { }');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.elseBranch[0].type, 'Conditional');
  });
});

describe('Parser Attempt Salvage', () => {
  it('ATTEMPT SALVAGE', () => {
    const program = parse('ATTEMPT { UTTER(1); } SALVAGE err { UTTER(err); }');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'AttemptSalvage');
    assert.strictEqual(stmt.errorName, 'err');
  });
});

describe('Parser Condemn', () => {
  it('CONDEMN', () => {
    const program = parse('CONDEMN "error message";');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'CondemnStmt');
    assert.strictEqual(stmt.message.value, 'error message');
  });
});

describe('Parser Bequeath', () => {
  it('BEQUEATH with value', () => {
    const program = parse('BEQUEATH 42;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.type, 'BequeathStmt');
    assert.strictEqual(stmt.value.value, 42);
  });

  it('BEQUEATH without value', () => {
    const program = parse('BEQUEATH;');
    const stmt = program.statements[0];
    assert.strictEqual(stmt.value, null);
  });
});

describe('Parser Expressions', () => {
  it('literal integer', () => {
    const program = parse('BIRTH x WITH 42;');
    assert.strictEqual(program.statements[0].value.type, 'Literal');
    assert.strictEqual(program.statements[0].value.value, 42);
  });

  it('literal float', () => {
    const program = parse('BIRTH x WITH 3.14;');
    assert.strictEqual(program.statements[0].value.value, 3.14);
  });

  it('literal string', () => {
    const program = parse('BIRTH x WITH "hello";');
    assert.strictEqual(program.statements[0].value.value, 'hello');
  });

  it('literal ALIVE', () => {
    const program = parse('BIRTH x WITH ALIVE;');
    assert.strictEqual(program.statements[0].value.value, true);
  });

  it('literal DEAD', () => {
    const program = parse('BIRTH x WITH DEAD;');
    assert.strictEqual(program.statements[0].value.value, false);
  });

  it('literal VOID', () => {
    const program = parse('BIRTH x WITH VOID;');
    assert.strictEqual(program.statements[0].value.value, null);
  });

  it('identifier', () => {
    const program = parse('BIRTH x WITH myVar;');
    assert.strictEqual(program.statements[0].value.type, 'Identifier');
    assert.strictEqual(program.statements[0].value.name, 'myVar');
  });

  it('binary add', () => {
    const program = parse('BIRTH x WITH 1 + 2;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.type, 'BinaryOp');
    assert.strictEqual(expr.operator, '+');
  });

  it('binary subtract', () => {
    const program = parse('BIRTH x WITH 5 - 3;');
    assert.strictEqual(program.statements[0].value.operator, '-');
  });

  it('binary multiply', () => {
    const program = parse('BIRTH x WITH 2 * 3;');
    assert.strictEqual(program.statements[0].value.operator, '*');
  });

  it('binary divide', () => {
    const program = parse('BIRTH x WITH 10 / 2;');
    assert.strictEqual(program.statements[0].value.operator, '/');
  });

  it('binary modulo', () => {
    const program = parse('BIRTH x WITH 7 % 3;');
    assert.strictEqual(program.statements[0].value.operator, '%');
  });

  it('comparison ==', () => {
    const program = parse('BIRTH r WITH x == y;');
    assert.strictEqual(program.statements[0].value.operator, '==');
  });

  it('comparison !=', () => {
    const program = parse('BIRTH r WITH x != y;');
    assert.strictEqual(program.statements[0].value.operator, '!=');
  });

  it('comparison <', () => {
    const program = parse('BIRTH r WITH x < y;');
    assert.strictEqual(program.statements[0].value.operator, '<');
  });

  it('comparison >', () => {
    const program = parse('BIRTH r WITH x > y;');
    assert.strictEqual(program.statements[0].value.operator, '>');
  });

  it('comparison <=', () => {
    const program = parse('BIRTH r WITH x <= y;');
    assert.strictEqual(program.statements[0].value.operator, '<=');
  });

  it('comparison >=', () => {
    const program = parse('BIRTH r WITH x >= y;');
    assert.strictEqual(program.statements[0].value.operator, '>=');
  });

  it('logical AND', () => {
    const program = parse('BIRTH r WITH x AND y;');
    assert.strictEqual(program.statements[0].value.operator, 'AND');
  });

  it('logical OR', () => {
    const program = parse('BIRTH r WITH x OR y;');
    assert.strictEqual(program.statements[0].value.operator, 'OR');
  });

  it('unary NOT', () => {
    const program = parse('BIRTH r WITH NOT x;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.type, 'UnaryOp');
    assert.strictEqual(expr.operator, 'NOT');
  });

  it('unary minus', () => {
    const program = parse('BIRTH r WITH -x;');
    assert.strictEqual(program.statements[0].value.operator, '-');
  });

  it('unary bitwise not', () => {
    const program = parse('BIRTH r WITH ~x;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.type, 'UnaryOp');
    assert.strictEqual(expr.operator, '~');
  });

  it('bitwise AND', () => {
    const program = parse('BIRTH r WITH x & y;');
    assert.strictEqual(program.statements[0].value.operator, '&');
  });

  it('bitwise OR', () => {
    const program = parse('BIRTH r WITH x | y;');
    assert.strictEqual(program.statements[0].value.operator, '|');
  });

  it('bitwise XOR', () => {
    const program = parse('BIRTH r WITH x ^ y;');
    assert.strictEqual(program.statements[0].value.operator, '^');
  });

  it('bitwise shift', () => {
    let program = parse('BIRTH r WITH x << y;');
    assert.strictEqual(program.statements[0].value.operator, '<<');
    program = parse('BIRTH r WITH x >> y;');
    assert.strictEqual(program.statements[0].value.operator, '>>');
  });

  it('call with no args', () => {
    const program = parse('func();');
    const expr = program.statements[0].expression;
    assert.strictEqual(expr.type, 'CallExpr');
    assert.strictEqual(expr.args.length, 0);
  });

  it('call with args', () => {
    const program = parse('func(1, 2, 3);');
    const expr = program.statements[0].expression;
    assert.strictEqual(expr.args.length, 3);
  });

  it('index expression', () => {
    const program = parse('BIRTH x WITH arr[0];');
    assert.strictEqual(program.statements[0].value.type, 'IndexExpr');
  });

  it('member expression', () => {
    const program = parse('BIRTH x WITH obj.field;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.type, 'MemberExpr');
    assert.strictEqual(expr.member, 'field');
  });

  it('empty array literal', () => {
    const program = parse('BIRTH x WITH [];');
    assert.strictEqual(program.statements[0].value.type, 'ArrayLiteral');
    assert.strictEqual(program.statements[0].value.elements.length, 0);
  });

  it('array literal', () => {
    const program = parse('BIRTH x WITH [1, 2, 3];');
    assert.strictEqual(program.statements[0].value.elements.length, 3);
  });

  it('empty map literal', () => {
    const program = parse('BIRTH x WITH {};');
    assert.strictEqual(program.statements[0].value.type, 'MapLiteral');
    assert.strictEqual(program.statements[0].value.entries.length, 0);
  });

  it('map literal', () => {
    const program = parse('BIRTH x WITH {x: 1, y: 2};');
    assert.strictEqual(program.statements[0].value.entries.length, 2);
  });

  it('map literal with string keys', () => {
    const program = parse('BIRTH x WITH {"key": 1};');
    assert.strictEqual(program.statements[0].value.entries[0][0], 'key');
  });

  it('grouped expression', () => {
    const program = parse('BIRTH x WITH (1 + 2) * 3;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.operator, '*');
    assert.strictEqual(expr.left.operator, '+');
  });
});

describe('Parser Precedence', () => {
  it('multiply before add', () => {
    const program = parse('BIRTH x WITH 1 + 2 * 3;');
    const expr = program.statements[0].value;
    // Should be 1 + (2 * 3)
    assert.strictEqual(expr.operator, '+');
    assert.strictEqual(expr.right.operator, '*');
  });

  it('divide before subtract', () => {
    const program = parse('BIRTH x WITH 6 - 4 / 2;');
    const expr = program.statements[0].value;
    // Should be 6 - (4 / 2)
    assert.strictEqual(expr.operator, '-');
    assert.strictEqual(expr.right.operator, '/');
  });

  it('comparison before logical', () => {
    const program = parse('BIRTH x WITH x < 5 AND y > 3;');
    const expr = program.statements[0].value;
    // Should be (x < 5) AND (y > 3)
    assert.strictEqual(expr.operator, 'AND');
  });

  it('AND before OR', () => {
    const program = parse('BIRTH x WITH a OR b AND c;');
    const expr = program.statements[0].value;
    // Should be a OR (b AND c)
    assert.strictEqual(expr.operator, 'OR');
    assert.strictEqual(expr.right.operator, 'AND');
  });

  it('bitwise precedence', () => {
    // Shift > And > Xor > Or
    // 1 | 2 ^ 3 & 4 << 5
    // Expected: 1 | (2 ^ (3 & (4 << 5)))
    const program = parse('BIRTH x WITH 1 | 2 ^ 3 & 4 << 5;');
    const expr = program.statements[0].value;
    assert.strictEqual(expr.operator, '|');
    assert.strictEqual(expr.right.operator, '^');
    assert.strictEqual(expr.right.right.operator, '&');
    assert.strictEqual(expr.right.right.right.operator, '<<');
  });
});

describe('Parser Errors', () => {
  it('missing semicolon', () => {
    assert.throws(() => parse('BIRTH x WITH 5'), ParseError);
  });

  it('missing paren', () => {
    assert.throws(() => parse('~ATH(T { } EXECUTE(VOID);'), ParseError);
  });

  it('missing EXECUTE', () => {
    assert.throws(() => parse('~ATH(T) { }'), ParseError);
  });
});
