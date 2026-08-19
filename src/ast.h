#ifndef kirby_ast_h
#define kirby_ast_h
#include <stdbool.h>
#include <stdlib.h>

#include "strbuf.h"
#include "token.h"

typedef enum {
  NODE_LITERAL,
  NODE_UNARY,
  NODE_BINARY,
  NODE_GROUPING,
  NODE_VARIABLE,
  NODE_ASSIGN,
  NODE_AND,
  NODE_OR,
  NODE_NULLISH,
  NODE_CALL,
  NODE_GET,
  NODE_SET,
  NODE_SELF,
  NODE_INDEX_GET,
  NODE_INDEX_SET,
  NODE_EXPR_STMT,
  NODE_PRINT,
  NODE_VAR_DECL,
  NODE_BLOCK,
  NODE_IF,
  NODE_WHILE,
  NODE_FOR,
  NODE_RETURN,
  NODE_FUNCTION,
  NODE_STRUCT,
  NODE_STRUCT_INIT,
  NODE_IMPL,
  NODE_ARRAY,
  NODE_BREAK,
  NODE_CONTINUE,
  NODE_TYPE,
  NODE_TYPE_ALIAS,
  NODE_TYPE_FUNCTION,
  // Sentinel
  NODE_COUNT
} NodeKind;

typedef struct AstNode AstNode;

void print_ast(StrBuf *sb, AstNode *ast);

typedef enum {
  LITERAL_NIL,
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

typedef struct {
  Token op;
  AstNode *operand;
} UnaryNode;

typedef struct {
  Token op;
  AstNode *left;
  AstNode *right;
} BinaryNode;

typedef struct {
  AstNode *inner;
} GroupingNode;

typedef struct {
  Token name;
} VariableNode;

typedef struct {
  Token name;
  AstNode *value;
} AssignNode;

typedef struct {
  AstNode *left;
  AstNode *right;
} LogicalNode;

typedef struct {
  AstNode *expr;
} ExprStmtNode;

typedef struct {
  AstNode *expr;
} PrintNode;

typedef struct {
  AstNode *value;
} ReturnNode;

typedef struct {
  Token keyword;
} SelfNode;

typedef struct {
  AstNode *callee;
  /**
   * closing ')' — used for error location
   */
  Token paren;
  AstNode **args;
  int argCount;
} CallNode;

typedef struct {
  AstNode *object;
  Token name;
} GetNode;
typedef struct {
  AstNode *object;
  Token name;
  AstNode *value;
} SetNode;

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

typedef struct {
  AstNode **stmts;
  int count;
  AstNode *value;
  int endLine;
} BlockNode;

typedef struct {
  AstNode *condition;
  AstNode *thenBranch;
  /**
   * NULL if no else clause
   */
  AstNode *elseBranch;
} IfNode;

typedef struct {
  AstNode *condition;
  AstNode *body;
} WhileNode;

typedef struct {
  Token token;
} BreakNode;

typedef struct {
  Token token;
} ContinueNode;

typedef struct {
  AstNode *init;
  AstNode *condition;
  AstNode *body;
  AstNode *increment;
} ForNode;

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
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `fun sum[T](a: T, b: T): T`. NULL/0 for a non-generic function.
   * Never set for lambdas -- there's no name slot to attach `[T]` to.
   */
  Token *genericParams;
  int genericParamCount;
} FunctionNode;

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

typedef struct {
  Token name;
  AstNode *value;
} StructInitFieldNode;

typedef struct {
  Token name;
  StructInitFieldNode *fields;
  int fieldCount;
  int endLine;
} StructInitNode;

typedef struct {
  Token name;
  /**
   * arena-allocated array of bare generic parameter name tokens, e.g. the
   * `T` in `impl Box[T]`. Written independently of the struct's own
   * declared parameters for now (Phase 1 is grammar only -- nothing
   * cross-checks these match the struct's `genericParams` yet).
   */
  Token *genericParams;
  int genericParamCount;
  FunctionNode **methods;
  int methodCount;
  int endLine;
} ImplNode;

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

typedef struct {
  /**
   * arena-allocated array of element node pointers
   */
  AstNode **items;
  int count;
} ArrayNode;

void arrayNodeDataInit(ArrayNodeData *and);
void arrayNodeDataWrite(ArrayNodeData *and, AstNode *item);
void arrayNodeDataFree(ArrayNodeData *and);

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
    ArrayNode array;
    BreakNode break_;
    ContinueNode continue_;
    TypeNode type_;
    TypeAliasNode typeAlias;
    TypeFunctionNode typeFunction;
  } as;
};

AstNode *astAlloc(NodeKind kind, int line);
void *astAllocRaw(size_t size);
void astFreeAll(void);

#endif
