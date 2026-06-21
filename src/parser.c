#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "lexer.h"
#include "object.h"
#include "token.h"
#include "token_stream.h"

typedef struct {
  TokenStream *tokens;
  Token previous;
  Token current;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or || ??
  PREC_AND,        // and &&
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

static AstNode *expression(Parser *p);
static AstNode *statement(Parser *p);
static AstNode *declaration(Parser *p);
static AstNode *parse_precedence(Parser *p, Precedence prec);
static void parse_error_at(Parser *p, Token *token, const char *message);
static void error_at_current(Parser *p, const char *message);
static void parse_error(Parser *p, const char *message);

typedef AstNode *(*PrefixFn)(Parser *p, bool canAssign);
typedef AstNode *(*InfixFn)(Parser *p, AstNode *left, bool canAssign);

typedef struct {
  PrefixFn prefix;
  InfixFn infix;
  Precedence precedence;
} ParseRule;

// Token Functions

static void advance(Parser *parser) {
  parser->previous = parser->current;

  for (;;) {
    parser->current = tsAdvance(parser->tokens);

    if (parser->current.type != TOKEN_ERROR)
      break;
    error_at_current(parser, parser->current.start);
  }
}

static bool check(Parser *parser, TokenType type) {
  return parser->current.type == type;
}

static bool match(Parser *parser, TokenType type) {
  if (!check(parser, type))
    return false;

  advance(parser);

  return true;
}

static void consume(Parser *parser, TokenType type, const char *message) {
  if (parser->current.type == type) {
    advance(parser);
    return;
  }

  error_at_current(parser, message);
}

static bool is_at_end(Parser *parser) {
  return parser->current.type == TOKEN_EOF;
}

// Discards tokens until a statement boundary is found (panic-mode recovery).
static void synchronize(Parser *p) {
  p->panicMode = false;
  while (!is_at_end(p)) {
    if (p->previous.type == TOKEN_SEMICOLON)
      return;
    switch (p->current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:;
    }
    advance(p);
  }
}

// Parsing Functions

static ParseRule *get_rule(TokenType type);
typedef AstNode *(*ParseFn)(Parser *parser, bool canAssign);

static AstNode *parse_precedence(Parser *parser, Precedence precedence) {
  TRACELN("parser.parse_precedence(%d)", precedence);

  advance(parser);

  ParseFn prefixRule = get_rule(parser->previous.type)->prefix;
  if (prefixRule == NULL) {
    parse_error(parser, "Expect expression.");
    return NULL;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;

  prefixRule(parser, canAssign);

  while (precedence <= get_rule(parser->current.type)->precedence) {
    advance(parser);
    ParseFn infixRule = get_rule(parser->previous.type)->infix;
    infixRule(parser, canAssign);
  }

  if (canAssign && match(parser, TOKEN_EQUAL)) {
    parse_error(parser, "Invalid assignment target.");
  }

  TRACELN("parser.parse_precedence() end");
}

// Parsing Rule Functions

static AstNode *expression(Parser *parser) {
  TRACELN("parser.expression()");

  return parse_precedence(parser, PREC_ASSIGNMENT);
}

static AstNode *number(Parser *parser, bool canAssign) {
  (void)canAssign;
  double v = strtod(parser->previous.start, NULL);
  AstNode *node = astAlloc(NODE_LITERAL, parser->previous.line);
  node->as.literal.value = NUMBER_VAL(v);
  return node;
}

static AstNode *string_(Parser *parser, bool canAssign) {
  (void)canAssign;
  // Strip the surrounding quotes and intern the string.
  AstNode *node = astAlloc(NODE_LITERAL, parser->previous.line);
  node->as.literal.value = OBJ_VAL(
      copyString(parser->previous.start + 1, parser->previous.length - 2));
  return node;
}

static AstNode *literal(Parser *parser, bool canAssign) {
  (void)canAssign;
  AstNode *node = astAlloc(NODE_LITERAL, parser->previous.line);
  switch (parser->previous.type) {
  case TOKEN_TRUE:
    node->as.literal.value = BOOL_VAL(true);
    break;
  case TOKEN_FALSE:
    node->as.literal.value = BOOL_VAL(false);
    break;
  case TOKEN_NIL:
    node->as.literal.value = NIL_VAL;
    break;
  default:
    break; // unreachable
  }
  return node;
}

static AstNode *grouping(Parser *p, bool canAssign) {
  (void)canAssign;
  int line = p->previous.line;
  AstNode *inner = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
  AstNode *node = astAlloc(NODE_GROUPING, line);
  node->as.grouping.inner = inner;
  return node;
}

static AstNode *unary(Parser *p, bool canAssign) {
  (void)canAssign;
  Token op = p->previous;
  AstNode *operand = parse_precedence(p, PREC_UNARY);
  AstNode *node = astAlloc(NODE_UNARY, op.line);
  node->as.unary.op = op;
  node->as.unary.operand = operand;
  return node;
}

static AstNode *variable(Parser *p, bool canAssign) {
  Token name = p->previous;

  // If followed by '=' and we're in an assignable position, parse assignment.
  if (canAssign && match(p, TOKEN_EQUAL)) {
    AstNode *value = expression(p);
    AstNode *node = astAlloc(NODE_ASSIGN, name.line);
    node->as.assign.name = name;
    node->as.assign.value = value;
    return node;
  }

  AstNode *node = astAlloc(NODE_VARIABLE, name.line);
  node->as.variable.name = name;
  return node;
}

static AstNode *this_(Parser *p, bool canAssign) {
  (void)canAssign;
  AstNode *node = astAlloc(NODE_THIS, p->previous.line);
  node->as.this_.keyword = p->previous;
  return node;
}

static AstNode *super_(Parser *p, bool canAssign) {
  (void)canAssign;
  Token keyword = p->previous;
  consume(p, TOKEN_DOT, "Expect '.' after 'super'.");
  consume(p, TOKEN_IDENTIFIER, "Expect superclass method name.");
  Token method = p->previous;
  AstNode *node = astAlloc(NODE_SUPER, keyword.line);
  node->as.super_.keyword = keyword;
  node->as.super_.method = method;
  return node;
}

// ── Infix rules
// ───────────────────────────────────────────────────────────────

static AstNode *binary(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;
  Token op = p->previous;
  ParseRule *rule = getRule(op.type);
  AstNode *right = parsePrecedence(p, (Precedence)(rule->precedence + 1));
  AstNode *node = astAlloc(NODE_BINARY, op.line);
  node->as.binary.op = op;
  node->as.binary.left = left;
  node->as.binary.right = right;
  return node;
}

static AstNode *and_(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;
  AstNode *right = parsePrecedence(p, PREC_AND);
  AstNode *node = astAlloc(NODE_AND, p->previous.line);
  node->as.logical.left = left;
  node->as.logical.right = right;
  return node;
}

static AstNode *or_(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;
  AstNode *right = parsePrecedence(p, PREC_OR);
  AstNode *node = astAlloc(NODE_OR, p->previous.line);
  node->as.logical.left = left;
  node->as.logical.right = right;
  return node;
}

static AstNode *call(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;
  int line = p->previous.line;

  // Parse argument list.
  // We store args in a temporary stack-local array then copy into the arena.
  AstNode *argBuf[256];
  int argCount = 0;

  if (!check(p, TOKEN_RIGHT_PAREN)) {
    do {
      if (argCount >= 255) {
        errorAtCurrent(p, "Can't have more than 255 arguments.");
      }
      argBuf[argCount++] = expression(p);
    } while (match(p, TOKEN_COMMA));
  }
  Token paren = p->current;
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

  // Copy arg pointers into arena.
  AstNode **args = NULL;
  if (argCount > 0) {
    args = (AstNode **)astAllocRaw(argCount * sizeof(AstNode *));
    memcpy(args, argBuf, argCount * sizeof(AstNode *));
  }

  AstNode *node = astAlloc(NODE_CALL, line);
  node->as.call.callee = left;
  node->as.call.paren = paren;
  node->as.call.args = args;
  node->as.call.argCount = argCount;
  return node;
}

static AstNode *dot(Parser *p, AstNode *left, bool canAssign) {
  consume(p, TOKEN_IDENTIFIER, "Expect property name after '.'.");
  Token name = p->previous;

  if (canAssign && match(p, TOKEN_EQUAL)) {
    AstNode *value = expression(p);
    AstNode *node = astAlloc(NODE_SET, name.line);
    node->as.set.object = left;
    node->as.set.name = name;
    node->as.set.value = value;
    return node;
  }

  AstNode *node = astAlloc(NODE_GET, name.line);
  node->as.get.object = left;
  node->as.get.name = name;
  return node;
}

// ── Pratt dispatch table
// ──────────────────────────────────────────────────────

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string_, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {super_, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

// ── Statement parsers
// ─────────────────────────────────────────────────────────

// Parses a block body `{ stmts* }` — the opening brace must already be
// consumed. Returns a heap-allocated BlockNode; stmts array lives in the arena.
static BlockNode parseBlock(Parser *p) {
  // Collect into a local growable array, then copy into arena once done.
  int capacity = 8, count = 0;
  AstNode **buf = (AstNode **)malloc(capacity * sizeof(AstNode *));

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    if (count >= capacity) {
      capacity *= 2;
      buf = (AstNode **)realloc(buf, capacity * sizeof(AstNode *));
    }
    buf[count++] = declaration(p);
  }
  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");

  // Copy into arena so the array outlives this stack frame.
  AstNode **stmts = NULL;
  if (count > 0) {
    stmts = (AstNode **)astAllocRaw(count * sizeof(AstNode *));
    memcpy(stmts, buf, count * sizeof(AstNode *));
  }
  free(buf);

  BlockNode block;
  block.stmts = stmts;
  block.count = count;
  return block;
}

static AstNode *blockStatement(Parser *p) {
  int line = p->previous.line;
  BlockNode block = parseBlock(p);
  AstNode *node = astAlloc(NODE_BLOCK, line);
  node->as.block = block;
  return node;
}

static AstNode *printStatement(Parser *p) {
  int line = p->previous.line;
  AstNode *expr = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after value.");
  AstNode *node = astAlloc(NODE_PRINT, line);
  node->as.print.expr = expr;
  return node;
}

static AstNode *expressionStatement(Parser *p) {
  int line = p->current.line;
  AstNode *expr = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after expression.");
  AstNode *node = astAlloc(NODE_EXPR_STMT, line);
  node->as.exprStmt.expr = expr;
  return node;
}

static AstNode *ifStatement(Parser *p) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  AstNode *cond = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AstNode *thenBranch = statement(p);
  AstNode *elseBranch = NULL;
  if (match(p, TOKEN_ELSE))
    elseBranch = statement(p);

  AstNode *node = astAlloc(NODE_IF, line);
  node->as.if_.condition = cond;
  node->as.if_.thenBranch = thenBranch;
  node->as.if_.elseBranch = elseBranch;
  return node;
}

static AstNode *whileStatement(Parser *p) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  AstNode *cond = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AstNode *body = statement(p);

  AstNode *node = astAlloc(NODE_WHILE, line);
  node->as.while_.condition = cond;
  node->as.while_.body = body;
  return node;
}

// A `for` loop is desugared into a while loop at parse time.
//   for (init; cond; incr) body
// becomes:
//   { init; while (cond) { body; incr; } }
static AstNode *forStatement(Parser *p) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  // -- Initialiser --
  AstNode *init = NULL;
  if (match(p, TOKEN_SEMICOLON)) {
    // no initialiser
  } else if (match(p, TOKEN_VAR)) {
    // reuse varDecl parsing — implemented below; forward call via declaration()
    // We need only the var-decl node, so handle it inline:
    consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
    Token name = p->previous;
    AstNode *initializer = NULL;
    if (match(p, TOKEN_EQUAL))
      initializer = expression(p);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    AstNode *vd = astAlloc(NODE_VAR_DECL, name.line);
    vd->as.varDecl.name = name;
    vd->as.varDecl.initializer = initializer;
    init = vd;
  } else {
    init = expressionStatement(p);
  }

  // -- Condition --
  AstNode *cond = NULL;
  if (!check(p, TOKEN_SEMICOLON))
    cond = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

  // -- Increment --
  AstNode *incr = NULL;
  if (!check(p, TOKEN_RIGHT_PAREN))
    incr = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

  // -- Body --
  AstNode *body = statement(p);

  // Attach increment at end of body (wrap in a block).
  if (incr != NULL) {
    AstNode **stmts = (AstNode **)astAllocRaw(2 * sizeof(AstNode *));
    stmts[0] = body;
    AstNode *incrStmt = astAlloc(NODE_EXPR_STMT, incr->line);
    incrStmt->as.exprStmt.expr = incr;
    stmts[1] = incrStmt;
    AstNode *bodyBlock = astAlloc(NODE_BLOCK, line);
    bodyBlock->as.block.stmts = stmts;
    bodyBlock->as.block.count = 2;
    body = bodyBlock;
  }

  // If no condition, synthesise `true`.
  if (cond == NULL) {
    cond = astAlloc(NODE_LITERAL, line);
    cond->as.literal.value = BOOL_VAL(true);
  }

  // Build while node.
  AstNode *loop = astAlloc(NODE_WHILE, line);
  loop->as.while_.condition = cond;
  loop->as.while_.body = body;

  // Wrap in a block if there's an initialiser.
  if (init != NULL) {
    AstNode **stmts = (AstNode **)astAllocRaw(2 * sizeof(AstNode *));
    stmts[0] = init;
    stmts[1] = loop;
    AstNode *outerBlock = astAlloc(NODE_BLOCK, line);
    outerBlock->as.block.stmts = stmts;
    outerBlock->as.block.count = 2;
    return outerBlock;
  }

  return loop;
}

static AstNode *returnStatement(Parser *p) {
  int line = p->previous.line;
  Token keyword = p->previous;
  AstNode *value = NULL;
  if (!check(p, TOKEN_SEMICOLON))
    value = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after return value.");
  AstNode *node = astAlloc(NODE_RETURN, line);
  node->as.return_.value = value;
  (void)keyword;
  return node;
}

static AstNode *statement(Parser *p) {
  if (match(p, TOKEN_PRINT))
    return printStatement(p);
  if (match(p, TOKEN_IF))
    return ifStatement(p);
  if (match(p, TOKEN_WHILE))
    return whileStatement(p);
  if (match(p, TOKEN_FOR))
    return forStatement(p);
  if (match(p, TOKEN_RETURN))
    return returnStatement(p);
  if (match(p, TOKEN_LEFT_BRACE))
    return blockStatement(p);
  return expressionStatement(p);
}

// ── Declaration parsers
// ───────────────────────────────────────────────────────

static AstNode *varDeclaration(Parser *p) {
  consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
  Token name = p->previous;

  AstNode *initializer = NULL;
  if (match(p, TOKEN_EQUAL))
    initializer = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  AstNode *node = astAlloc(NODE_VAR_DECL, name.line);
  node->as.varDecl.name = name;
  node->as.varDecl.initializer = initializer;
  return node;
}

// Parses `fun` or a method inside a class.
// `isMethod` controls whether a `fun` keyword is expected before the name.
static AstNode *functionDeclaration(Parser *p, bool isMethod) {
  if (!isMethod)
    consume(p, TOKEN_IDENTIFIER, "Expect function name.");
  Token name = p->previous;
  int line = name.line;

  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  // Collect parameter tokens into a temporary buffer.
  Token paramBuf[256];
  int arity = 0;
  if (!check(p, TOKEN_RIGHT_PAREN)) {
    do {
      if (arity >= 255) {
        errorAtCurrent(p, "Can't have more than 255 parameters.");
      }
      consume(p, TOKEN_IDENTIFIER, "Expect parameter name.");
      paramBuf[arity++] = p->previous;
    } while (match(p, TOKEN_COMMA));
  }
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(p, TOKEN_LEFT_BRACE, "Expect '{' before function body.");

  // Copy params into arena.
  Token *params = NULL;
  if (arity > 0) {
    params = (Token *)astAllocRaw(arity * sizeof(Token));
    memcpy(params, paramBuf, arity * sizeof(Token));
  }

  BlockNode body = parseBlock(p);

  AstNode *node = astAlloc(NODE_FUNCTION, line);
  node->as.function.name = name;
  node->as.function.params = params;
  node->as.function.arity = arity;
  node->as.function.body = body;
  node->as.function.isMethod = isMethod;
  return node;
}

static AstNode *classDeclaration(Parser *p) {
  consume(p, TOKEN_IDENTIFIER, "Expect class name.");
  Token name = p->previous;
  int line = name.line;

  Token *superclass = NULL;
  if (match(p, TOKEN_LESS)) {
    consume(p, TOKEN_IDENTIFIER, "Expect superclass name.");
    superclass = (Token *)astAllocRaw(sizeof(Token));
    *superclass = p->previous;
  }

  consume(p, TOKEN_LEFT_BRACE, "Expect '{' before class body.");

  // Collect methods.
  int methodCap = 8, methodCount = 0;
  FunctionNode **methodBuf =
      (FunctionNode **)malloc(methodCap * sizeof(FunctionNode *));

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    if (methodCount >= methodCap) {
      methodCap *= 2;
      methodBuf = (FunctionNode **)realloc(methodBuf,
                                           methodCap * sizeof(FunctionNode *));
    }
    // Each method is a function without a leading `fun` keyword.
    consume(p, TOKEN_IDENTIFIER, "Expect method name.");
    AstNode *method = functionDeclaration(p, /*isMethod=*/true);
    methodBuf[methodCount++] = &method->as.function;
  }
  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

  // Copy method array into arena.
  FunctionNode **methods = NULL;
  if (methodCount > 0) {
    methods =
        (FunctionNode **)astAllocRaw(methodCount * sizeof(FunctionNode *));
    memcpy(methods, methodBuf, methodCount * sizeof(FunctionNode *));
  }
  free(methodBuf);

  AstNode *node = astAlloc(NODE_CLASS, line);
  node->as.class_.name = name;
  node->as.class_.superclass = superclass;
  node->as.class_.methods = methods;
  node->as.class_.methodCount = methodCount;
  return node;
}

static AstNode *declaration(Parser *p) {
  AstNode *node = NULL;

  if (match(p, TOKEN_CLASS)) {
    node = classDeclaration(p);
  } else if (match(p, TOKEN_FUN)) {
    node = functionDeclaration(p, /*isMethod=*/false);
  } else if (match(p, TOKEN_VAR)) {
    node = varDeclaration(p);
  } else {
    node = statement(p);
  }

  if (p->panicMode)
    synchronize(p);
  return node;
}

static void initParser(Parser *parser, TokenStream *tokens) {
  parser->tokens = tokens;
  parser->hadError = false;
  parser->panicMode = false;

  advance(parser);
}

// ── Entry point
// ───────────────────────────────────────────────────────────────

AstNode **parse(const char *source, int *outCount, bool *hadError) {
  TokenStream tokens = lex(source);

  Parser parser;
  initParser(&parser, &tokens);

  int capacity = 8;
  int count = 0;

  AstNode **ast = (AstNode **)malloc(capacity * sizeof(AstNode *));

  while (!is_at_end(&parser)) {
    if (count >= capacity) {
      capacity *= 2;

      ast = (AstNode **)realloc(ast, capacity * sizeof(AstNode *));
    }

    ast[count++] = declaration(&parser);
  }

  tsFree(&tokens);

  *outCount = count;
  *hadError = parser.hadError;

  return ast;
}

// Error Reporting Functions

static void parse_error_at(Parser *p, Token *token, const char *message) {
  if (p->panicMode)
    return;
  p->panicMode = true;
  p->hadError = true;

  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

static void error_at_current(Parser *p, const char *message) {
  parse_error_at(p, &p->current, message);
}

static void parse_error(Parser *p, const char *message) {
  parse_error_at(p, &p->previous, message);
}
