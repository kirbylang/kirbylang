#include "compiler.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "common.h"
#include "compiled_unit.h"
#include "token.h"

FnCompiler *current = NULL;
LoopCompiler *currentLoop = NULL;

static CompiledUnit *compilingUnit = NULL;

static bool hadError = false;

/**
 * Anonymous lambdas get an auto-generated name ("lambda0x...")
 */
static int lambdaCount = 0;

/**
 * The line most recently entered via compileExpr()/compileStmt()
 */
static int currentLine = 0;

/**
 * A resolved variable reference: which opcode pair to use, and the operand
 * (local slot / upvalue index / global-name constant index) to go with it.
 */
typedef struct {
  uint8_t getOp;
  uint8_t setOp;
  uint8_t arg;
  bool isMutable;
} VarRef;

static void compileExpr(AstNode *node);
static void compileStmt(AstNode *node);
static void compileBlockContents(BlockNode *block);
static void compileFunction(FunctionNode *fn, FunctionType type);
static uint8_t makeConstant(CompiledConst value, Token *tok);
static uint8_t stringConstant(const char *chars, int length, Token *tok);
static void emitNumberConstant(double number);
static void emitStringConstant(const char *chars, int length);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static void declareVariable(Token *name, bool isMutable);
static void defineVariable(uint8_t global);
static void markInitialized(void);
static void beginScope(void);
static void endScope(void);
static void errorAt(int line, const char *lexeme, int lexemeLen,
                    const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", line);
  if (lexeme != NULL) {
    fprintf(stderr, " at '%.*s'", lexemeLen, lexeme);
  }
  fprintf(stderr, ": %s\n", message);
}

static void errorAtToken(Token *token, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

static void errorAtNode(AstNode *node, const char *message) {
  errorAt(node->line, NULL, 0, message);
}

/**
 * Generic error handler
 *
 * Line number is recorded as 0.
 */
static void error(const char *message) {
  errorAt(currentLine, NULL, 0, message);
}

/**
 * Initialize compiler to compile a function
 */
static void initCompiler(FnCompiler *compiler, FunctionType functionType,
                         Token *nameToken) {
  TRACELN("  compiler.initCompiler()");

  compiler->enclosing = current;
  compiler->type = functionType;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->upvalueCount = 0;
  compiler->fnIndex = cuAddFunction(compilingUnit);
  compiler->fn = cuGetFnByIndex(compilingUnit, compiler->fnIndex);
  compiler->fn->isStatic = (functionType == TYPE_STATIC_METHOD);
  compiler->enclosingLoop = currentLoop;
  current = compiler;

  currentLoop = NULL;

  // cuAddFunction may realloc the functions array, invalidating cached fn
  // pointers on enclosing compilers. Refresh them from their indices.
  for (FnCompiler *c2 = current->enclosing; c2 != NULL; c2 = c2->enclosing) {
    c2->fn = cuGetFnByIndex(compilingUnit, c2->fnIndex);
  }

  if (nameToken != NULL) {
    current->fn->nameOffset =
        cuInternString(compilingUnit, nameToken->start, nameToken->length);
    current->fn->nameLength = nameToken->length;
  } else if (functionType == TYPE_FUNCTION) {
    lambdaCount++;
    char lambdaName[32];
    int len =
        snprintf(lambdaName, sizeof(lambdaName), "lambda0x%x", lambdaCount);
    if (len < 0) {
      len = 0;
    } else if (len >= (int)sizeof(lambdaName)) {
      len = (int)sizeof(lambdaName) - 1;
    }
    current->fn->nameOffset = cuInternString(compilingUnit, lambdaName, len);
    current->fn->nameLength = len;
  }

  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  local->isMutable = false;

  if (functionType == TYPE_METHOD) {
    local->name.start = "self";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

/**
 * Get the current function being compiled
 */
static CompiledFn *currentFn(void) { return current->fn; }

/**
 * Get the last emitted opcode
 */
static uint8_t previousOpCode(void) {
  CompiledFn *fn = currentFn();

  if (fn->codeCount == 0) {
    return 0;
  }

  return fn->code[fn->codeCount - 1];
}

/**
 * Make a new constant from an identifier token
 */
static uint8_t identifierConstant(Token *identifier) {
  return stringConstant(identifier->start, identifier->length, identifier);
}

/**
 * Compare two identifier tokens for equality
 */
static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Resolve local variable by identifier
 *
 * @returns -1 if not found, otherwise the index of the local from the
 * function's locals
 */
static int resolveLocal(FnCompiler *compiler, Token *identifier) {
  // Loop through the locals in reverse order to find by identifier
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];

    if (identifiersEqual(identifier, &local->name)) {
      if (local->depth == -1) {
        errorAtToken(identifier,
                     "Can't read local variable in its own initializer");
      }

      TRACELN("  compiler.resolveLocal() -> Found local %d", i);

      return i;
    }
  }

  TRACELN("  compiler.resolveLocal() -> Could not find local");

  return -1;
}

static int addUpvalue(FnCompiler *compiler, uint8_t index, bool isLocal,
                      bool isMutable) {
  int upvalueCount = compiler->upvalueCount;

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
  compiler->upvalues[upvalueCount].isMutable = isMutable;
  return compiler->upvalueCount++;
}

static int resolveUpvalue(FnCompiler *compiler, Token *name) {
  if (compiler->enclosing == NULL)
    return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t)local, true,
                      compiler->enclosing->locals[local].isMutable);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false,
                      compiler->enclosing->upvalues[upvalue].isMutable);
  }

  return -1;
}

static void addLocal(Token name, bool isMutable) {
  if (current->localCount == UINT8_COUNT) {
    errorAtToken(&name, "Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;

  local->isCaptured = false;
  local->isMutable = isMutable;
}

static void declareVariable(Token *name, bool isMutable) {
  if (current->scopeDepth == 0)
    return;

  addLocal(*name, isMutable);
}

/**
 * Names of globals declared with `let`.
 *
 * Globals have no compile-time slot, so immutability is tracked by name in a
 * table that outlives a single compile() call -- it persists for the life of
 * a compiler session (see compilerSessionBegin()/compilerSessionEnd() below),
 * since the REPL compiles each line separately but shares one set of
 * globals across the whole session.
 *
 * This is a plain (non-GC) set of names: the compiler must not allocate heap
 * objects.
 */
typedef struct {
  char **names;
  int *lengths;
  int count;
  int capacity;
} NameSet;

static void nameSetFree(NameSet *set);

static NameSet immutableGlobals;

/**
 * End a compiler session, freeing everything tracked during it.
 */
void compilerSessionEnd(void) { nameSetFree(&immutableGlobals); }

static bool nameSetContains(NameSet *set, const char *chars, int length) {
  for (int i = 0; i < set->count; i++) {
    if (set->lengths[i] == length &&
        memcmp(set->names[i], chars, (size_t)length) == 0) {
      return true;
    }
  }
  return false;
}

static void nameSetAdd(NameSet *set, const char *chars, int length) {
  if (nameSetContains(set, chars, length)) {
    return;
  }
  if (set->count + 1 > set->capacity) {
    set->capacity = set->capacity < 8 ? 8 : set->capacity * 2;
    set->names = realloc(set->names, sizeof(char *) * set->capacity);
    set->lengths = realloc(set->lengths, sizeof(int) * set->capacity);
    if (set->names == NULL || set->lengths == NULL) {
      fprintf(stderr, "realloc failed in nameSetAdd");
      exit(EXIT_CODE_OS_ERR);
    }
  }
  char *copy = malloc((size_t)length);
  memcpy(copy, chars, (size_t)length);
  set->names[set->count] = copy;
  set->lengths[set->count] = length;
  set->count++;
}

static void nameSetFree(NameSet *set) {
  for (int i = 0; i < set->count; i++) {
    free(set->names[i]);
  }
  free(set->names);
  free(set->lengths);
  set->names = NULL;
  set->lengths = NULL;
  set->count = 0;
  set->capacity = 0;
}

static void markGlobalImmutable(Token *name) {
  nameSetAdd(&immutableGlobals, name->start, name->length);
}

static bool isGlobalImmutable(Token *name) {
  return nameSetContains(&immutableGlobals, name->start, name->length);
}

// Resolves `name` as a local, then an upvalue, then falls back to treating
// it as a global
static VarRef resolveVariable(Token *name) {
  VarRef ref;

  int arg = resolveLocal(current, name);

  if (arg != -1) {
    ref.getOp = OP_GET_LOCAL;
    ref.setOp = OP_SET_LOCAL;
    ref.arg = (uint8_t)arg;
    ref.isMutable = current->locals[arg].isMutable;
  } else if ((arg = resolveUpvalue(current, name)) != -1) {
    ref.getOp = OP_GET_UPVALUE;
    ref.setOp = OP_SET_UPVALUE;
    ref.arg = (uint8_t)arg;
    ref.isMutable = current->upvalues[arg].isMutable;
  } else {
    ref.arg = identifierConstant(name);
    ref.getOp = OP_GET_GLOBAL;
    ref.setOp = OP_SET_GLOBAL;
    ref.isMutable = !isGlobalImmutable(name);
  }

  return ref;
}

static uint8_t parseVariableFromToken(Token *name, bool isMutable) {
  declareVariable(name, isMutable);
  if (current->scopeDepth > 0)
    return 0;

  currentLine = name->line;

  // identifierConstant() interns the name into the chunk's constant table,
  // which keeps it reachable for the GC before it is stored below.
  uint8_t constant = identifierConstant(name);
  if (!isMutable)
    markGlobalImmutable(name);
  return constant;
}

/**
 * Initialize a new local variable
 */
static void markInitialized(void) {
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

static void emitByte(uint8_t byte) {
  cuWriteByte(currentFn(), byte, currentLine);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentFn()->codeCount - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
  TRACELN("  compiler.emitJump()");

  emitByte(instruction);
  emitBytes(0xff, 0xff);
  return currentFn()->codeCount - 2;
}

static void patchJump(int offset) {
  int jump = currentFn()->codeCount - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  currentFn()->code[offset] = (jump >> 8) & 0xff;
  currentFn()->code[offset + 1] = jump & 0xff;
}

static uint8_t makeConstant(CompiledConst value, Token *tok) {
  int constant = cuAddConstant(currentFn(), value);

  if (constant > UINT8_MAX) {
    if (tok != NULL) {
      errorAtToken(tok, "Too many constants in one chunk.");
    } else {
      error("Too many constants in one chunk.");
    }
    return 0;
  }

  return (uint8_t)constant;
}

static uint8_t stringConstant(const char *chars, int length, Token *tok) {
  CompiledConst k;
  k.kind = CONST_STRING;
  k.as.string.offset = cuInternString(compilingUnit, chars, length);
  k.as.string.length = length;
  return makeConstant(k, tok);
}

static void emitNumberConstant(double number) {
  CompiledConst k;
  k.kind = CONST_NUMBER;
  k.as.number = number;
  emitBytes(OP_CONSTANT, makeConstant(k, NULL));
}

static void emitStringConstant(const char *chars, int length) {
  emitBytes(OP_CONSTANT, stringConstant(chars, length, NULL));
}

static void beginScope(void) { current->scopeDepth++; }

/**
 * Called at the end of a scope
 *
 * This either pops locals off the stack or makes them upvalues if they were
 * captured by a lambda
 */
static void captureOrCleanLocalsGoingOutOfScope(void) {
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {

    // now reads the NEXT local down
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }

    current->localCount--;
  }
}

static void endScope(void) {
  current->scopeDepth--;

  captureOrCleanLocalsGoingOutOfScope();
}

/**
 * Used for loop breaks
 *
 * Emits the pops needed to discard every local declared deeper than
 * `targetDepth`
 *
 * This leaves the current->localCount or scopeDepth unmodified;
 *
 * A break jumpsout of the loop from the middle of a scope that is still
 * open, so the locals must be popped at runtime while remaining declared
 * at compile time (statements after the `break` are still in that scope
 * and still address those slots). Mutating localCount here would corrupt
 * every subsequent slot index in the loop body.
 *
 * It emits one opcode per local, OP_CLOSE_UPVALUE for captured ones
 * but walks a local copy of the index instead of mutating the compiler's.
 */
static void emitPopsToDepth(int targetDepth) {
  int i = current->localCount;

  while (i > 0 && current->locals[i - 1].depth > targetDepth) {
    if (current->locals[i - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }

    i--;
  }
}

/**
 * Closes the scope for a block used in EXPRESSION position
 *
 * Counts how many locals are going out of scope and emits a single
 * OP_CLOSE_BLOCK_EXPR, which pops that many slots *below* the block's
 * result value while leaving the result itself on top of the stack.
 */
static void compileBlockExprClose(void) {
  current->scopeDepth--;

  int locals = 0;
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    locals++;
    current->localCount--;
  }

  emitBytes(OP_CLOSE_BLOCK_EXPR, (uint8_t)locals);
}

static void emitImplicitReturn(void) {
  TRACELN("  compiler.emitImplicitReturn()");

  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

static void emitValueReturn(void) {
  TRACELN("  compiler.emitValueReturn()");

  emitByte(OP_RETURN);
}

static int endCompiler(void) {
  TRACELN("  compiler.endCompiler()");

  currentLoop = current->enclosingLoop;

  if (previousOpCode() != OP_RETURN) {
    emitImplicitReturn();
  }

  current->fn->upvalueCount = current->upvalueCount;
  int index = current->fnIndex;

  current = current->enclosing;

  return index;
}

static void compileCall(CallNode *c) {
  // Compile method calls to OP_INVOKE instead of OP_GET_PROPERTY -> OP_CALL
  if (c->callee->kind == NODE_GET) {
    GetNode *g = &c->callee->as.get;
    compileExpr(g->object);
    uint8_t name = identifierConstant(&g->name);
    for (int i = 0; i < c->argCount; i++) {
      compileExpr(c->args[i]);
    }
    emitBytes(OP_INVOKE, name);
    emitByte((uint8_t)c->argCount);
    return;
  }

  compileExpr(c->callee);
  for (int i = 0; i < c->argCount; i++) {
    compileExpr(c->args[i]);
  }
  emitBytes(OP_CALL, (uint8_t)c->argCount);
}

/**
 * Is there a receiver in scope for `self` to refer to?
 *
 * True inside an instance method, and inside any closure nested within one
 * (where `self` is reached as an upvalue). False at the top level, inside a
 * plain function, and inside a static method.
 */
static bool selfInScope(void) {
  for (FnCompiler *c = current; c != NULL; c = c->enclosing) {
    if (c->type == TYPE_METHOD)
      return true;
  }
  return false;
}

static void compileExpr(AstNode *node) {
  if (node == NULL)
    return;

  currentLine = node->line;

  switch (node->kind) {
  case NODE_LITERAL: {
    LiteralNode *literal = &node->as.literal;

    switch (literal->kind) {
    case LITERAL_NIL:
      emitByte(OP_NIL);
      break;
    case LITERAL_BOOL:
      emitByte(literal->as.boolean ? OP_TRUE : OP_FALSE);
      break;
    case LITERAL_NUMBER:
      emitNumberConstant(literal->as.number);
      break;
    case LITERAL_STRING:
      emitStringConstant(literal->as.string.chars, literal->as.string.length);
      break;
    }

    break;
  }

  case NODE_UNARY: {
    UnaryNode *u = &node->as.unary;
    compileExpr(u->operand);
    switch (u->op.type) {
    case TOKEN_BANG:
      emitByte(OP_NOT);
      break;
    case TOKEN_MINUS:
      emitByte(OP_NEGATE);
      break;
    default:
      break; // unreachable
    }
    break;
  }

  case NODE_BINARY: {
    BinaryNode *b = &node->as.binary;
    compileExpr(b->left);
    compileExpr(b->right);
    switch (b->op.type) {
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
      break; // unreachable
    }
    break;
  }

  case NODE_GROUPING:
    compileExpr(node->as.grouping.inner);
    break;

  case NODE_VARIABLE: {
    VarRef ref = resolveVariable(&node->as.variable.name);
    emitBytes(ref.getOp, ref.arg);
    break;
  }

  case NODE_ASSIGN: {
    AssignNode *a = &node->as.assign;
    VarRef ref = resolveVariable(&a->name);
    if (!ref.isMutable) {
      errorAtToken(&a->name, "Cannot assign to immutable binding");
    }
    compileExpr(a->value);
    emitBytes(ref.setOp, ref.arg);
    break;
  }

  case NODE_AND: {
    LogicalNode *l = &node->as.logical;
    compileExpr(l->left);
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    compileExpr(l->right);
    patchJump(endJump);
    break;
  }

  case NODE_OR: {
    LogicalNode *l = &node->as.logical;
    compileExpr(l->left);
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);
    patchJump(elseJump);
    emitByte(OP_POP);
    compileExpr(l->right);
    patchJump(endJump);
    break;
  }

  case NODE_NULLISH: {
    LogicalNode *l = &node->as.logical;
    compileExpr(l->left);
    int endJump = emitJump(OP_JUMP_IF_NOT_NIL);
    emitByte(OP_POP);
    compileExpr(l->right);
    patchJump(endJump);
    break;
  }

  case NODE_CALL:
    compileCall(&node->as.call);
    break;

  case NODE_STRUCT_INIT: {
    StructInitNode *si = &node->as.structInit;

    VarRef ref = resolveVariable(&si->name);
    emitBytes(ref.getOp, ref.arg);

    for (int i = 0; i < si->fieldCount; i++) {
      StructInitFieldNode *field = &si->fields[i];

      currentLine = field->name.line;
      emitBytes(OP_CONSTANT, identifierConstant(&field->name));

      compileExpr(field->value);
    }

    currentLine = si->endLine;
    emitBytes(OP_STRUCT_INIT, (uint8_t)si->fieldCount);
    break;
  }

  case NODE_GET: {
    GetNode *g = &node->as.get;
    compileExpr(g->object);
    uint8_t name = identifierConstant(&g->name);
    emitBytes(OP_GET_PROPERTY, name);
    break;
  }

  case NODE_SET: {
    SetNode *s = &node->as.set;
    compileExpr(s->object);
    uint8_t name = identifierConstant(&s->name);
    compileExpr(s->value);
    emitBytes(OP_SET_PROPERTY, name);
    break;
  }

  case NODE_SELF: {
    if (!selfInScope()) {
      if (current->type == TYPE_STATIC_METHOD) {
        errorAtToken(&node->as.self_.keyword,
                     "Can't use 'self' in a static method. Add 'self' as the "
                     "first parameter to make it an instance method.");
      } else {
        errorAtToken(&node->as.self_.keyword,
                     "Can't use 'self' outside of an instance method.");
      }
      emitByte(OP_NIL);
      break;
    }
    VarRef ref = resolveVariable(&node->as.self_.keyword);
    emitBytes(ref.getOp, ref.arg);
    break;
  }

  case NODE_INDEX_GET: {
    IndexGetNode *ig = &node->as.indexGet;
    compileExpr(ig->object);
    compileExpr(ig->index);
    emitByte(OP_GET_INDEX);
    break;
  }

  case NODE_INDEX_SET: {
    IndexSetNode *is = &node->as.indexSet;
    compileExpr(is->object);
    compileExpr(is->index);
    compileExpr(is->value);
    emitByte(OP_SET_INDEX);
    break;
  }

  case NODE_ARRAY: {
    ArrayNode *arr = &node->as.array;
    if (arr->count > UINT8_MAX) {
      errorAtNode(node, "Too many elements in array literal.");
    }
    for (int i = 0; i < arr->count; i++) {
      compileExpr(arr->items[i]);
    }
    emitBytes(OP_ARRAY, (uint8_t)arr->count);
    break;
  }

  // A lambda expression, e.g. `var f = fun (x) { x };`
  //
  // Compiles the closure and leaves it on the stack
  case NODE_FUNCTION:
    compileFunction(&node->as.function, TYPE_FUNCTION);
    break;

  case NODE_IF: {
    IfNode *f = &node->as.if_;
    compileExpr(f->condition);

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    compileExpr(f->thenBranch);

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (f->elseBranch != NULL) {
      compileExpr(f->elseBranch);
    } else {
      emitByte(OP_NIL);
    }

    patchJump(elseJump);
    break;
  }

  // A block used as an EXPRESSION, e.g. `var x = { foo(); 5 };`.
  case NODE_BLOCK: {
    beginScope();
    BlockNode *blk = &node->as.block;
    for (int i = 0; i < blk->count; i++) {
      compileStmt(blk->stmts[i]);
    }
    if (blk->value != NULL) {
      compileExpr(blk->value);
    } else {
      currentLine = blk->endLine;
      emitByte(OP_NIL);
    }

    currentLine = blk->endLine;
    compileBlockExprClose();
    break;
  }

  default:
    errorAtNode(node, "Internal error: not a valid expression node.");
    break;
  }
}

static void compileVarDecl(AstNode *node) {
  VarDeclNode *vd = &node->as.varDecl;
  uint8_t global = parseVariableFromToken(&vd->name, vd->isMutable);

  if (vd->initializer != NULL) {
    compileExpr(vd->initializer);
  } else {
    currentLine = vd->name.line;
    emitByte(OP_NIL);
  }
  currentLine = vd->declEndLine;
  defineVariable(global);
}

static void compileFunction(FunctionNode *fn, FunctionType type) {
  FnCompiler compiler;
  initCompiler(&compiler, type, fn->isLambda ? NULL : &fn->name);
  current->fn->isPublic = fn->isPublic;
  beginScope();

  current->fn->arity = fn->arity;
  if (fn->arity > 255) {
    errorAtToken(&fn->name, "Can't have more than 255 parameters.");
  }
  for (int i = 0; i < fn->arity; i++) {
    declareVariable(&fn->params[i], /*isMutable=*/true);
    markInitialized();
  }

  if (fn->exprBody != NULL) {
    compileExpr(fn->exprBody);
    emitValueReturn();
  } else {
    compileBlockContents(&fn->body);
  }

  currentLine = fn->bodyEndLine;

  int compiledIndex = endCompiler();
  int compiledUpvalueCount =
      cuGetFnByIndex(compilingUnit, compiledIndex)->upvalueCount;

  CompiledConst k;
  k.kind = CONST_FUNCTION;
  k.as.functionIndex = compiledIndex;
  emitBytes(OP_CLOSURE, makeConstant(k, NULL));

  for (int i = 0; i < compiledUpvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void compileFunctionDeclStmt(AstNode *node) {
  FunctionNode *fn = &node->as.function;
  uint8_t global = parseVariableFromToken(&fn->name, /*isMutable=*/true);
  // Marked initialized *before* compiling the body so the function can call
  // itself recursively through its own local slot.
  markInitialized();
  compileFunction(fn, TYPE_FUNCTION);
  defineVariable(global);
}

static void compileStructDecl(AstNode *node) {
  StructNode *sn = &node->as.struct_;

  uint8_t nameConstant = identifierConstant(&sn->name);
  declareVariable(&sn->name, /*isMutable=*/true);
  emitBytes(OP_STRUCT, nameConstant);
  defineVariable(nameConstant);

  // Push the struct back onto the stack so the fields can be bound to it
  VarRef ref = resolveVariable(&sn->name);
  emitBytes(ref.getOp, ref.arg);

  for (int i = 0; i < sn->fieldCount; i++) {
    VarDeclNode *field = &sn->fields[i];

    currentLine = field->name.line;
    uint8_t constant = identifierConstant(&field->name);

    if (field->initializer != NULL) {
      compileExpr(field->initializer);
    } else {
      emitByte(OP_NIL);
    }

    currentLine = field->declEndLine;
    emitBytes(OP_FIELD, constant);
    emitByte(field->isPublic ? 1 : 0);
  }

  currentLine = sn->endLine;
  emitByte(OP_POP);
}

static void compileImplDecl(AstNode *node) {
  ImplNode *in = &node->as.impl;

  currentLine = in->name.line;

  // Push the struct onto the stack so the methods can be bound to it
  VarRef ref = resolveVariable(&in->name);
  emitBytes(ref.getOp, ref.arg);

  for (int i = 0; i < in->methodCount; i++) {
    FunctionNode *method = in->methods[i];
    uint8_t constant = identifierConstant(&method->name);

    FunctionType type = method->hasSelf ? TYPE_METHOD : TYPE_STATIC_METHOD;

    compileFunction(method, type);
    emitBytes(OP_METHOD, constant);
  }

  currentLine = in->endLine;
  emitByte(OP_POP); // pop the struct off the stack
}

static void compileReturn(AstNode *node) {
  ReturnNode *r = &node->as.return_;

  if (current->type == TYPE_SCRIPT) {
    errorAtNode(node, "Can't return from top-level code.");
  }

  if (r->value == NULL) {
    emitByte(OP_NIL);
  } else {
    compileExpr(r->value);
  }

  emitValueReturn();
}

static void compileBreak(AstNode *node) {
  if (currentLoop == NULL) {
    errorAtNode(node, "Can't use 'break' outside of a loop.");
    return;
  }

  if (currentLoop->breakCount == UINT8_COUNT) {
    errorAtNode(node, "Too many breaks in one loop.");
    return;
  }

  emitPopsToDepth(currentLoop->scopeDepth); // non-destructive
  currentLoop->breakJumps[currentLoop->breakCount++] = emitJump(OP_JUMP);
}

/**
 * @param int The scope depth when entering the loop. This is what the break
 * statement will jump to.
 */
static void beginLoop(LoopCompiler *loop, int scopeDepth) {
  loop->enclosing = currentLoop;
  loop->scopeDepth = scopeDepth;
  loop->breakCount = 0;
  currentLoop = loop;
}

/**
 * Patches every `break` in this loop to jump here, then pops the loop.
 */
static void endLoop(void) {
  for (int i = 0; i < currentLoop->breakCount; i++) {
    patchJump(currentLoop->breakJumps[i]);
  }

  currentLoop = currentLoop->enclosing;
}

static void compileIf(AstNode *node) {
  IfNode *f = &node->as.if_;

  compileExpr(f->condition);

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  compileStmt(f->thenBranch);

  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP);

  if (f->elseBranch != NULL) {
    compileStmt(f->elseBranch);
  }

  patchJump(elseJump);
}

static void compileWhile(AstNode *node) {
  WhileNode *w = &node->as.while_;

  int loopStart = currentFn()->codeCount;

  compileExpr(w->condition);

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  LoopCompiler loop;
  beginLoop(&loop, current->scopeDepth);

  compileStmt(w->body);

  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);

  endLoop();
}

static void compileFor(AstNode *node) {
  ForNode *f = &node->as.for_;

  int outerDepth = current->scopeDepth;

  if (f->init != NULL) {
    beginScope();
    compileStmt(f->init);
  }

  int loopStart = currentFn()->codeCount;

  compileExpr(f->condition);

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  if (f->increment != NULL) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentFn()->codeCount;
    compileExpr(f->increment);
    emitByte(OP_POP);
    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  LoopCompiler loop;
  beginLoop(&loop, outerDepth);

  compileStmt(f->body);

  emitLoop(loopStart);

  patchJump(exitJump);
  emitByte(OP_POP);

  if (f->init != NULL) {
    endScope();
  }

  endLoop();
}

// Used for function declarations (where an implicit return is used)
// and block statements (where an implicit return is disguarded)
static void compileBlockContents(BlockNode *block) {
  for (int i = 0; i < block->count; i++) {
    compileStmt(block->stmts[i]);
  }

  if (block->value != NULL) {
    compileExpr(block->value);

    if (current->type == TYPE_SCRIPT) {
      errorAtNode(block->value, "Expect ';' after expression.");
    } else {
      emitValueReturn();
    }
  }
}

static void compileStmt(AstNode *node) {
  if (node == NULL)
    return;

  currentLine = node->line;

  switch (node->kind) {
  case NODE_EXPR_STMT:
    compileExpr(node->as.exprStmt.expr);
    emitByte(OP_POP);
    break;

  case NODE_PRINT:
    compileExpr(node->as.print.expr);
    emitByte(OP_PRINT);
    break;

  case NODE_VAR_DECL:
    compileVarDecl(node);
    break;

  case NODE_BLOCK:
    beginScope();
    compileBlockContents(&node->as.block);
    currentLine = node->as.block.endLine;
    endScope();
    break;

  case NODE_IF:
    compileIf(node);
    break;

  case NODE_WHILE:
    compileWhile(node);
    break;

  case NODE_FOR:
    compileFor(node);
    break;

  case NODE_RETURN:
    compileReturn(node);
    break;

  case NODE_BREAK:
    compileBreak(node);
    break;

  case NODE_FUNCTION:
    compileFunctionDeclStmt(node);
    break;

  case NODE_STRUCT:
    compileStructDecl(node);
    break;

  case NODE_IMPL:
    compileImplDecl(node);
    break;

  default:
    errorAtNode(node, "Internal error: not a valid statement node.");
    break;
  }
}

CompiledUnit *compile(AstNode **ast, int count, int endLine) {
  TRACELN("  compiler.compile()");

  CompiledUnit *unit = malloc(sizeof(CompiledUnit));
  if (unit == NULL) {
    fprintf(stderr, "malloc failed in compile");
    exit(EXIT_CODE_OS_ERR);
  }
  cuInit(unit);

  compilingUnit = unit;
  hadError = false;

  FnCompiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT, NULL);

  // Hoist top-level function declarations.
  for (int i = 0; i < count; i++) {
    if (ast[i]->kind == NODE_FUNCTION) {
      compileStmt(ast[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (ast[i]->kind != NODE_FUNCTION) {
      compileStmt(ast[i]);
    }
  }

  currentLine = endLine;

  endCompiler();

  bool ok = !hadError;
  compilingUnit = NULL;

  if (!ok) {
    freeCompiledUnit(unit);
    free(unit);
    return NULL;
  }

  return unit;
}

bool compilerIsActive(void) { return current != NULL; }
