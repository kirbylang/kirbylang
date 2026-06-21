#ifndef clox_ast_h
#define clox_ast_h
#include <stdlib.h>

#include "token.h"
#include "value.h"

// ── Node kind tags ───────────────────────────────────────────────────────────

typedef enum {
  // Expressions
  NODE_LITERAL,  // number, string, true, false, nil
  NODE_UNARY,    // -expr  !expr
  NODE_BINARY,   // expr op expr
  NODE_GROUPING, // ( expr )
  NODE_VARIABLE, // identifier
  NODE_ASSIGN,   // name = expr
  NODE_AND,      // expr and expr
  NODE_OR,       // expr or  expr
  NODE_CALL,     // callee(args...)
  NODE_GET,      // object.name
  NODE_SET,      // object.name = value
  NODE_THIS,     // this
  NODE_SUPER,    // super.method
  // Statements
  NODE_EXPR_STMT, // expression ;
  NODE_PRINT,     // print expr ;
  NODE_VAR_DECL,  // var name = init ;
  NODE_BLOCK,     // { statements }
  NODE_IF,        // if (cond) then else?
  NODE_WHILE,     // while (cond) body
  NODE_RETURN,    // return expr? ;
  NODE_FUNCTION,  // fun name(params) body
  NODE_CLASS,     // class Name (< Super)? { methods }
  // Sentinel
  NODE_COUNT
} NodeKind;

typedef struct AstNode AstNode;

// ── Per-kind payload structs ─────────────────────────────────────────────────

typedef struct {
  Value value;
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
  AstNode *value; /* NULL = bare return */
} ReturnNode;
typedef struct {
  Token keyword;
} ThisNode;
typedef struct {
  Token keyword;
  Token method;
} SuperNode;

typedef struct {
  AstNode *callee;
  Token paren; // closing ')' — used for error location
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
  Token name;
  AstNode *initializer; // NULL if `var x;` (no initializer)
} VarDeclNode;

typedef struct {
  AstNode **stmts;
  int count;
} BlockNode;

typedef struct {
  AstNode *condition;
  AstNode *thenBranch;
  AstNode *elseBranch; // NULL if no else clause
} IfNode;

typedef struct {
  AstNode *condition;
  AstNode *body;
} WhileNode;

typedef struct {
  Token name;
  Token *params; // arena-allocated array of Token
  int arity;
  BlockNode body;
  bool isMethod;
} FunctionNode;

typedef struct {
  Token name;
  Token *superclass; // NULL if no superclass; pointer to Token in arena
  FunctionNode **methods;
  int methodCount;
} ClassNode;

// ── The tagged-union node ────────────────────────────────────────────────────

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
    ThisNode this_;
    SuperNode super_;
    ExprStmtNode exprStmt;
    PrintNode print;
    VarDeclNode varDecl;
    BlockNode block;
    IfNode if_;
    WhileNode while_;
    ReturnNode return_;
    FunctionNode function;
    ClassNode class_;
  } as;
};

AstNode *astAlloc(NodeKind kind, int line);
void *astAllocRaw(size_t size); // for param arrays, arg arrays, etc.
void astFreeAll(void);

#endif
