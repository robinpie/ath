/**
 * Tests for the !~ATH lexer.
 */

import { describe, it } from 'node:test';
import assert from 'node:assert';
import { Lexer } from '../src/lexer.js';
import { TokenType } from '../src/tokens.js';
import { LexerError } from '../src/errors.js';

describe('Lexer Basics', () => {
  it('empty source produces only EOF', () => {
    const lexer = new Lexer('');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens.length, 1);
    assert.strictEqual(tokens[0].type, TokenType.EOF);
  });

  it('whitespace-only source produces only EOF', () => {
    const lexer = new Lexer('   \t\n\r  ');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens.length, 1);
    assert.strictEqual(tokens[0].type, TokenType.EOF);
  });

  it('single line comment is ignored', () => {
    const lexer = new Lexer('// this is a comment');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens.length, 1);
    assert.strictEqual(tokens[0].type, TokenType.EOF);
  });

  it('comment does not consume following lines', () => {
    const lexer = new Lexer('// comment\n42');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens.length, 2);
    assert.strictEqual(tokens[0].type, TokenType.INTEGER);
    assert.strictEqual(tokens[0].value, 42);
  });
});

describe('Lexer Integers', () => {
  it('simple positive integer', () => {
    const lexer = new Lexer('42');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.INTEGER);
    assert.strictEqual(tokens[0].value, 42);
  });

  it('zero is a valid integer', () => {
    const lexer = new Lexer('0');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.INTEGER);
    assert.strictEqual(tokens[0].value, 0);
  });

  it('negative integer', () => {
    const lexer = new Lexer('-7');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.INTEGER);
    assert.strictEqual(tokens[0].value, -7);
  });

  it('large integer', () => {
    const lexer = new Lexer('1234567890');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.INTEGER);
    assert.strictEqual(tokens[0].value, 1234567890);
  });
});

describe('Lexer Floats', () => {
  it('simple float', () => {
    const lexer = new Lexer('3.14');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.FLOAT);
    assert.ok(Math.abs(tokens[0].value - 3.14) < 0.001);
  });

  it('negative float', () => {
    const lexer = new Lexer('-0.5');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.FLOAT);
    assert.ok(Math.abs(tokens[0].value - (-0.5)) < 0.001);
  });

  it('zero as float', () => {
    const lexer = new Lexer('0.0');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.FLOAT);
    assert.strictEqual(tokens[0].value, 0.0);
  });

  it('float with leading zero', () => {
    const lexer = new Lexer('0.123');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.FLOAT);
    assert.ok(Math.abs(tokens[0].value - 0.123) < 0.001);
  });
});

describe('Lexer Durations', () => {
  it('milliseconds duration', () => {
    const lexer = new Lexer('100ms');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DURATION);
    assert.deepStrictEqual(tokens[0].value, { unit: 'ms', value: 100 });
  });

  it('seconds duration', () => {
    const lexer = new Lexer('5s');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DURATION);
    assert.deepStrictEqual(tokens[0].value, { unit: 's', value: 5 });
  });

  it('minutes duration', () => {
    const lexer = new Lexer('2m');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DURATION);
    assert.deepStrictEqual(tokens[0].value, { unit: 'm', value: 2 });
  });

  it('hours duration', () => {
    const lexer = new Lexer('1h');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DURATION);
    assert.deepStrictEqual(tokens[0].value, { unit: 'h', value: 1 });
  });
});

describe('Lexer Strings', () => {
  it('simple string', () => {
    const lexer = new Lexer('"hello"');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.STRING);
    assert.strictEqual(tokens[0].value, 'hello');
  });

  it('empty string', () => {
    const lexer = new Lexer('""');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.STRING);
    assert.strictEqual(tokens[0].value, '');
  });

  it('string with spaces', () => {
    const lexer = new Lexer('"hello world"');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.STRING);
    assert.strictEqual(tokens[0].value, 'hello world');
  });

  it('escape sequence: newline', () => {
    const lexer = new Lexer('"line1\\nline2"');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].value, 'line1\nline2');
  });

  it('escape sequence: tab', () => {
    const lexer = new Lexer('"col1\\tcol2"');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].value, 'col1\tcol2');
  });

  it('escape sequence: backslash', () => {
    const lexer = new Lexer('"path\\\\file"');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].value, 'path\\file');
  });

  it('escape sequence: quote', () => {
    const lexer = new Lexer('"say \\"hello\\""');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].value, 'say "hello"');
  });

  it('unterminated string raises error', () => {
    const lexer = new Lexer('"hello');
    assert.throws(() => lexer.tokenize(), LexerError);
  });
});

describe('Lexer Keywords', () => {
  it('import keyword', () => {
    const lexer = new Lexer('import');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.IMPORT);
  });

  it('bifurcate keyword', () => {
    const lexer = new Lexer('bifurcate');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.BIFURCATE);
  });

  it('EXECUTE keyword', () => {
    const lexer = new Lexer('EXECUTE');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.EXECUTE);
  });

  it('DIE keyword', () => {
    const lexer = new Lexer('DIE');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DIE);
  });

  it('THIS keyword', () => {
    const lexer = new Lexer('THIS');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.THIS);
  });

  it('~ATH special token', () => {
    const lexer = new Lexer('~ATH');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.TILDE_ATH);
  });

  it('entity type keywords', () => {
    const tests = [
      ['timer', TokenType.TIMER],
      ['process', TokenType.PROCESS],
      ['connection', TokenType.CONNECTION],
      ['watcher', TokenType.WATCHER],
    ];
    for (const [keyword, expected] of tests) {
      const lexer = new Lexer(keyword);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${keyword}`);
    }
  });

  it('expression language keywords', () => {
    const keywords = [
      ['BIRTH', TokenType.BIRTH],
      ['ENTOMB', TokenType.ENTOMB],
      ['WITH', TokenType.WITH],
      ['SHOULD', TokenType.SHOULD],
      ['LEST', TokenType.LEST],
      ['RITE', TokenType.RITE],
      ['BEQUEATH', TokenType.BEQUEATH],
      ['ATTEMPT', TokenType.ATTEMPT],
      ['SALVAGE', TokenType.SALVAGE],
      ['CONDEMN', TokenType.CONDEMN],
      ['AND', TokenType.AND],
      ['OR', TokenType.OR],
      ['NOT', TokenType.NOT],
    ];
    for (const [keyword, expected] of keywords) {
      const lexer = new Lexer(keyword);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${keyword}`);
    }
  });

  it('ALIVE boolean', () => {
    const lexer = new Lexer('ALIVE');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.ALIVE);
    assert.strictEqual(tokens[0].value, true);
  });

  it('DEAD boolean', () => {
    const lexer = new Lexer('DEAD');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.DEAD);
    assert.strictEqual(tokens[0].value, false);
  });

  it('VOID', () => {
    const lexer = new Lexer('VOID');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.VOID);
    assert.strictEqual(tokens[0].value, null);
  });
});

describe('Lexer Identifiers', () => {
  it('simple identifier', () => {
    const lexer = new Lexer('myVar');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.IDENTIFIER);
    assert.strictEqual(tokens[0].value, 'myVar');
  });

  it('identifier with underscore', () => {
    const lexer = new Lexer('_private');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.IDENTIFIER);
    assert.strictEqual(tokens[0].value, '_private');
  });

  it('identifier with numbers', () => {
    const lexer = new Lexer('timer2');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.IDENTIFIER);
    assert.strictEqual(tokens[0].value, 'timer2');
  });

  it('identifiers are case-sensitive', () => {
    const lexer = new Lexer('this THIS This');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.IDENTIFIER);  // lowercase 'this'
    assert.strictEqual(tokens[1].type, TokenType.THIS);  // uppercase 'THIS'
    assert.strictEqual(tokens[2].type, TokenType.IDENTIFIER);  // 'This'
  });
});

describe('Lexer Operators', () => {
  it('arithmetic operators', () => {
    const operators = [
      ['+', TokenType.PLUS],
      ['-', TokenType.MINUS],
      ['*', TokenType.STAR],
      ['/', TokenType.SLASH],
      ['%', TokenType.PERCENT],
    ];
    for (const [op, expected] of operators) {
      const lexer = new Lexer(op);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${op}`);
    }
  });

  it('comparison operators', () => {
    const operators = [
      ['==', TokenType.EQ],
      ['!=', TokenType.NE],
      ['<', TokenType.LT],
      ['>', TokenType.GT],
      ['<=', TokenType.LE],
      ['>=', TokenType.GE],
    ];
    for (const [op, expected] of operators) {
      const lexer = new Lexer(op);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${op}`);
    }
  });

  it('entity operators', () => {
    const operators = [
      ['&&', TokenType.AMPAMP],
      ['||', TokenType.PIPEPIPE],
      ['!', TokenType.BANG],
    ];
    for (const [op, expected] of operators) {
      const lexer = new Lexer(op);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${op}`);
    }
  });

  it('assignment operator', () => {
    const lexer = new Lexer('=');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].type, TokenType.ASSIGN);
  });
});

describe('Lexer Punctuation', () => {
  it('punctuation tokens', () => {
    const punctuation = [
      ['(', TokenType.LPAREN],
      [')', TokenType.RPAREN],
      ['{', TokenType.LBRACE],
      ['}', TokenType.RBRACE],
      ['[', TokenType.LBRACKET],
      [']', TokenType.RBRACKET],
      [';', TokenType.SEMICOLON],
      [',', TokenType.COMMA],
      ['.', TokenType.DOT],
      [':', TokenType.COLON],
    ];
    for (const [punct, expected] of punctuation) {
      const lexer = new Lexer(punct);
      const tokens = lexer.tokenize();
      assert.strictEqual(tokens[0].type, expected, `Failed for ${punct}`);
    }
  });
});

describe('Lexer Line and Column Tracking', () => {
  it('first token position', () => {
    const lexer = new Lexer('hello');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].line, 1);
    assert.strictEqual(tokens[0].column, 1);
  });

  it('second line position', () => {
    const lexer = new Lexer('hello\nworld');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[1].line, 2);
    assert.strictEqual(tokens[1].column, 1);
  });

  it('column tracking', () => {
    const lexer = new Lexer('a b c');
    const tokens = lexer.tokenize();
    assert.strictEqual(tokens[0].column, 1);
    assert.strictEqual(tokens[1].column, 3);
    assert.strictEqual(tokens[2].column, 5);
  });
});

describe('Lexer Complex Examples', () => {
  it('import statement', () => {
    const lexer = new Lexer('import timer T(100ms);');
    const tokens = lexer.tokenize();
    const types = tokens.slice(0, -1).map(t => t.type);  // Exclude EOF
    const expected = [
      TokenType.IMPORT, TokenType.TIMER, TokenType.IDENTIFIER,
      TokenType.LPAREN, TokenType.DURATION, TokenType.RPAREN,
      TokenType.SEMICOLON
    ];
    assert.deepStrictEqual(types, expected);
  });

  it('~ATH loop', () => {
    const lexer = new Lexer('~ATH(T) { } EXECUTE(VOID);');
    const tokens = lexer.tokenize();
    const types = tokens.slice(0, -1).map(t => t.type);
    const expected = [
      TokenType.TILDE_ATH, TokenType.LPAREN, TokenType.IDENTIFIER,
      TokenType.RPAREN, TokenType.LBRACE, TokenType.RBRACE,
      TokenType.EXECUTE, TokenType.LPAREN, TokenType.VOID,
      TokenType.RPAREN, TokenType.SEMICOLON
    ];
    assert.deepStrictEqual(types, expected);
  });

  it('bifurcate statement', () => {
    const lexer = new Lexer('bifurcate THIS[LEFT, RIGHT];');
    const tokens = lexer.tokenize();
    const types = tokens.slice(0, -1).map(t => t.type);
    const expected = [
      TokenType.BIFURCATE, TokenType.THIS, TokenType.LBRACKET,
      TokenType.IDENTIFIER, TokenType.COMMA, TokenType.IDENTIFIER,
      TokenType.RBRACKET, TokenType.SEMICOLON
    ];
    assert.deepStrictEqual(types, expected);
  });
});
