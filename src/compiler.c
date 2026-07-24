#include "compiler.h"

#include <stdio.h>
#include <string.h>

#include "ast.h"
#include "chunk.h"
#include "common.h"
#include "memory.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Compiler *current = NULL;
ClassCompiler *currentClass = NULL;
LoopCompiler *currentLoop = NULL;

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
} VarRef;

static void compileExpr(AstNode *node);
static void compileStmt(AstNode *node);
static void compileBlockContents(BlockNode *block);
static void compileFunction(FunctionNode *fn, FunctionType type);
static uint8_t makeConstant(Value value, Token *tok);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static void declareVariable(Token *name);
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
static void initCompiler(Compiler *compiler, FunctionType functionType,
                         Token *nameToken) {
  TRACELN("  compiler.initCompiler()");

  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = functionType;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  compiler->enclosingLoop = currentLoop;
  current = compiler;

  currentLoop = NULL;

  if (nameToken != NULL) {
    current->function->name = copyString(nameToken->start, nameToken->length);
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
    current->function->name = copyString(lambdaName, len);
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

/**
 * Get the current chunk being compiled
 */
static Chunk *currentChunk(void) { return &current->function->chunk; }

/**
 * Get the last emitted opcode
 */
static uint8_t previousOpCode(void) {
  Chunk *chunk = currentChunk();

  if (chunk->count == 0) {
    return 0;
  }

  return chunk->code[chunk->count - 1];
}

/**
 * Make a new constant from an identifier token
 */
static uint8_t identifierConstant(Token *identifier) {
  return makeConstant(
      OBJ_VAL(copyString(identifier->start, identifier->length)), identifier);
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
static int resolveLocal(Compiler *compiler, Token *identifier) {
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
    errorAtToken(&name, "Too many local variables in function.");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;

  local->isCaptured = false;
}

static void declareVariable(Token *name) {
  if (current->scopeDepth == 0)
    return;
  addLocal(*name);
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
  } else if ((arg = resolveUpvalue(current, name)) != -1) {
    ref.getOp = OP_GET_UPVALUE;
    ref.setOp = OP_SET_UPVALUE;
    ref.arg = (uint8_t)arg;
  } else {
    ref.arg = identifierConstant(name);
    ref.getOp = OP_GET_GLOBAL;
    ref.setOp = OP_SET_GLOBAL;
  }

  return ref;
}

static uint8_t parseVariableFromToken(Token *name) {
  declareVariable(name);
  if (current->scopeDepth > 0)
    return 0;

  currentLine = name->line;
  return identifierConstant(name);
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
  writeChunk(currentChunk(), byte, currentLine);
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

static void patchJump(int offset) {
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static uint8_t makeConstant(Value value, Token *tok) {
  int constant = addConstantToChunk(currentChunk(), value);

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

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value, NULL));
}

static void beginScope(void) { current->scopeDepth++; }

/**
 * TODO
 */
static void handleLocalsInScope(void) {
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

  handleLocalsInScope();
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

  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

  emitByte(OP_RETURN);
}

static void emitValueReturn(void) {
  TRACELN("  compiler.emitValueReturn()");

  if (current->type == TYPE_INITIALIZER) {
    // Disreguard the return value.
    emitByte(OP_POP);
    // Return 'this' instead.
    emitBytes(OP_GET_LOCAL, 0);
  }

  emitByte(OP_RETURN);
}

static ObjFunction *endCompiler(void) {
  TRACELN("  compiler.endCompiler()");

  currentLoop = current->enclosingLoop;

  if (previousOpCode() != OP_RETURN) {
    emitImplicitReturn();
  }

  ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
  if (!hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                         ? function->name->chars
                                         : "<script>");
  }
#endif

  current = current->enclosing;

  return function;
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

static void compileExpr(AstNode *node) {
  if (node == NULL)
    return;

  currentLine = node->line;

  switch (node->kind) {
  case NODE_LITERAL: {
    Value v = node->as.literal.value;
    if (IS_NIL(v)) {
      emitByte(OP_NIL);
    } else if (IS_BOOL(v)) {
      emitByte(AS_BOOL(v) ? OP_TRUE : OP_FALSE);
    } else {
      emitConstant(v);
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

  case NODE_THIS: {
    if (currentClass == NULL) {
      errorAtToken(&node->as.this_.keyword,
                   "Can't use 'this' outside of a class.");
      emitByte(OP_NIL);
      break;
    }
    VarRef ref = resolveVariable(&node->as.this_.keyword);
    emitBytes(ref.getOp, ref.arg);
    break;
  }

  // TODO: Remove this
  case NODE_SUPER:
    errorAtToken(&node->as.super_.keyword, "Superclasses aren't supported.");
    emitByte(OP_NIL);
    break;

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
  uint8_t global = parseVariableFromToken(&vd->name);

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
  Compiler compiler;
  initCompiler(&compiler, type, fn->isLambda ? NULL : &fn->name);
  beginScope();

  current->function->arity = fn->arity;
  if (fn->arity > 255) {
    errorAtToken(&fn->name, "Can't have more than 255 parameters.");
  }
  for (int i = 0; i < fn->arity; i++) {
    declareVariable(&fn->params[i]);
    markInitialized();
  }

  if (fn->exprBody != NULL) {
    compileExpr(fn->exprBody);
    emitValueReturn();
  } else {
    compileBlockContents(&fn->body);
  }

  currentLine = fn->bodyEndLine;

  ObjFunction *compiled = endCompiler();

  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(compiled), NULL));

  for (int i = 0; i < compiled->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void compileFunctionDeclStmt(AstNode *node) {
  FunctionNode *fn = &node->as.function;
  uint8_t global = parseVariableFromToken(&fn->name);
  // Marked initialized *before* compiling the body so the function can call
  // itself recursively through its own local slot.
  markInitialized();
  compileFunction(fn, TYPE_FUNCTION);
  defineVariable(global);
}

static void compileClassDecl(AstNode *node) {
  ClassNode *cn = &node->as.class_;

  if (cn->superclass != NULL) {
    errorAtToken(cn->superclass, "Superclasses aren't supported.");
  }

  uint8_t nameConstant = identifierConstant(&cn->name);
  declareVariable(&cn->name);
  emitBytes(OP_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  classCompiler.fieldCount =
      0; // TODO: unused -- nothing reads ClassCompiler.fields
  currentClass = &classCompiler;

  // Push the class back onto the stack so fields/methods can be bound to it
  VarRef ref = resolveVariable(&cn->name);
  emitBytes(ref.getOp, ref.arg);

  for (int i = 0; i < cn->memberCount; i++) {
    ClassMember *m = &cn->members[i];

    if (m->kind == CLASS_MEMBER_FIELD) {
      currentLine = m->as.field.name.line;
      uint8_t constant = identifierConstant(&m->as.field.name);

      if (m->as.field.initializer != NULL) {
        compileExpr(m->as.field.initializer);
      } else {
        emitByte(OP_NIL);
      }

      currentLine = m->as.field.declEndLine;
      emitBytes(OP_FIELD, constant);
    } else {
      FunctionNode *method = m->as.method;
      uint8_t constant = identifierConstant(&method->name);

      FunctionType type = TYPE_METHOD;

      if (method->name.length == 4 &&
          memcmp(method->name.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
      }

      compileFunction(method, type);
      emitBytes(OP_METHOD, constant);
    }
  }

  currentLine = cn->endLine;
  emitByte(OP_POP); // pop the class off the stack

  currentClass = currentClass->enclosing;
}

static void compileReturn(AstNode *node) {
  ReturnNode *r = &node->as.return_;

  if (current->type == TYPE_SCRIPT) {
    errorAtNode(node, "Can't return from top-level code.");
  }

  if (r->value == NULL) {
    emitByte(OP_NIL);
  } else {
    if (current->type == TYPE_INITIALIZER) {
      errorAtNode(node, "Can't return a value from an initializer.");
    }
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

  int loopStart = currentChunk()->count;

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

  int loopStart = currentChunk()->count;

  compileExpr(f->condition);

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);

  if (f->increment != NULL) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
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

    // TODO: Matches the original quirk exactly: this implicit-return path emits
    // a bare OP_RETURN, *not* emitValueReturn() -- so (like the original) it
    // does not special-case TYPE_INITIALIZER the way an explicit
    // `return expr;` does. A tail expression is only valid inside some
    // function; at true top-level (TYPE_SCRIPT) it's the same error the
    // original raised at compile time for a dangling expression.
    if (current->type == TYPE_SCRIPT) {
      errorAtNode(block->value, "Expect ';' after expression.");
    } else {
      emitByte(OP_RETURN);
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

  case NODE_CLASS:
    compileClassDecl(node);
    break;

  default:
    errorAtNode(node, "Internal error: not a valid statement node.");
    break;
  }
}

ObjFunction *compile(AstNode **ast, int count, int endLine) {
  TRACELN("  compiler.compile()");

  hadError = false;

  Compiler compiler;
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

  ObjFunction *function = endCompiler();

  return hadError ? NULL : function;
}

void markCompilerRoots(void) {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Obj *)compiler->function);
    compiler = compiler->enclosing;
  }
}
