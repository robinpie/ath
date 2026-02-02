/**
 * AST node factories for the !~ATH language.
 */

// ============ Statements ============

export const createProgram = (statements = [], line = 0, column = 0) => ({
  type: 'Program',
  statements,
  line,
  column,
});

export const createImportStmt = (entityType, name, args = [], line = 0, column = 0) => ({
  type: 'ImportStmt',
  entityType,  // 'timer', 'process', 'connection', 'watcher'
  name,
  args,
  line,
  column,
});

export const createBifurcateStmt = (entity, branch1, branch2, line = 0, column = 0) => ({
  type: 'BifurcateStmt',
  entity,
  branch1,
  branch2,
  line,
  column,
});

export const createAthLoop = (entityExpr, body = [], execute = [], line = 0, column = 0) => ({
  type: 'AthLoop',
  entityExpr,
  body,
  execute,
  line,
  column,
});

export const createDieStmt = (target, line = 0, column = 0) => ({
  type: 'DieStmt',
  target,
  line,
  column,
});

export const createVarDecl = (name, value, line = 0, column = 0) => ({
  type: 'VarDecl',
  name,
  value,
  line,
  column,
});

export const createConstDecl = (name, value, line = 0, column = 0) => ({
  type: 'ConstDecl',
  name,
  value,
  line,
  column,
});

export const createAssignment = (target, value, line = 0, column = 0) => ({
  type: 'Assignment',
  target,
  value,
  line,
  column,
});

export const createRiteDef = (name, params = [], body = [], line = 0, column = 0) => ({
  type: 'RiteDef',
  name,
  params,
  body,
  line,
  column,
});

export const createConditional = (condition, thenBranch = [], elseBranch = null, line = 0, column = 0) => ({
  type: 'Conditional',
  condition,
  thenBranch,
  elseBranch,
  line,
  column,
});

export const createAttemptSalvage = (attemptBody = [], errorName, salvageBody = [], line = 0, column = 0) => ({
  type: 'AttemptSalvage',
  attemptBody,
  errorName,
  salvageBody,
  line,
  column,
});

export const createCondemnStmt = (message, line = 0, column = 0) => ({
  type: 'CondemnStmt',
  message,
  line,
  column,
});

export const createBequeathStmt = (value = null, line = 0, column = 0) => ({
  type: 'BequeathStmt',
  value,
  line,
  column,
});

export const createExprStmt = (expression, line = 0, column = 0) => ({
  type: 'ExprStmt',
  expression,
  line,
  column,
});

// ============ Entity Expressions ============

export const createEntityAnd = (left, right, line = 0, column = 0) => ({
  type: 'EntityAnd',
  left,
  right,
  line,
  column,
});

export const createEntityOr = (left, right, line = 0, column = 0) => ({
  type: 'EntityOr',
  left,
  right,
  line,
  column,
});

export const createEntityNot = (operand, line = 0, column = 0) => ({
  type: 'EntityNot',
  operand,
  line,
  column,
});

export const createEntityIdent = (name, line = 0, column = 0) => ({
  type: 'EntityIdent',
  name,
  line,
  column,
});

// ============ Die Targets ============

export const createDieIdent = (name, line = 0, column = 0) => ({
  type: 'DieIdent',
  name,
  line,
  column,
});

export const createDiePair = (left, right, line = 0, column = 0) => ({
  type: 'DiePair',
  left,
  right,
  line,
  column,
});

// ============ Expressions ============

export const createLiteral = (value, line = 0, column = 0) => ({
  type: 'Literal',
  value,
  line,
  column,
});

export const createIdentifier = (name, line = 0, column = 0) => ({
  type: 'Identifier',
  name,
  line,
  column,
});

export const createBinaryOp = (operator, left, right, line = 0, column = 0) => ({
  type: 'BinaryOp',
  operator,  // '+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=', 'AND', 'OR'
  left,
  right,
  line,
  column,
});

export const createUnaryOp = (operator, operand, line = 0, column = 0) => ({
  type: 'UnaryOp',
  operator,  // 'NOT', '-'
  operand,
  line,
  column,
});

export const createCallExpr = (callee, args = [], line = 0, column = 0) => ({
  type: 'CallExpr',
  callee,
  args,
  line,
  column,
});

export const createIndexExpr = (obj, index, line = 0, column = 0) => ({
  type: 'IndexExpr',
  obj,
  index,
  line,
  column,
});

export const createMemberExpr = (obj, member, line = 0, column = 0) => ({
  type: 'MemberExpr',
  obj,
  member,
  line,
  column,
});

export const createArrayLiteral = (elements = [], line = 0, column = 0) => ({
  type: 'ArrayLiteral',
  elements,
  line,
  column,
});

export const createMapLiteral = (entries = [], line = 0, column = 0) => ({
  type: 'MapLiteral',
  entries,  // Array of [key, value] pairs
  line,
  column,
});

export const createDuration = (unit, value, line = 0, column = 0) => ({
  type: 'Duration',
  unit,   // 'ms', 's', 'm', 'h'
  value,
  line,
  column,
});
