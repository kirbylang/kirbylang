#ifndef clox_ast_h
#define clox_ast_h
#include <stdlib.h>

#include "token.h"
#include "value.h"

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
  NODE_THIS,
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
  NODE_CLASS,
  NODE_ARRAY,
  NODE_BREAK,
  // Sentinel
  NODE_COUNT
} NodeKind;

typedef struct AstNode AstNode;

const char *print_ast(AstNode *ast);

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
  AstNode *initializer;
  int declEndLine;
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
  int arity;
  BlockNode body;    // used when exprBody == NULL;
  AstNode *exprBody; // non-NULL for `fun name(...) = expr;` bodies
  int bodyEndLine;
  bool isMethod;
  bool isLambda;
} FunctionNode;

typedef enum { CLASS_MEMBER_FIELD, CLASS_MEMBER_METHOD } ClassMemberKind;

typedef struct {
  ClassMemberKind kind;
  union {
    FunctionNode *method;
    VarDeclNode field;
  } as;
} ClassMember;

typedef struct {
  Token name;
  ClassMember *members;
  int memberCount;
  /**
   * line of the closing '}' of the class body
   */
  int endLine;
} ClassNode;

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
    ThisNode this_;
    ExprStmtNode exprStmt;
    PrintNode print;
    VarDeclNode varDecl;
    BlockNode block;
    IfNode if_;
    WhileNode while_;
    ForNode for_;
    ReturnNode return_;
    FunctionNode function;
    ClassNode class_;
    ArrayNode array;
    BreakNode break_;
  } as;
};

AstNode *astAlloc(NodeKind kind, int line);
void *astAllocRaw(size_t size);
void astFreeAll(void);

#endif
