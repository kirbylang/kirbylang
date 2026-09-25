---
aliases:
  - Parser
---

## Parser

Files: `src/parser.h`, `src/parser.c`

```c
// src/parser.h

/**
 * Parses `source` into a flat array of AST nodes.
 *
 * The caller must call `astFreeAll()` to free AST arena memory after use.
 *
 * @param source The source code to parse
 * @param outCount The number of top-level declarations parsed.
 * @param hadError If any parse error was reported.
 * @param outEndLine The line of the EOF token
 */
AstNode **parse(const char *source, int *outCount, bool *hadError,
                int *outEndLine);

```

## AST

Files: `src/ast.h`, `src/ast.c`

```c
void astFreeAll(void);
```

### AstNode

```c
struct AstNode {
	NodeKind kind;
	int line;
	union {
		LiteralNode literal;
		UnaryNode unary;
		BinaryNode binary;
		GroupingNode grouping;
		VariableNode variable;
		AssignNode assign;
		LogicalNode logical;
		CallNode call;
		GetNode get;
		SetNode set;
		IndexGetNode indexGet;
		IndexSetNode indexSet;
		SelfNode self_;
		ExprStmtNode exprStmt;
		PrintNode print;
		VarDeclNode varDecl;
		BlockNode block;
		IfNode if_;
		WhileNode while_;
		ForNode for_;
		ReturnNode return_;
		FunctionNode function;
		StructNode struct_;
		StructInitNode structInit;
		ImplNode impl;
		TraitNode trait_;
		ArrayNode array;
		BreakNode break_;
		ContinueNode continue_;
		TypeNode type_;
		TypeAliasNode typeAlias;
		TypeFunctionNode typeFunction;
		InterpStringNode interpString;
	} as;
};
```

### Literals

```c
typedef enum {
  LITERAL_NIL,
  LITERAL_UNIT,
  LITERAL_BOOL,
  LITERAL_NUMBER,
  LITERAL_STRING
} LiteralKind;

typedef struct {
  LiteralKind kind;
  union {
    bool boolean;
    double number;
    struct {
      const char *chars;
      int length;
    } string;
  } as;
} LiteralNode;
```

### Operators

```c
typedef struct {
Token op;
AstNode *operand;
} UnaryNode;

typedef struct {
Token op;
AstNode *left;
AstNode *right;
/**
 * `+` on two strings. Set by the type checker.
 */
bool isStringConcat;
} BinaryNode;
```

### Grouping Expression

```c
typedef struct {
AstNode *inner;
} GroupingNode;
```

### Identifiers/Bindings

```c
typedef struct {
Token name;
} VariableNode;
```

### Assignment

```c
typedef struct {
Token name;
AstNode *value;
} AssignNode;
```

### And/Or

```c
typedef struct {
AstNode *left;
AstNode *right;
} LogicalNode;
```

### Expression Statements

Expressions followed by a semicolon.

```c
typedef struct {
AstNode *expr;
} ExprStmtNode;
```

### Print Statement

```c
typedef struct {
AstNode *expr;
} PrintNode;
```

### Calling Functions

```c
typedef struct {
  AstNode *callee;
  /**
   * closing ')' — used for error location
   */
  Token paren;
  AstNode **args;
  int argCount;
} CallNode;
```

### Getting/Setting Instance Fields

```c
typedef struct {
  AstNode *object;
  Token name;
} GetNode;
```

```c
typedef struct {
  AstNode *object;
  Token name;
  AstNode *value;
} SetNode;
```

### Index Accessing

```c
typedef struct {
  AstNode *object;
  AstNode *index;
  /**
   * opening '[' — used for error location
   */
  Token bracket;
} IndexGetNode;

typedef struct {
  AstNode *object;
  AstNode *index;
  AstNode *value;
  Token bracket;
} IndexSetNode;
```

### Types

```c
typedef struct {
  Token name;
  /**
   * NODE_TYPE nodes
   */
  AstNode **genericArgs;
  int genericArgCount;
} TypeNode;

typedef struct {
  Token name;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `type Wrapper[T] = T;`. Distinct from TypeNode.genericArgs --
   * these are parameter *declarations* (bare names), not type arguments.
   */
  Token *genericParams;
  int genericParamCount;
  /**
   * The type expression on the right of `=`.
   */
  AstNode *target;
} TypeAliasNode;
```

```c
/**
 * A function-type expression, e.g. `fun (i64, i64) => i64`. Distinct from
 * TypeNode -- a function type has no name to hang nominal identity on, it's
 * compared structurally (params + return) instead.
 */
typedef struct {
  /**
   * arena-allocated array of NODE_TYPE(-like) nodes
   */
  AstNode **paramTypes;
  int paramCount;
  /**
   * Never NULL -- the return type is mandatory on a function type, same as
   * on a real function declaration.
   */
  AstNode *returnType;
} TypeFunctionNode;
```

### Let/Var Bindings

```c
typedef struct {
  Token name;
  AstNode *initializer;
  /**
   * Type annotation, if present.
   *
   * NULL if not present.
   */
  AstNode *declaredType;
  int declEndLine;
  bool isMutable;
  bool isPublic;
} VarDeclNode;
```

### Block Statement

```c
typedef struct {
  AstNode **stmts;
  int count;
  AstNode *value;
  int endLine;
} BlockNode;
```

### Flow Control

```c
typedef struct {
  AstNode *condition;
  AstNode *thenBranch;
  /**
   * NULL if no else clause
   */
  AstNode *elseBranch;
} IfNode;
```

```c
typedef struct {
  AstNode *init;
  AstNode *condition;
  AstNode *body;
  AstNode *increment;
} ForNode;
```

```c
typedef struct {
  AstNode *condition;
  AstNode *body;
} WhileNode;
```

```c
typedef struct {
  Token token;
} BreakNode;
```

```c
typedef struct {
  Token token;
} ContinueNode;
```

```c
typedef struct {
AstNode *value;
} ReturnNode;
```

### Functions

```c
typedef struct {
  Token name;
  /**
   * arena-allocated array of Token
   */
  Token *params;
  /**
   * Function parameter types, if present.
   *
   * NULL if not present on param.
   */
  AstNode **paramTypes;
  int arity;
  BlockNode body;    // used when exprBody == NULL;
  AstNode *exprBody; // non-NULL for `fun name(...) = expr;` bodies
  /**
   * Function return type, if present.
   *
   * NULL if not present.
   */
  AstNode *returnType;
  int bodyEndLine;
  bool isMethod;
  bool hasSelf;
  bool isLambda;
  bool isPublic;
  // Is this a fun declaration in a trait? e.g. no body
  bool isTraitSignature;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `fun sum[T](a: T, b: T): T`. NULL/0 for a non-generic function.
   * Never set for lambdas -- there's no name slot to attach `[T]` to.
   */
  Token *genericParams;
  int genericParamCount;
} FunctionNode;
```

### Structs

```c
typedef struct {
  Token name;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `struct Box[T]`. NULL/0 for a non-generic struct.
   */
  Token *genericParams;
  int genericParamCount;
  VarDeclNode *fields;
  int fieldCount;
  int endLine;
} StructNode;
```

```c
typedef struct {
  Token name;
  AstNode *value;
} StructInitFieldNode;
```

```c
typedef struct {
  Token name;
  StructInitFieldNode *fields;
  int fieldCount;
  int endLine;
} StructInitNode;
```

### Traits

```c
typedef struct {
  Token targetName;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `impl Box[T]`. Written independently of the struct's own
   * declared parameters for now (Phase 1 is grammar only -- nothing
   * cross-checks these match the struct's `genericParams` yet).
   */
  Token *genericParams;
  int genericParamCount;
  // impl Trait for Struct?
  bool hasTraitName;
  Token traitName;
  FunctionNode **methods;
  int methodCount;
  int endLine;
} ImplNode;
```

```c
typedef struct {
  Token name;
  bool hasSupertrait;
  Token supertrait;
  /**
   * arena-allocated array of FunctionNode pointers, each with
   * isTraitSignature == true -- required methods with no body.
   */
  FunctionNode **methods;
  int methodCount;
  int endLine;
  bool isBuiltin;
} TraitNode;
```

```c
typedef struct {
Token keyword;
} SelfNode;
```

### Arrays

```c
/**
 * Called in the parser's arrayLiteral function
 *
 * It is heap-allocated via realloc and must be
 * freed with arrayNodeDataFree() once its contents have been copied into
 * the AST arena — it does not live inside AstNode itself. See ArrayNode
 * below, which holds the arena-owned copy.
 */
typedef struct {
  AstNode **data;
  int count;
  int capacity;
} ArrayNodeData;
```

```c
typedef struct {
  /**
   * arena-allocated array of element node pointers
   */
  AstNode **items;
  int count;
} ArrayNode;
```

### Interpolated Strings

`$"Hello {name}!"`. The type checker sets each part's `conversion`, and the
compiler joins the parts with `@arrJoin`.

```c
typedef enum {
  STRING_CONVERSION_NONE,   // already a string
  STRING_CONVERSION_NUMBER, // @numberToString
  STRING_CONVERSION_BOOL,   // @boolToString
} StringConversion;

typedef struct {
  AstNode *expr;
  StringConversion conversion;
} InterpPart;

typedef struct {
  InterpPart *parts;
  int count;
} InterpStringNode;
```

```c
void arrayNodeDataInit(ArrayNodeData *and);
void arrayNodeDataWrite(ArrayNodeData *and, AstNode *item);
void arrayNodeDataFree(ArrayNodeData *and);
```

### NodeKind

| Idx | Token                |
| --: | :------------------- |
|   0 | `NODE_LITERAL`       |
|   1 | `NODE_UNARY`         |
|   2 | `NODE_BINARY`        |
|   3 | `NODE_GROUPING`      |
|   4 | `NODE_VARIABLE`      |
|   5 | `NODE_ASSIGN`        |
|   6 | `NODE_AND`           |
|   7 | `NODE_OR`            |
|   8 | `NODE_NULLISH`       |
|   9 | `NODE_CALL`          |
|  10 | `NODE_GET`           |
|  11 | `NODE_SET`           |
|  12 | `NODE_SELF`          |
|  13 | `NODE_INDEX_GET`     |
|  14 | `NODE_INDEX_SET`     |
|  15 | `NODE_EXPR_STMT`     |
|  16 | `NODE_PRINT`         |
|  17 | `NODE_VAR_DECL`      |
|  18 | `NODE_BLOCK`         |
|  19 | `NODE_IF`            |
|  20 | `NODE_WHILE`         |
|  21 | `NODE_FOR`           |
|  22 | `NODE_RETURN`        |
|  23 | `NODE_FUNCTION`      |
|  24 | `NODE_STRUCT`        |
|  25 | `NODE_STRUCT_INIT`   |
|  26 | `NODE_IMPL`          |
|  27 | `NODE_TRAIT`         |
|  28 | `NODE_ARRAY`         |
|  29 | `NODE_BREAK`         |
|  30 | `NODE_CONTINUE`      |
|  31 | `NODE_TYPE`          |
|  32 | `NODE_TYPE_ALIAS`    |
|  33 | `NODE_TYPE_FUNCTION` |
|  34 | `NODE_INTERP_STRING` |
