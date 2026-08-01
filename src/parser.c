#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "lexer.h"
#include "object.h"
#include "parser.h"
#include "token.h"
#include "token_stream.h"

#ifndef TRACELN
#define TRACELN(...) ((void)0)
#endif

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

typedef AstNode *(*PrefixParseFn)(Parser *p, bool canAssign);
typedef AstNode *(*InfixParseFn)(Parser *p, AstNode *left, bool canAssign);

typedef struct {
  PrefixParseFn prefix;
  InfixParseFn infix;
  Precedence precedence;
} ParseRule;

static AstNode *expression(Parser *p);
static AstNode *statement(Parser *p, bool *isTail);
static AstNode *declaration(Parser *p, bool *isTail);
static AstNode *parse_precedence(Parser *p, Precedence prec);
static ParseRule *get_rule(TokenType type);
static void parse_error_at(Parser *p, Token *token, const char *message);
static void error_at_current(Parser *p, const char *message);
static void parse_error(Parser *p, const char *message);
static AstNode *blockExpr(Parser *p, bool canAssign);
static BlockNode parseBlock(Parser *p);
static AstNode *lambda(Parser *p, bool canAssign);
static AstNode *ifExpr(Parser *p, bool canAssign);
static AstNode *nullish_(Parser *p, AstNode *left, bool canAssign);
static AstNode *struct_(Parser *p, AstNode *left, bool canAssign);
static AstNode *varDeclaration(Parser *p, bool isMutable);

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
    case TOKEN_STRUCT:
    case TOKEN_IMPL:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_LET:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
    case TOKEN_RIGHT_BRACE:
      return;
    default:;
    }

    advance(p);
  }
}

static AstNode *parse_precedence(Parser *parser, Precedence precedence) {
  TRACELN("parser.parse_precedence(%d)", precedence);

  advance(parser);

  PrefixParseFn prefixRule = get_rule(parser->previous.type)->prefix;

  if (prefixRule == NULL) {
    parse_error(parser, "Expect expression.");
    return NULL;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  AstNode *left = prefixRule(parser, canAssign);

  while (precedence <= get_rule(parser->current.type)->precedence) {
    advance(parser);
    InfixParseFn infixRule = get_rule(parser->previous.type)->infix;
    left = infixRule(parser, left, canAssign);
  }

  if (canAssign && match(parser, TOKEN_EQUAL)) {
    parse_error(parser, "Invalid assignment target.");
  }

  TRACELN("parser.parse_precedence() end");

  return left;
}

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

  AstNode *node = astAlloc(NODE_LITERAL, parser->previous.line);

  const char *chars = parser->previous.start + 1;
  int length = parser->previous.length - 2;

  char *buffer = (char *)malloc((size_t)length + 1);
  int out = 0;

  for (int i = 0; i < length; i++) {
    if (chars[i] == '\\' && i + 1 < length) {
      i++;
      switch (chars[i]) {
      case 'n':
        buffer[out++] = '\n';
        break;
      case 'r':
        buffer[out++] = '\r';
        break;
      case 't':
        buffer[out++] = '\t';
        break;
      case '"':
        buffer[out++] = '"';
        break;
      case '\\':
        buffer[out++] = '\\';
        break;
      default: {
        char msg[48];
        snprintf(msg, sizeof(msg), "Invalid escape sequence: \\%c", chars[i]);
        parse_error(parser, msg);
        break;
      }
      }
    } else {
      buffer[out++] = chars[i];
    }
  }

  node->as.literal.value = OBJ_VAL(copyString(buffer, out));
  free(buffer);

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

static AstNode *self_(Parser *p, bool canAssign) {
  (void)canAssign;

  AstNode *node = astAlloc(NODE_SELF, p->previous.line);
  node->as.self_.keyword = p->previous;

  return node;
}

static AstNode *ifExpr(Parser *p, bool canAssign) {
  (void)canAssign;

  int line = p->previous.line;

  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");

  AstNode *cond = expression(p);

  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  AstNode *thenBranch = expression(p);
  AstNode *elseBranch = NULL;

  if (match(p, TOKEN_ELSE)) {
    elseBranch = expression(p);
  }

  AstNode *node = astAlloc(NODE_IF, line);
  node->as.if_.condition = cond;
  node->as.if_.thenBranch = thenBranch;
  node->as.if_.elseBranch = elseBranch;

  return node;
}

static AstNode *arrayLiteral(Parser *p, bool canAssign) {
  (void)canAssign;

  Token bracket = p->previous;

  ArrayNodeData and;
  arrayNodeDataInit(&and);

  TRACELN("arrayLiteral data initialized empty");

  if (!check(p, TOKEN_RIGHT_BRACKET)) {
    TRACELN("right bracket not found");

    do {
      AstNode *expr_node = expression(p);
      arrayNodeDataWrite(&and, expr_node);

    } while (match(p, TOKEN_COMMA));
  }

  consume(p, TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");

  TRACELN("right bracket found");

  AstNode *node = astAlloc(NODE_ARRAY, bracket.line);
  node->as.array.count = and.count;

  if (and.count > 0) {
    AstNode **items = (AstNode **)astAllocRaw(and.count * sizeof(AstNode *));
    memcpy(items, and.data, and.count * sizeof(AstNode *));
    node->as.array.items = items;
  } else {
    node->as.array.items = NULL;
  }

  arrayNodeDataFree(&and);

  return node;
}

static AstNode *binary(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;

  Token op = p->previous;

  ParseRule *rule = get_rule(op.type);
  AstNode *right = parse_precedence(p, (Precedence)(rule->precedence + 1));

  AstNode *node = astAlloc(NODE_BINARY, op.line);
  node->as.binary.op = op;
  node->as.binary.left = left;
  node->as.binary.right = right;

  return node;
}

static AstNode *and_(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;

  AstNode *right = parse_precedence(p, PREC_AND);

  AstNode *node = astAlloc(NODE_AND, p->previous.line);
  node->as.logical.left = left;
  node->as.logical.right = right;

  return node;
}

static AstNode *or_(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;

  AstNode *right = parse_precedence(p, PREC_OR);

  AstNode *node = astAlloc(NODE_OR, p->previous.line);
  node->as.logical.left = left;
  node->as.logical.right = right;

  return node;
}

static AstNode *nullish_(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;

  AstNode *right = parse_precedence(p, PREC_OR);

  AstNode *node = astAlloc(NODE_NULLISH, p->previous.line);
  node->as.logical.left = left;
  node->as.logical.right = right;

  return node;
}

static AstNode *call(Parser *p, AstNode *left, bool canAssign) {
  (void)canAssign;
  int line = p->previous.line;

  AstNode *argBuf[256];
  int argCount = 0;

  if (!check(p, TOKEN_RIGHT_PAREN)) {
    do {
      if (argCount >= 255) {
        error_at_current(p, "Can't have more than 255 arguments.");
      }

      argBuf[argCount++] = expression(p);
    } while (match(p, TOKEN_COMMA));
  }

  Token paren = p->current;

  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

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

static AstNode *struct_(Parser *p, AstNode *name, bool canAssign) {
  (void)canAssign;

  bool isStructName = name != NULL && name->kind == NODE_VARIABLE;

  if (!isStructName) {
    parse_error(p, "Only a struct name can be initialized with '{'.");
  }

  StructInitFieldNode fieldBuf[256];
  int fieldCount = 0;

  if (!check(p, TOKEN_RIGHT_BRACE)) {
    do {
      // Trailing comma
      if (check(p, TOKEN_RIGHT_BRACE))
        break;

      if (fieldCount >= 255) {
        error_at_current(p, "Can't have more than 255 fields.");
      }

      consume(p, TOKEN_IDENTIFIER, "Expect field name.");
      Token fieldName = p->previous;

      for (int i = 0; i < fieldCount; i++) {
        if (fieldBuf[i].name.length == fieldName.length &&
            memcmp(fieldBuf[i].name.start, fieldName.start,
                   (size_t)fieldName.length) == 0) {
          parse_error(p, "Duplicate field in struct initializer.");
          break;
        }
      }

      consume(p, TOKEN_COLON, "Expect ':' after field name.");

      AstNode *value = expression(p);

      if (fieldCount < 255) {
        fieldBuf[fieldCount].name = fieldName;
        fieldBuf[fieldCount].value = value;
        fieldCount++;
      }
    } while (match(p, TOKEN_COMMA));
  }

  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after struct initializer.");

  if (!isStructName)
    return name;

  Token name_token = name->as.variable.name;

  StructInitFieldNode *fields = NULL;
  if (fieldCount > 0) {
    fields = (StructInitFieldNode *)astAllocRaw(fieldCount *
                                                sizeof(StructInitFieldNode));
    memcpy(fields, fieldBuf, fieldCount * sizeof(StructInitFieldNode));
  }

  AstNode *node = astAlloc(NODE_STRUCT_INIT, name_token.line);
  node->as.structInit.name = name_token;
  node->as.structInit.fields = fields;
  node->as.structInit.fieldCount = fieldCount;
  node->as.structInit.endLine = p->previous.line;

  return node;
}

static AstNode *index_(Parser *p, AstNode *left, bool canAssign) {
  Token bracket = p->previous;
  AstNode *index = expression(p);
  consume(p, TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

  if (canAssign && match(p, TOKEN_EQUAL)) {
    AstNode *value = expression(p);
    AstNode *node = astAlloc(NODE_INDEX_SET, bracket.line);
    node->as.indexSet.object = left;
    node->as.indexSet.index = index;
    node->as.indexSet.value = value;
    node->as.indexSet.bracket = bracket;
    return node;
  }

  AstNode *node = astAlloc(NODE_INDEX_GET, bracket.line);
  node->as.indexGet.object = left;
  node->as.indexGet.index = index;
  node->as.indexGet.bracket = bracket;
  return node;
}

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {blockExpr, struct_, PREC_CALL},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACKET] = {arrayLiteral, index_, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_COLON] = {NULL, NULL, PREC_NONE},
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
    [TOKEN_STRUCT] = {NULL, NULL, PREC_NONE},
    [TOKEN_IMPL] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {lambda, NULL, PREC_NONE},
    [TOKEN_IF] = {ifExpr, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_QUESTION_QUESTION] = {NULL, nullish_, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SELF] = {self_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_LET] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_MODULO] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *get_rule(TokenType type) { return &rules[type]; }

static BlockNode parseBlock(Parser *p) {
  int capacity = 8, count = 0;
  AstNode **buf = (AstNode **)malloc(capacity * sizeof(AstNode *));
  AstNode *tailNode = NULL;

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    bool isTail = false;
    AstNode *node = declaration(p, &isTail);

    if (isTail) {
      tailNode = node;
      break;
    }

    if (count >= capacity) {
      capacity *= 2;
      buf = (AstNode **)realloc(buf, capacity * sizeof(AstNode *));
    }

    buf[count++] = node;
  }

  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block.");

  AstNode **stmts = NULL;

  if (count > 0) {
    stmts = (AstNode **)astAllocRaw(count * sizeof(AstNode *));
    memcpy(stmts, buf, count * sizeof(AstNode *));
  }

  free(buf);

  BlockNode block;
  block.stmts = stmts;
  block.count = count;
  block.value = tailNode;
  block.endLine = p->previous.line; // the '}' just consumed above

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

static AstNode *breakStatement(Parser *p) {
  Token token = p->previous;
  int line = token.line;

  consume(p, TOKEN_SEMICOLON, "Expect ';' after break.");

  AstNode *node = astAlloc(NODE_BREAK, line);
  node->as.break_.token = token;

  return node;
}

static AstNode *expressionStatement(Parser *p, bool *isTail) {
  int line = p->current.line;
  AstNode *expr = expression(p);

  if (check(p, TOKEN_RIGHT_BRACE)) {
    *isTail = true;
    return expr;
  }

  if (!match(p, TOKEN_SEMICOLON)) {
    parse_error(p, "Expect ';' after expression.");
  }

  *isTail = false;
  AstNode *node = astAlloc(NODE_EXPR_STMT, line);
  node->as.exprStmt.expr = expr;

  return node;
}

static AstNode *ifStatement(Parser *p, bool *isTail) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  AstNode *cond = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AstNode *thenBranch = statement(p, isTail);
  AstNode *elseBranch = NULL;
  if (match(p, TOKEN_ELSE))
    elseBranch = statement(p, isTail);

  AstNode *node = astAlloc(NODE_IF, line);
  node->as.if_.condition = cond;
  node->as.if_.thenBranch = thenBranch;
  node->as.if_.elseBranch = elseBranch;
  return node;
}

static AstNode *whileStatement(Parser *p, bool *isTail) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  AstNode *cond = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  AstNode *body = statement(p, isTail);

  AstNode *node = astAlloc(NODE_WHILE, line);
  node->as.while_.condition = cond;
  node->as.while_.body = body;
  return node;
}

// static AstNode *forStatement(Parser *p, bool *isTail) {
//   int line = p->previous.line;
//   consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
//   AstNode *cond = expression(p);
//   consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
//   AstNode *body = statement(p, isTail);

//   AstNode *node = astAlloc(NODE_WHILE, line);
//   node->as.for_.init = NULL;
//   node->as.for_.condition = cond;
//   node->as.for_.body = body;
//   node->as.for_.increment = NULL;
//   return node;
// }

// A `for` loop is desugared into a single NODE_WHILE at parse time, with
// its init/increment clauses kept as distinct fields (see WhileNode in
// ast.h) rather than spliced into surrounding blocks -- see compileWhile()
// in compiler.c for why.
static AstNode *forStatement(Parser *p, bool *isTail) {
  int line = p->previous.line;
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  AstNode *init = NULL;
  if (match(p, TOKEN_SEMICOLON)) {
    // no initialiser
  } else if (match(p, TOKEN_VAR)) {
    init = varDeclaration(p, /*isMutable=*/true);
  } else if (match(p, TOKEN_LET)) {
    init = varDeclaration(p, /*isMutable=*/false);
  } else {
    init = expressionStatement(p, isTail);
  }

  AstNode *cond = NULL;
  if (!check(p, TOKEN_SEMICOLON))
    cond = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

  AstNode *incr = NULL;
  if (!check(p, TOKEN_RIGHT_PAREN))
    incr = expression(p);
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

  AstNode *body = statement(p, isTail);

  if (cond == NULL) {
    cond = astAlloc(NODE_LITERAL, line);
    cond->as.literal.value = BOOL_VAL(true);
  }

  AstNode *loop = astAlloc(NODE_FOR, line);
  loop->as.for_.init = init;
  loop->as.for_.condition = cond;
  loop->as.for_.body = body;
  loop->as.for_.increment = incr;
  return loop;
}

// Determines whether the upcoming token can start an EXPRESSION, for the
// dynamic per-item dispatch a block used as an expression needs (see
// parseBlockExprContents below). `fun` is special-cased to always mean a
// (named) function declaration even inside a block-expression, matching
// the original: `fun` has a prefix rule (lambda) too, but block-expressions
// never want a bare `fun name() {...}` line to be parsed as a lambda
// expression-statement.
static bool isExpressionStart(TokenType type) {
  if (type == TOKEN_FUN)
    return false;
  return get_rule(type)->prefix != NULL;
}

// Parses the contents of a block used in EXPRESSION position: `{ ... }`.
// Unlike a regular (statement) block, each item is independently
// classified: if the next token can start an expression, it's parsed as an
// expression -- POP'd once compiled, unless it's the last item before `}`,
// in which case it becomes the block's value (BlockNode.value); otherwise
// it's parsed as a full declaration
// (var/struct/impl/for/while/print/return/fun).
static BlockNode parseBlockExprContents(Parser *p) {
  int capacity = 8, count = 0;
  AstNode **buf = (AstNode **)malloc(capacity * sizeof(AstNode *));
  AstNode *tailNode = NULL;

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    AstNode *node;

    if (isExpressionStart(p->current.type)) {
      int line = p->current.line;
      AstNode *expr = expression(p);

      if (check(p, TOKEN_RIGHT_BRACE)) {
        tailNode = expr;
        break;
      }

      consume(p, TOKEN_SEMICOLON, "Expect ';' after expression.");
      node = astAlloc(NODE_EXPR_STMT, line);
      node->as.exprStmt.expr = expr;
    } else {
      bool isTail = false;
      node = declaration(p, &isTail);
    }

    if (count >= capacity) {
      capacity *= 2;
      buf = (AstNode **)realloc(buf, capacity * sizeof(AstNode *));
    }
    buf[count++] = node;

    if (p->panicMode) {
      synchronize(p);
    }
  }

  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after block expression.");

  AstNode **stmts = NULL;
  if (count > 0) {
    stmts = (AstNode **)astAllocRaw(count * sizeof(AstNode *));
    memcpy(stmts, buf, count * sizeof(AstNode *));
  }
  free(buf);

  BlockNode block;
  block.stmts = stmts;
  block.count = count;
  block.value = tailNode;
  block.endLine = p->previous.line; // the '}' just consumed above
  return block;
}

static AstNode *blockExpr(Parser *p, bool canAssign) {
  (void)canAssign;
  int line = p->previous.line;
  BlockNode block = parseBlockExprContents(p);
  AstNode *node = astAlloc(NODE_BLOCK, line);
  node->as.block = block;
  return node;
}

static AstNode *returnStatement(Parser *p) {
  int line = p->previous.line;
  AstNode *value = NULL;
  if (!check(p, TOKEN_SEMICOLON))
    value = expression(p);
  consume(p, TOKEN_SEMICOLON, "Expect ';' after return value.");
  AstNode *node = astAlloc(NODE_RETURN, line);
  node->as.return_.value = value;
  return node;
}

static AstNode *statement(Parser *p, bool *isTail) {
  *isTail = false;

  if (match(p, TOKEN_PRINT))
    return printStatement(p);
  if (match(p, TOKEN_IF))
    return ifStatement(p, isTail);
  if (match(p, TOKEN_WHILE))
    return whileStatement(p, isTail);
  if (match(p, TOKEN_FOR))
    return forStatement(p, isTail);
  if (match(p, TOKEN_RETURN))
    return returnStatement(p);
  if (match(p, TOKEN_LEFT_BRACE))
    return blockStatement(p);
  if (match(p, TOKEN_BREAK))
    return breakStatement(p);
  return expressionStatement(p, isTail);
}

static AstNode *varDeclaration(Parser *p, bool isMutable) {
  consume(p, TOKEN_IDENTIFIER, "Expect variable name.");
  Token name = p->previous;

  AstNode *initializer = NULL;
  if (match(p, TOKEN_EQUAL))
    initializer = expression(p);

  if (!isMutable && initializer == NULL)
    parse_error(p, "'let' binding requires an initializer.");

  consume(p, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  AstNode *node = astAlloc(NODE_VAR_DECL, name.line);
  node->as.varDecl.name = name;
  node->as.varDecl.initializer = initializer;
  node->as.varDecl.isMutable = isMutable;
  node->as.varDecl.declEndLine = p->previous.line; // the ';' just consumed
  return node;
}

// Parses the "(params) { body }" or "(params) = expr;" tail shared by
// function declarations, methods, and lambdas -- the `fun` keyword (and, for
// a function or method, the name identifier) has already been consumed by
// the caller.
// `name` is stored on the node but is otherwise unused when isLambda is
// true (there's no real name token for an anonymous function -- the
// compiler generates one, matching the original's `lambda0x...` naming).
static AstNode *functionTail(Parser *p, Token name, int line, bool isMethod,
                             bool isLambda) {
  consume(p, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  Token paramBuf[256];
  int arity = 0;

  bool hasSelf = false;
  if (check(p, TOKEN_SELF)) {
    if (!isMethod) {
      error_at_current(
          p, "Only methods in an 'impl' block can declare a 'self' parameter.");
    }
    advance(p);
    hasSelf = true;
  }

  if (!check(p, TOKEN_RIGHT_PAREN) && (!hasSelf || match(p, TOKEN_COMMA))) {
    do {
      if (arity >= 255) {
        error_at_current(p, "Can't have more than 255 parameters.");
      }

      if (check(p, TOKEN_SELF)) {
        error_at_current(p, "'self' must be the first parameter.");
      }
      consume(p, TOKEN_IDENTIFIER, "Expect parameter name.");
      paramBuf[arity++] = p->previous;
    } while (match(p, TOKEN_COMMA));
  }
  consume(p, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  Token *params = NULL;
  if (arity > 0) {
    params = (Token *)astAllocRaw(arity * sizeof(Token));
    memcpy(params, paramBuf, arity * sizeof(Token));
  }

  AstNode *node = astAlloc(NODE_FUNCTION, line);
  node->as.function.name = name;
  node->as.function.params = params;
  node->as.function.arity = arity;
  node->as.function.isMethod = isMethod;
  node->as.function.hasSelf = hasSelf;
  node->as.function.isLambda = isLambda;
  node->as.function.exprBody = NULL;
  node->as.function.body.stmts = NULL;
  node->as.function.body.count = 0;
  node->as.function.body.value = NULL;
  node->as.function.body.endLine = line;
  node->as.function.bodyEndLine = line;

  if (!isLambda && match(p, TOKEN_EQUAL)) {
    // Function body expression: `fun sum(a, b) = a + b;`. Only offered for
    // named functions/methods, which are always their own complete
    // statement -- the ';' consumed here is that statement's own
    // terminator. A lambda has no such guarantee (it's usually embedded in
    // a larger statement, e.g. `var f = fun (x) = x;`), which would leave
    // that enclosing statement's own ';' unconsumed; matches the
    // changelog's "Lambda body expressions" item, which is intentionally
    // still unchecked/unimplemented.
    node->as.function.exprBody = expression(p);
    consume(p, TOKEN_SEMICOLON, "Expect ';' after function body expression.");
    node->as.function.bodyEndLine = p->previous.line; // the ';' just consumed
  } else {
    consume(p, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    node->as.function.body = parseBlock(p);
    node->as.function.bodyEndLine = node->as.function.body.endLine;
  }

  return node;
}

static AstNode *functionDeclaration(Parser *p, bool isMethod) {
  if (!isMethod)
    consume(p, TOKEN_IDENTIFIER, "Expect function name.");
  Token name = p->previous;
  int line = name.line;
  return functionTail(p, name, line, isMethod, /*isLambda=*/false);
}

// A lambda expression: `fun (a, b) { a + b }` or `fun (a, b) = a + b;`,
// used anywhere an expression is expected (e.g. `var f = fun (x) { x };`).
// The 'fun' keyword is already consumed (p->previous) -- there's no name to
// consume next, just straight into the parameter list.
static AstNode *lambda(Parser *p, bool canAssign) {
  (void)canAssign;
  Token funKeyword = p->previous;
  return functionTail(p, funKeyword, funKeyword.line, /*isMethod=*/false,
                      /*isLambda=*/true);
}

static AstNode *structDeclaration(Parser *p) {
  consume(p, TOKEN_IDENTIFIER, "Expect struct name.");
  Token name = p->previous;
  int line = name.line;

  consume(p, TOKEN_LEFT_BRACE, "Expect '{' before struct body.");

  int fieldCap = 8, fieldCount = 0;
  VarDeclNode *fieldBuf = (VarDeclNode *)malloc(fieldCap * sizeof(VarDeclNode));

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    if (fieldCount >= fieldCap) {
      fieldCap *= 2;
      fieldBuf =
          (VarDeclNode *)realloc(fieldBuf, fieldCap * sizeof(VarDeclNode));
    }

    if (match(p, TOKEN_VAR)) {
      consume(p, TOKEN_IDENTIFIER, "Expect field name.");
      Token fieldName = p->previous;
      AstNode *init = NULL;
      if (match(p, TOKEN_EQUAL)) {
        init = expression(p);
      }
      consume(p, TOKEN_SEMICOLON, "Expect ';' after field.");

      fieldBuf[fieldCount].name = fieldName;
      fieldBuf[fieldCount].initializer = init;
      fieldBuf[fieldCount].isMutable = true;
      fieldBuf[fieldCount].declEndLine = p->previous.line; // the ';'
      fieldCount++;
    } else if (check(p, TOKEN_FUN)) {
      error_at_current(p, "Expect field declaration. Methods belong in an "
                          "'impl' block.");
      advance(p); // 'fun'
      if (check(p, TOKEN_IDENTIFIER)) {
        advance(p); // the method name
        functionDeclaration(p, /*isMethod=*/true);
      }
      p->panicMode = false;
    } else {
      error_at_current(p, "Expect field declaration.");
    }

    // Without this, a malformed member (e.g. a missing '}') leaves the
    // offending token in place forever and this loop spins indefinitely,
    // since nothing here ever calls synchronize() the way declaration() does
    // for top-level statements.
    if (p->panicMode) {
      synchronize(p);
    }
  }
  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after struct body.");
  int endLine = p->previous.line;

  VarDeclNode *fields = NULL;
  if (fieldCount > 0) {
    fields = (VarDeclNode *)astAllocRaw(fieldCount * sizeof(VarDeclNode));
    memcpy(fields, fieldBuf, fieldCount * sizeof(VarDeclNode));
  }
  free(fieldBuf);

  AstNode *node = astAlloc(NODE_STRUCT, line);
  node->as.struct_.name = name;
  node->as.struct_.fields = fields;
  node->as.struct_.fieldCount = fieldCount;
  node->as.struct_.endLine = endLine;
  return node;
}

static AstNode *implDeclaration(Parser *p) {
  consume(p, TOKEN_IDENTIFIER, "Expect struct name after 'impl'.");
  Token name = p->previous;
  int line = name.line;

  consume(p, TOKEN_LEFT_BRACE, "Expect '{' before impl body.");

  int methodCap = 8, methodCount = 0;
  FunctionNode **methodBuf =
      (FunctionNode **)malloc(methodCap * sizeof(FunctionNode *));

  while (!check(p, TOKEN_RIGHT_BRACE) && !is_at_end(p)) {
    if (methodCount >= methodCap) {
      methodCap *= 2;
      methodBuf = (FunctionNode **)realloc(methodBuf,
                                           methodCap * sizeof(FunctionNode *));
    }

    if (check(p, TOKEN_VAR)) {
      error_at_current(p, "Expect method declaration. Fields belong in the "
                          "'struct' declaration.");
      advance(p); // 'var'
      if (check(p, TOKEN_IDENTIFIER)) {
        advance(p);
        if (match(p, TOKEN_EQUAL))
          expression(p);
        match(p, TOKEN_SEMICOLON);
      }
      p->panicMode = false;
    } else {
      consume(p, TOKEN_FUN, "Expect 'fun' before method declaration.");
      consume(p, TOKEN_IDENTIFIER, "Expect method name.");
      AstNode *method = functionDeclaration(p, /*isMethod=*/true);
      methodBuf[methodCount++] = &method->as.function;
    }

    if (p->panicMode) {
      synchronize(p);
    }
  }
  consume(p, TOKEN_RIGHT_BRACE, "Expect '}' after impl body.");
  int endLine = p->previous.line;

  FunctionNode **methods = NULL;
  if (methodCount > 0) {
    methods =
        (FunctionNode **)astAllocRaw(methodCount * sizeof(FunctionNode *));
    memcpy(methods, methodBuf, methodCount * sizeof(FunctionNode *));
  }
  free(methodBuf);

  AstNode *node = astAlloc(NODE_IMPL, line);
  node->as.impl.name = name;
  node->as.impl.methods = methods;
  node->as.impl.methodCount = methodCount;
  node->as.impl.endLine = endLine;
  return node;
}

static AstNode *declaration(Parser *p, bool *isTail) {
  AstNode *node = NULL;
  *isTail = false;

  if (match(p, TOKEN_STRUCT)) {
    node = structDeclaration(p);
  } else if (match(p, TOKEN_IMPL)) {
    node = implDeclaration(p);
  } else if (match(p, TOKEN_FUN)) {
    node = functionDeclaration(p, /*isMethod=*/false);
  } else if (match(p, TOKEN_VAR)) {
    node = varDeclaration(p, /*isMutable=*/true);
  } else if (match(p, TOKEN_LET)) {
    node = varDeclaration(p, /*isMutable=*/false);
  } else {
    node = statement(p, isTail);
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

AstNode **parse(const char *source, int *outCount, bool *hadError,
                int *outEndLine) {
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
    bool isTail = false;
    ast[count++] = declaration(&parser, &isTail);
  }

  *outEndLine = parser.current.line; // the EOF token itself

  tsFree(&tokens);

  *outCount = count;
  *hadError = parser.hadError;

  return ast;
}

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
