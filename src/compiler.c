#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

static void advance(Scanner *scanner);
static uint8_t identifierConstant(Token *name);
static uint8_t parseVariable(Scanner *scanner, const char *errorMessage);
static void defineVariable(uint8_t global);
static void expression(Scanner *scanner);
static bool check(TokenType type);
static void varDeclaration(Scanner *scanner);
static void expressionStatement(Scanner *scanner);
static void consume(Scanner *scanner, TokenType type, const char *message);
static ParseRule *getRule(TokenType type);
static bool match(Scanner *scanner, TokenType type);
static void statement(Scanner *scanner);
static void printStatement(Scanner *scanner);
static void synchronize(Scanner *scanner);
static void declaration(Scanner *scanner);
static uint8_t makeConstant(Value value);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static ObjFunction *endCompiler();
static void beginScope();
static void block(Scanner *scanner);
static void ifStatement(Scanner *scanner);
static void patchJump(int offset);
static int emitJump(uint8_t instruction);
static void whileStatement(Scanner *scanner);
static void emitLoop(int loopStart);
static void forStatement(Scanner *scanner);
static void funDeclaration(Scanner *scanner);
static void returnStatement(Scanner *scanner);
static void emitImplicitReturn();
static void emitValueReturn();
static void endScope();
static void classDeclaration(Scanner *scanner);
static void namedVariable(Scanner *scanner, Token name, bool canAssign);
static void indexValue(Scanner *scanner, bool canAssign);
static void error(const char *message);

Parser parser;
Chunk *compilingChunk;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;

/**
 * Initialize compiler to compile a function
 *
 * - Sets current compiler as the enclosing compiler
 * - Sets the function type being compiled
 * - Sets new compiler as the current one
 */
static void initCompiler(Compiler *compiler, FunctionType functionType) {
  TRACELN("  compiler.initCompiler()");
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = functionType;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  current = compiler;

  if (functionType != TYPE_SCRIPT) {
    current->function->name =
        copyString(parser.previous.start, parser.previous.length);
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;

  if (functionType != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

ObjFunction *compile(Scanner *scanner, const char *source) {
  TRACELN("  compiler.compile()");

  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance(scanner);

  while (!match(scanner, TOKEN_EOF)) {
    declaration(scanner);
  }

  ObjFunction *function = endCompiler();

  return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}

static void declaration(Scanner *scanner) {
  TRACELN("  compiler.declaration()");
  if (match(scanner, TOKEN_CLASS)) {
    classDeclaration(scanner);
  } else if (match(scanner, TOKEN_FUN)) {
    funDeclaration(scanner);
  } else if (match(scanner, TOKEN_VAR)) {
    varDeclaration(scanner);
  } else {
    statement(scanner);
  }

  if (parser.panicMode)
    synchronize(scanner);
}

static void statement(Scanner *scanner) {
  TRACELN("  compiler.statement()");

  if (match(scanner, TOKEN_PRINT)) {
    printStatement(scanner);
  } else if (match(scanner, TOKEN_FOR)) {
    forStatement(scanner);
  } else if (match(scanner, TOKEN_IF)) {
    ifStatement(scanner);
  } else if (match(scanner, TOKEN_RETURN)) {
    returnStatement(scanner);
  } else if (match(scanner, TOKEN_WHILE)) {
    whileStatement(scanner);
  } else if (match(scanner, TOKEN_LEFT_BRACE)) {
    beginScope();
    block(scanner);
    endScope();
  } else {
    expression(scanner);

    if (match(scanner, TOKEN_SEMICOLON)) {
      emitByte(OP_POP);
      return;
    }

    if (current->type != TYPE_SCRIPT && check(TOKEN_RIGHT_BRACE)) {
      emitByte(OP_RETURN);
      return;
    }

    error("Expect ';' after expression.");
  }
}

/**
 * Get the current chunk being compiled
 */
static Chunk *currentChunk() { return &current->function->chunk; }

static uint8_t previousOpCode() {
  Chunk *chunk = currentChunk();

  if (chunk->count == 0) {
    return 0;
  }

  return chunk->code[chunk->count - 1];
}

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;

  parser.hadError = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s' (%s)", token->length, token->start,
            tokenTypeToString(token->type));
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void advance(Scanner *scanner) {
  // TRACELN(ANSI_COLOR_YELLOW "compiler.advance()" ANSI_COLOR_RESET);

  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken(scanner);

    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}

static void parsePrecedence(Scanner *scanner, Precedence precedence) {
  TRACELN("  compiler.parsePrecedence(%d)", precedence);

  advance(scanner);
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(scanner, canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance(scanner);
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(scanner, canAssign);
  }

  if (canAssign && match(scanner, TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }

  TRACELN("  compiler.parsePrecedence() end");
}

static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer");
      }

      TRACELN("  compiler.resolveLocal() -> Found local %d", i);

      return i;
    }
  }

  TRACELN("  compiler.resolveLocal() -> Could not find local");

  return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;

  local->isCaptured = false;
}

static void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  TRACELN("  compiler.declareVariable() -> %s", AS_CSTRING(OBJ_VAL(name)));
  addLocal(*name);
}

static void and_(Scanner *scanner, bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(scanner, PREC_AND);

  patchJump(endJump);
}

static uint8_t parseVariable(Scanner *scanner, const char *errorMessage) {
  TRACELN("  compiler.parseVariable()");

  consume(scanner, TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0)
    return 0;

  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0)
    return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  TRACELN("  compiler.defineVariable()");

  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }

  emitBytes(OP_DEFINE_GLOBAL, global);
}

static void consume(Scanner *scanner, TokenType type, const char *message) {
  TRACELN("  compiler.consume(%s)", tokenTypeToString(type));

  if (parser.current.type == type) {
    TRACELN("  compiler.consume matches = true");
    advance(scanner);
    return;
  }

  errorAtCurrent(message);
}

static uint8_t argumentList(Scanner *scanner) {
  TRACELN("  compiler.argumentList()");

  uint8_t argCount = 0;

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression(scanner);
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(scanner, TOKEN_COMMA));
  }

  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");

  return argCount;
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(Scanner *scanner, TokenType type) {
  if (!check(type))
    return false;
  advance(scanner);
  return true;
}

static void expression(Scanner *scanner) {
  TRACELN("  compiler.expression()");

  parsePrecedence(scanner, PREC_ASSIGNMENT);
}

static void block(Scanner *scanner) {
  TRACELN("  compiler.block()");

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration(scanner);
  }

  consume(scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(Scanner *scanner, FunctionType type) {
  TRACELN("  compiler.function()");
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(scanner, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }

      uint8_t constant = parseVariable(scanner, "Expect parameter name.");

      defineVariable(constant);
    } while (match(scanner, TOKEN_COMMA));
  }

  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  if (match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
    consume(scanner, TOKEN_SEMICOLON,
            "Expect ';' after function body expression");
    emitValueReturn();
  } else {
    consume(scanner, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(scanner);
  }

  TRACELN("  compiler.function() end");

  ObjFunction *function = endCompiler();

  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void method(Scanner *scanner) {
  consume(scanner, TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifierConstant(&parser.previous);

  FunctionType type = TYPE_METHOD;

  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }

  function(scanner, type);

  emitBytes(OP_METHOD, constant);
}

static void fieldDeclaration(Scanner *scanner) {
  consume(scanner, TOKEN_IDENTIFIER, "Expect field name.");
  uint8_t constant = identifierConstant(&parser.previous);

  if (match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
  } else {
    emitByte(OP_NIL);
  }

  consume(scanner, TOKEN_SEMICOLON, "Expect ';' after field.");
  emitBytes(OP_FIELD, constant);
}
static void classDeclaration(Scanner *scanner) {
  consume(scanner, TOKEN_IDENTIFIER, "Expect class name.");
  Token className = parser.previous;
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable();

  emitBytes(OP_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

  namedVariable(scanner, className, false);
  consume(scanner, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    if (match(scanner, TOKEN_VAR)) {
      fieldDeclaration(scanner);
    } else {
      method(scanner);
    }
  }
  consume(scanner, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emitByte(OP_POP);

  currentClass = currentClass->enclosing;
}

static void funDeclaration(Scanner *scanner) {
  TRACELN("  compiler.funDeclaration()");

  uint8_t global = parseVariable(scanner, "Expected function name");
  markInitialized();
  function(scanner, TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration(Scanner *scanner) {
  TRACELN("  compiler.varDeclaration()");

  uint8_t global = parseVariable(scanner, "Expect variable name.");

  if (match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
  } else {
    emitByte(OP_NIL);
  }
  consume(scanner, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void expressionStatement(Scanner *scanner) {
  TRACELN("  compiler.expressionStatement()");

  expression(scanner);
  consume(scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void forStatement(Scanner *scanner) {
  TRACELN("  compiler.forStatement()");

  beginScope();

  consume(scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  if (match(scanner, TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(scanner, TOKEN_VAR)) {
    varDeclaration(scanner);
  } else {
    expressionStatement(scanner);
  }

  int loopStart = currentChunk()->count;

  int exitJump = -1;
  if (!match(scanner, TOKEN_SEMICOLON)) {
    expression(scanner);
    consume(scanner, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Condition.
  }

  if (!match(scanner, TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression(scanner);
    emitByte(OP_POP);
    consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  statement(scanner);
  emitLoop(loopStart);

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }

  endScope();
}

static void ifStatement(Scanner *scanner) {
  TRACELN("  compiler.ifStatement()");

  consume(scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(scanner);
  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement(scanner);

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(scanner, TOKEN_ELSE))
    statement(scanner);

  patchJump(elseJump);
}

static void ifExpression(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.ifExpression()");

  consume(scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(scanner);
  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  expression(scanner);

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (match(scanner, TOKEN_ELSE)) {
    expression(scanner);
  } else {
    emitByte(OP_NIL);
  }

  patchJump(elseJump);
}

static void printStatement(Scanner *scanner) {
  TRACELN("  compiler.printStatement()");

  expression(scanner);
  consume(scanner, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void returnStatement(Scanner *scanner) {
  TRACELN("  compiler.returnStatement()");

  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(scanner, TOKEN_SEMICOLON)) {
    emitByte(OP_NIL);
    emitValueReturn();
  } else {
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer.");
    }

    expression(scanner);
    consume(scanner, TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitValueReturn();
  }
}

static void whileStatement(Scanner *scanner) {
  TRACELN("  compiler.whileStatement()");

  int loopStart = currentChunk()->count;

  consume(scanner, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(scanner);
  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement(scanner);

  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);
}

static void synchronize(Scanner *scanner) {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;

    default:; // Do nothing.
    }

    advance(scanner);
  }
}

static void grouping(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.grouping()");

  expression(scanner);
  consume(scanner, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void emitByte(uint8_t byte) {
  // #ifdef DEBUG_PRINT_CODE
  //   TRACELN("  compiler.emitByte() -> %s", disassembleByte(byte));
  // #endif
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
  TRACELN("  compiler.emitJump()");

  emitByte(instruction);
  emitBytes(0xff, 0xff);
  return currentChunk()->count - 2;
}

static void binary(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.binary()");

  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence(scanner, (Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  case TOKEN_MODULO:
    emitByte(OP_MODULO);
    break;
  default:
    return; // Unreachable.
  }
}

static void call(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.call()");

  uint8_t argCount = argumentList(scanner);
  emitBytes(OP_CALL, argCount);
}

static void dot(Scanner *scanner, bool canAssign) {
  consume(scanner, TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);

  if (canAssign && match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
    emitBytes(OP_SET_PROPERTY, name);
  } else if (match(scanner, TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList(scanner);
    emitBytes(OP_INVOKE, name);
    emitByte(argCount);
  } else {
    emitBytes(OP_GET_PROPERTY, name);
  }
}

static void literal(Scanner *scanner, bool canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    TRACELN("  compiler.literal() -> false");
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    TRACELN("  compiler.literal() -> nil");
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    TRACELN("  compiler.literal() -> true");
    emitByte(OP_TRUE);
    break;
  default:
    return; // Unreachable.
  }
}

static void unary(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.unary()");

  TokenType operatorType = parser.previous.type;

  parsePrecedence(scanner, PREC_UNARY);

  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return;
  }
}

static void arrayLiteral(Scanner *scanner, bool canAssign) {
  int array_size = 0;

  if (!check(TOKEN_RIGHT_BRACKET)) {
    do {
      expression(scanner);
      array_size++;
    } while (match(scanner, TOKEN_COMMA));
  }

  consume(scanner, TOKEN_RIGHT_BRACKET, "Expect ']'");

  emitBytes(OP_ARRAY, array_size);
}

static void indexValue(Scanner *scanner, bool canAssign) {
  expression(scanner);

  consume(scanner, TOKEN_RIGHT_BRACKET, "Expect ']'");

  if (canAssign && match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
    emitByte(OP_SET_INDEX);
  } else {
    emitByte(OP_GET_INDEX);
  }
}

static uint8_t makeConstant(Value value) {
  int constant = addConstantToChunk(currentChunk(), value);

  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  if (IS_STRING(value)) {
    TRACELN("  compiler.makeConstant() -> %d = %s", constant,
            AS_CSTRING(value));
  } else {
    TRACELN("  compiler.makeConstant(%d) -> %d", value.type, constant);
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  TRACELN("  compiler.emitConstant()");

  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void number(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.number()");

  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void or_(Scanner *scanner, bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(scanner, PREC_OR);
  patchJump(endJump);
}

static void nullish(Scanner *scanner, bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_NOT_NIL);

  emitByte(OP_POP);                  // discard left if it's nil
  parsePrecedence(scanner, PREC_OR); // compile RHS

  patchJump(endJump);
}

static void string(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.string()");

  const char *chars = parser.previous.start + 1;
  int length = parser.previous.length - 2;

  char *buffer = ALLOCATE(char, length + 1);

  int out = 0;

  for (size_t i = 0; i < length; i++) {
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
        char errMsg[40];
        snprintf(errMsg, sizeof(errMsg), "Invalid escape sequence: %c",
                 chars[i]);
        error(errMsg);
        break;
      }
      }
    } else {
      buffer[out++] = chars[i];
    }
  }

  buffer[out] = '\0';

  ObjString *string = copyString(buffer, out);

  emitConstant(OBJ_VAL(string));
}

static void namedVariable(Scanner *scanner, Token name, bool canAssign) {
  TRACELN("  compiler.namedVariable()");

  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    TRACELN("  compiler.namedVariable() is local");
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    TRACELN("  compiler.namedVariable() is global");
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(scanner, TOKEN_EQUAL)) {
    expression(scanner);
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(Scanner *scanner, bool canAssign) {
  TRACELN("  compiler.variable()");
  namedVariable(scanner, parser.previous, canAssign);
}

static void this_(Scanner *scanner, bool canAssign) {
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class.");
    return;
  }

  variable(scanner, false);
}

static bool isExpressionStart(Token *token) {
  return getRule(token->type)->prefix != NULL;
}

static void endScopeExpression() {
  current->scopeDepth--;

  int locals = 0;

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {

    locals++;
    current->localCount--;
  }

  emitBytes(OP_CLOSE_BLOCK_EXPR, locals);
}

static void blockExpression(Scanner *scanner, bool canAssign) {
  beginScope();

  bool producedValue = false;

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    if (isExpressionStart(&parser.current)) {
      expression(scanner);

      if (check(TOKEN_RIGHT_BRACE)) {
        producedValue = true;
        break;
      }

      consume(scanner, TOKEN_SEMICOLON, "Expect ';' after expression.");

      emitByte(OP_POP);
    } else {
      declaration(scanner);
    }
  }

  consume(scanner, TOKEN_RIGHT_BRACE, "Expect '}' after block expression.");

  if (!producedValue) {
    emitByte(OP_NIL);
  }

  endScopeExpression();
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {blockExpression, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACKET] = {arrayLiteral, indexValue, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
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
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, and_, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {ifExpression, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_, PREC_OR},
    [TOKEN_QUESTION_QUESTION] = {NULL, nullish, PREC_OR},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {this_, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_MODULO] = {NULL, binary, PREC_FACTOR},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule *getRule(TokenType type) { return &rules[type]; }

static void emitImplicitReturn() {
  TRACELN("  compiler.emitImplicitReturn()");

  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

  emitByte(OP_RETURN);
}

static void emitValueReturn() {
  TRACELN("  compiler.emitValueReturn()");

  // Assumes return value is already on the stack.
  if (current->type == TYPE_INITIALIZER) {
    // Initializers always return 'this'.
    emitByte(OP_POP);
    emitBytes(OP_GET_LOCAL, 0);
  }

  emitByte(OP_RETURN);
}

static ObjFunction *endCompiler() {
  TRACELN("  compiler.endCompiler()");

  if (previousOpCode() != OP_RETURN) {
    emitImplicitReturn();
  }

  ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  current = current->enclosing;

  return function;
}

static void beginScope() { current->scopeDepth++; }

static void handleLocalsInScope() {
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    emitByte(OP_POP);
    current->localCount--;

    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
  }
}

static void endScope() {
  current->scopeDepth--;

  handleLocalsInScope();
}
