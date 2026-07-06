#ifndef clox_ast_h
#define clox_ast_h
#include <stdlib.h>

#include "token.h"
#include "value.h"

// ── Node kind tags ───────────────────────────────────────────────────────────

typedef enum {
  // Expressions
  NODE_LITERAL,   // number, string, true, false, nil
  NODE_UNARY,     // -expr  !expr
  NODE_BINARY,    // expr op expr
  NODE_GROUPING,  // ( expr )
  NODE_VARIABLE,  // identifier
  NODE_ASSIGN,    // name = expr
  NODE_AND,       // expr and expr
  NODE_OR,        // expr or  expr
  NODE_NULLISH,   // expr ?? expr
  NODE_CALL,      // callee(args...)
  NODE_GET,       // object.name
  NODE_SET,       // object.name = value
  NODE_THIS,      // this
  NODE_SUPER,     // super.method
  NODE_INDEX_GET, // object[index]
  NODE_INDEX_SET, // object[index] = value
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
  NODE_ARRAY,
  // Sentinel
  NODE_COUNT
} NodeKind;

typedef struct AstNode AstNode;

const char *print_ast(AstNode *ast);

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
  AstNode *value;
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
  AstNode *object;
  AstNode *index;
  Token bracket; // opening '[' — used for error location
} IndexGetNode;

typedef struct {
  AstNode *object;
  AstNode *index;
  AstNode *value;
  Token bracket;
} IndexSetNode;

typedef struct {
  Token name;
  AstNode *initializer; // NULL if `var x;` (no initializer)
  int declEndLine;      // line of the trailing ';' -- see compiler.c
} VarDeclNode;

typedef struct {
  AstNode **stmts;
  int count;
  AstNode *value;
  int endLine; // line of the closing '}' — see compiler.c's endScope() sites
} BlockNode;

typedef struct {
  AstNode *condition;
  AstNode *thenBranch;
  AstNode *elseBranch; // NULL if no else clause
} IfNode;

typedef struct {
  // init/increment are non-NULL only when this WhileNode came from
  // desugaring a `for` loop; both NULL for a genuine `while` statement.
  // Kept as distinct fields (rather than splicing the increment into body,
  // and the init into a wrapping block, which is how an earlier version of
  // this desugaring worked) so the compiler can reproduce the original
  // forStatement()'s exact structure -- its own beginScope()/endScope()
  // around the init variable, and the classic "jump over the increment on
  // the first pass, then loop back through it after the body" bytecode
  // pattern. See compileWhile() in compiler.c.
  AstNode *init;
  AstNode *condition;
  AstNode *body;
  AstNode *increment;
} WhileNode;

typedef struct {
  Token name;    // unused/empty when isLambda is true
  Token *params; // arena-allocated array of Token
  int arity;
  BlockNode body;    // used when exprBody == NULL
  AstNode *exprBody; // non-NULL for `fun name(...) = expr;` bodies
  int bodyEndLine;   // line of the body's closing '}' or trailing ';'
  bool isMethod;
  bool isLambda; // true for anonymous `fun (...) {...}` expressions
} FunctionNode;

typedef enum { CLASS_MEMBER_FIELD, CLASS_MEMBER_METHOD } ClassMemberKind;

// A single class-body member (`var x = expr;` or `name(params) {...}`),
// tagged by kind. Class bodies keep members in a single array in source
// order (rather than separate fields[]/methods[] arrays) so the compiler
// can emit OP_FIELD/OP_METHOD in exactly the order they were declared,
// matching what a single interleaved pass over the source would produce.
typedef struct {
  ClassMemberKind kind;
  union {
    FunctionNode *method;
    VarDeclNode field;
  } as;
} ClassMember;

typedef struct {
  Token name;
  Token *superclass;    // NULL if no superclass; pointer to Token in arena
  ClassMember *members; // arena-allocated array, in source order
  int memberCount;
  int endLine; // line of the closing '}' of the class body
} ClassNode;

// ArrayNodeData is a *temporary builder* used only while parsing an array
// literal (elements are collected one at a time as they're parsed, before
// the final count is known). It is heap-allocated via realloc and must be
// freed with arrayNodeDataFree() once its contents have been copied into
// the AST arena — it does not live inside AstNode itself. See ArrayNode
// below, which holds the arena-owned copy.
typedef struct {
  AstNode **data;
  int count;
  int capacity;
} ArrayNodeData;

typedef struct {
  AstNode **items; // arena-allocated array of element node pointers
  int count;
} ArrayNode;

void arrayNodeDataInit(ArrayNodeData *and);
void arrayNodeDataWrite(ArrayNodeData *and, AstNode *item);
void arrayNodeDataFree(ArrayNodeData *and);

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
    IndexGetNode indexGet;
    IndexSetNode indexSet;
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
    ArrayNode array;
  } as;
};

AstNode *astAlloc(NodeKind kind, int line);
void *astAllocRaw(size_t size); // for param arrays, arg arrays, etc.
void astFreeAll(void);

#endif
