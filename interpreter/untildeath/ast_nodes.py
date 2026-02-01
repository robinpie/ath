"""AST node definitions for the !~ATH language."""

from dataclasses import dataclass, field
from typing import List, Optional, Union, Any


# ============ Statements ============

@dataclass
class Program:
    statements: List['Statement'] = field(default_factory=list)
    line: int = 0
    column: int = 0


Statement = Union[
    'ImportStmt', 'BifurcateStmt', 'AthLoop', 'DieStmt',
    'VarDecl', 'ConstDecl', 'Assignment', 'RiteDef',
    'Conditional', 'AttemptSalvage', 'CondemnStmt', 'BequeathStmt',
    'ExprStmt'
]


@dataclass
class ImportStmt:
    entity_type: str  # 'timer', 'process', 'connection', 'watcher'
    name: str
    args: List['Expression'] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class BifurcateStmt:
    entity: str = ""  # The entity being bifurcated (e.g., 'THIS')
    branch1: str = ""  # First branch name
    branch2: str = ""  # Second branch name
    line: int = 0
    column: int = 0


@dataclass
class AthLoop:
    entity_expr: 'EntityExpr' = None
    body: List[Statement] = field(default_factory=list)
    execute: List[Statement] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class DieStmt:
    target: 'DieTarget' = None
    line: int = 0
    column: int = 0


@dataclass
class VarDecl:
    name: str = ""
    value: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class ConstDecl:
    name: str = ""
    value: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class Assignment:
    target: 'Expression' = None  # Can be Identifier or IndexExpr
    value: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class RiteDef:
    name: str = ""
    params: List[str] = field(default_factory=list)
    body: List[Statement] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class Conditional:
    condition: 'Expression' = None
    then_branch: List[Statement] = field(default_factory=list)
    else_branch: Optional[List[Statement]] = None
    line: int = 0
    column: int = 0


@dataclass
class AttemptSalvage:
    attempt_body: List[Statement] = field(default_factory=list)
    error_name: str = ""
    salvage_body: List[Statement] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class CondemnStmt:
    message: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class BequeathStmt:
    value: Optional['Expression'] = None
    line: int = 0
    column: int = 0


@dataclass
class ExprStmt:
    expression: 'Expression' = None
    line: int = 0
    column: int = 0


# ============ Entity Expressions ============

EntityExpr = Union['EntityAnd', 'EntityOr', 'EntityNot', 'EntityIdent']


@dataclass
class EntityAnd:
    left: 'EntityExpr' = None
    right: 'EntityExpr' = None
    line: int = 0
    column: int = 0


@dataclass
class EntityOr:
    left: 'EntityExpr' = None
    right: 'EntityExpr' = None
    line: int = 0
    column: int = 0


@dataclass
class EntityNot:
    operand: 'EntityExpr' = None
    line: int = 0
    column: int = 0


@dataclass
class EntityIdent:
    name: str = ""
    line: int = 0
    column: int = 0


# ============ Die Targets ============

DieTarget = Union['DieIdent', 'DiePair']


@dataclass
class DieIdent:
    name: str = ""
    line: int = 0
    column: int = 0


@dataclass
class DiePair:
    left: 'DieTarget' = None
    right: 'DieTarget' = None
    line: int = 0
    column: int = 0


# ============ Expressions ============

Expression = Union[
    'Literal', 'Identifier', 'BinaryOp', 'UnaryOp',
    'CallExpr', 'IndexExpr', 'MemberExpr',
    'ArrayLiteral', 'MapLiteral', 'Duration'
]


@dataclass
class Literal:
    value: Any = None  # int, float, str, bool, None
    line: int = 0
    column: int = 0


@dataclass
class Identifier:
    name: str = ""
    line: int = 0
    column: int = 0


@dataclass
class BinaryOp:
    operator: str = ""  # '+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=', 'AND', 'OR'
    left: 'Expression' = None
    right: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class UnaryOp:
    operator: str = ""  # 'NOT', '-'
    operand: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class CallExpr:
    callee: 'Expression' = None
    args: List['Expression'] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class IndexExpr:
    obj: 'Expression' = None
    index: 'Expression' = None
    line: int = 0
    column: int = 0


@dataclass
class MemberExpr:
    obj: 'Expression' = None
    member: str = ""
    line: int = 0
    column: int = 0


@dataclass
class ArrayLiteral:
    elements: List['Expression'] = field(default_factory=list)
    line: int = 0
    column: int = 0


@dataclass
class MapLiteral:
    entries: List[tuple] = field(default_factory=list)  # List of (key: str, value: Expression)
    line: int = 0
    column: int = 0


@dataclass
class Duration:
    unit: str = ""  # 'ms', 's', 'm', 'h'
    value: int = 0
    line: int = 0
    column: int = 0
