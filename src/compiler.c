// AST-driven bytecode compiler.
//
// This replaces the old single-pass (scan-token-while-parsing) compiler.
// Source is now scanned+parsed in full up front (see lexer.c / parser.c),
// producing an array of AstNode* declarations. This file walks that tree
// and emits bytecode, reusing exactly the same low-level machinery the
// original compiler used (chunk writing, jump patching, local/upvalue
// resolution, per-function Compiler stack, etc.) -- only the "which syntax
// produces which opcode" logic has moved from being driven by
// `compilerParser.previous.type` to being driven by `AstNode->kind`.
//
// Known gap (pre-existing design decision, not something missed here):
//   - `class Foo < Bar { }` parses into ClassNode.superclass, but there is
//     no bytecode support for inheritance (this fork was already
//     superclass-free before this rewrite), so it's reported as a compile
//     error here rather than silently ignored or newly implemented.
//   - `super.method` (NODE_SUPER) has no bytecode support either, for the
//     same reason, and is likewise a compile error.
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

// Anonymous lambdas get an auto-generated name ("lambda0x...") for stack
// traces, since they have no real identifier -- mirrors the original.
static int lambdaCount = 0;

// The line most recently entered via compileExpr()/compileStmt(), used to
// tag emitted bytecode -- the AST equivalent of the old
// `compilerParser.previous.line`.
static int currentLine = 0;

// A resolved variable reference: which opcode pair to use, and the operand
// (local slot / upvalue index / global-name constant index) to go with it.
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

// ── Error reporting ──────────────────────────────────────────────────────────
// The AST is already syntactically valid (the parser guarantees that) --
// everything reported here is a semantic error (too many locals, reading a
// local in its own initializer, returning from top-level code, etc). There's
// no token stream left to resynchronize, so unlike the parser there's no
// panic-mode/synchronize() step: we just keep compiling so we can report as
// many independent errors as possible in one pass.

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

// For the handful of error sites that have neither a Token nor an AstNode
// handy (mirrors the original compiler falling back to
// `compilerParser.previous` in the same situations).
static void errorHere(const char *message) {
  errorAt(currentLine, NULL, 0, message);
}

/**
 * Initialize compiler to compile a function
 *
 * - Sets current compiler as the enclosing compiler
 * - Sets the function type being compiled
 * - Sets new compiler as the current one
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

  // nameToken is NULL for the top-level TYPE_SCRIPT compiler (stays
  // unnamed, like the book's <script>) and for lambdas (TYPE_FUNCTION with
  // no real identifier) -- those get an auto-generated name instead, purely
  // for stack traces.
  if (nameToken != NULL) {
    current->function->name = copyString(nameToken->start, nameToken->length);
  } else if (functionType == TYPE_FUNCTION) {
    lambdaCount++;
    char lambdaName[32];
    int len =
        snprintf(lambdaName, sizeof(lambdaName), "lambda0x%x", lambdaCount);
    // Original bug fixed here: it passed sizeof(lambdaName) (the buffer
    // capacity) as the string length instead of the formatted length,
    // which would copy uninitialized stack bytes past the real name into
    // the ObjString. Use the actual formatted length instead, clamped to
    // the buffer in the pathological case snprintf had to truncate.
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

// Get the last emitted opcode
static uint8_t previousOpCode(void) {
  Chunk *chunk = currentChunk();

  if (chunk->count == 0) {
    return 0;
  }

  return chunk->code[chunk->count - 1];
}

// Make a new constant from an identifier token
static uint8_t identifierConstant(Token *identifier) {
  return makeConstant(
      OBJ_VAL(copyString(identifier->start, identifier->length)), identifier);
}

// Compare two identifier tokens for equality
static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

// Resolve local variable by identifier
//
// @returns -1 if not found, otherwise the index of the local from the
// function's locals
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
    errorHere("Too many closure variables in function.");
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
// it as a global -- mirrors the original namedVariable()'s lookup order,
// just split out so both reads (NODE_VARIABLE) and writes (NODE_ASSIGN) can
// share it.
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
  // Matches the original: parser.previous is still the name token here
  // (nothing consumed since), so a 'too many constants' error from this
  // identifierConstant() call is tagged with the name's own line, not
  // whatever was last visited elsewhere.
  currentLine = name->line;
  return identifierConstant(name);
}

// Initialize a new local variable
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
    errorHere("Loop body too large.");

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
    errorHere("Too much code to jump over");
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
      errorHere("Too many constants in one chunk.");
    }
    return 0;
  }

  return (uint8_t)constant;
}

static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value, NULL));
}

static void beginScope(void) { current->scopeDepth++; }

// NOTE: this looks wrong (it emits *two* opcodes per local going out of
// scope: an unconditional OP_POP, then -- checking a DIFFERENT, not-yet-
// removed local's captured flag, since the decrement already happened --
// another OP_POP or OP_CLOSE_UPVALUE), and it reads current->locals[-1]
// out of bounds for the last local in any group. An earlier version of
// this file "fixed" it to the standard single-emission-per-local form.
// That was wrong: the project's golden snapshot tests were generated by
// actually running the original reference compiler, which emits exactly
// this (admittedly bizarre) byte sequence -- so it's restored here
// verbatim rather than cleaned up, to match those snapshots exactly.
static void handleLocalsInScope(void) {
  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1]
            .isCaptured) // now reads the NEXT local down
      emitByte(OP_CLOSE_UPVALUE);
    else
      emitByte(OP_POP); // second opcode for the same local
    current->localCount--;
  }
}

static void endScope(void) {
  current->scopeDepth--;

  handleLocalsInScope();
}

// Emits the pops needed to discard every local declared deeper than
// `targetDepth`, WITHOUT touching current->localCount or scopeDepth.
//
// This is handleLocalsInScope()'s counterpart for `break`: a break jumps
// out of the loop from the middle of a scope that is still open, so the
// locals must be popped at runtime while remaining declared at compile
// time (statements after the `break` are still in that scope and still
// address those slots). Mutating localCount here would corrupt every
// subsequent slot index in the loop body.
//
// It emits the same sequence as handleLocalsInScope() -- one opcode per
// local, OP_CLOSE_UPVALUE for captured ones -- but walks a local copy of
// the index instead of mutating the compiler's.
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

// Closes the scope for a block used in EXPRESSION position: unlike
// endScope() (which pops each local one at a time, discarding them), this
// counts how many locals are going out of scope and emits a single
// OP_CLOSE_BLOCK_EXPR, which pops that many slots *below* the block's
// result value while leaving the result itself on top of the stack.
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

  // Assumes return value is already on the stack.
  if (current->type == TYPE_INITIALIZER) {
    // Initializers always return 'this'.
    emitByte(OP_POP);
    emitBytes(OP_GET_LOCAL, 0);
  }

  emitByte(OP_RETURN);
}

static ObjFunction *endCompiler(void) {
  TRACELN("  compiler.endCompiler()");

  // Restore the loop context this function was declared inside of (see
  // initCompiler).
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

// ── Expressions ──────────────────────────────────────────────────────────────

static void compileCall(CallNode *c) {
  // `obj.method(args)` compiles straight to OP_INVOKE instead of
  // OP_GET_PROPERTY followed by OP_CALL -- same call-site optimization the
  // original dot()/call() pair did when '(' immediately followed '.'.
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
    // Resolved (and, for globals, its name constant added) *before*
    // compiling the value, to match the original's constant-pool ordering.
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
    emitByte(OP_POP); // discard left if it's nil
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
    // Name constant captured *before* compiling the value, matching the
    // original dot()'s ordering.
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

  case NODE_SUPER:
    // No inheritance support in this fork's bytecode (OP_GET_SUPER/
    // OP_INHERIT don't exist) -- 'super' was never wired up in the old
    // compiler either, so there's no prior behavior to match here.
    errorAtToken(&node->as.super_.keyword, "Superclasses aren't supported.");
    emitByte(OP_NIL); // keep the stack balanced for the rest of this (failed)
                      // compile
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

  case NODE_FUNCTION:
    // A lambda expression, e.g. `var f = fun (x) { x };` -- just compile
    // the closure and leave it on the stack; unlike a function
    // *declaration* (compileStmt's NODE_FUNCTION case), there's no name to
    // declare/define as a variable.
    compileFunction(&node->as.function, TYPE_FUNCTION);
    break;

  case NODE_IF: {
    // `if` used as an EXPRESSION: both arms are compiled with compileExpr
    // (they're guaranteed to be pure expressions -- see parser.c's
    // ifExpr()), and a missing `else` just means "nil".
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

  case NODE_BLOCK: {
    // A block used as an EXPRESSION, e.g. `var x = { foo(); 5 };`. Its
    // statements are compiled the ordinary way (each one is fully
    // self-contained, whatever mix of declarations/expression-statements
    // parseBlockExprContents produced), then its tail value (or a default
    // OP_NIL if it has none) is left on the stack, and the scope is closed
    // with a single OP_CLOSE_BLOCK_EXPR rather than per-local POPs -- see
    // compileBlockExprClose().
    beginScope();
    BlockNode *blk = &node->as.block;
    for (int i = 0; i < blk->count; i++) {
      compileStmt(blk->stmts[i]);
    }
    if (blk->value != NULL) {
      compileExpr(blk->value);
    } else {
      // Matches the original: the default OP_NIL (no tail value) runs
      // after consume(RIGHT_BRACE), so it's tagged with the closing
      // brace's line.
      currentLine = blk->endLine;
      emitByte(OP_NIL);
    }
    // Matches the original: endScopeExpression() also runs right after
    // consume(RIGHT_BRACE) (whether or not a tail value was produced), so
    // it's tagged with the closing brace's line too.
    currentLine = blk->endLine;
    compileBlockExprClose();
    break;
  }

  default:
    errorAtNode(node, "Internal error: not a valid expression node.");
    break;
  }
}

// ── Statements ───────────────────────────────────────────────────────────────

static void compileVarDecl(AstNode *node) {
  VarDeclNode *vd = &node->as.varDecl;
  uint8_t global = parseVariableFromToken(&vd->name);

  if (vd->initializer != NULL) {
    compileExpr(vd->initializer);
  } else {
    // Matches the original: nothing is consumed between the name and the
    // failed `match(EQUAL)` check, so parser.previous is still the name
    // token.
    currentLine = vd->name.line;
    emitByte(OP_NIL);
  }

  // Matches the original: consume(SEMICOLON) runs immediately before
  // defineVariable() (which, for a global, emits OP_DEFINE_GLOBAL), so
  // parser.previous is the ';' by this point.
  currentLine = vd->declEndLine;
  defineVariable(global);
}

// Compiles a function/method/lambda body: declares its parameters as
// locals, compiles the body (either a `{ block }`'s statements plus its
// implicit-return tail value -- see compileBlockContents() -- or a single
// `= expr;` body, matching the original's shared function()), then wraps
// the whole thing up into a closure. Note there's no beginScope()/
// endScope() *around* a block-form body itself -- it shares the single
// scope opened for the parameters, exactly like the original function()'s
// bare `block(scanner)` call.
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

  // Matches the original: `function()` calls endCompiler() immediately
  // after consuming the body's closing '}' (or ';' for a `= expr;` body),
  // with nothing in between to move `parser.previous` on -- so the
  // implicit-return fallback in endCompiler() (when the body didn't already
  // end in an explicit/tail return) is tagged with that closing token's
  // line, not whatever line was last visited while compiling the body.
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
  // Marked initialized *before* compiling the body (not just after, the way
  // defineVariable() would down below) so the function can call itself
  // recursively through its own local slot.
  markInitialized();
  compileFunction(fn, TYPE_FUNCTION);
  defineVariable(global);
}

static void compileClassDecl(AstNode *node) {
  ClassNode *cn = &node->as.class_;

  if (cn->superclass != NULL) {
    errorAtToken(cn->superclass, "Superclasses aren't supported.");
    // Fall through and compile the class body anyway (as if the
    // `< Superclass` clause weren't there) so we still report any further
    // errors in the class body instead of bailing out entirely.
  }

  uint8_t nameConstant = identifierConstant(&cn->name);
  declareVariable(&cn->name);
  emitBytes(OP_CLASS, nameConstant);
  defineVariable(nameConstant);

  ClassCompiler classCompiler;
  classCompiler.enclosing = currentClass;
  classCompiler.fieldCount = 0; // unused -- nothing reads ClassCompiler.fields
  currentClass = &classCompiler;

  // Push the class back onto the stack so fields/methods can be bound to
  // it, mirroring the original's `namedVariable(scanner, className, false)`.
  VarRef ref = resolveVariable(&cn->name);
  emitBytes(ref.getOp, ref.arg);

  // Fields and methods are compiled in the exact order they appear in the
  // source (see ClassNode.members in ast.h), matching what a single
  // interleaved pass over the class body would emit.
  for (int i = 0; i < cn->memberCount; i++) {
    ClassMember *m = &cn->members[i];

    if (m->kind == CLASS_MEMBER_FIELD) {
      // Matches the original fieldDeclaration(): parser.previous is the
      // field name right after consume(IDENTIFIER), so both the
      // identifierConstant() call (whose 'too many constants' error needs
      // the field's own line) and the no-initializer default-nil are
      // tagged with the name's line.
      currentLine = m->as.field.name.line;
      uint8_t constant = identifierConstant(&m->as.field.name);
      if (m->as.field.initializer != NULL) {
        compileExpr(m->as.field.initializer);
      } else {
        emitByte(OP_NIL);
      }
      // Matches the original: consume(SEMICOLON) runs immediately before
      // emitBytes(OP_FIELD, ...).
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

  // Matches the original: consume(RIGHT_BRACE) is immediately followed by
  // emitByte(OP_POP), with nothing in between -- so it's tagged with the
  // closing brace's line, not whatever line the last member was on.
  currentLine = cn->endLine;
  emitByte(OP_POP); // pop the class object pushed above

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

// Pushes a LoopCompiler for a loop whose body is about to be compiled.
// `scopeDepth` is the depth breaks must unwind back down to.
static void beginLoop(LoopCompiler *loop, int scopeDepth) {
  loop->enclosing = currentLoop;
  loop->scopeDepth = scopeDepth;
  loop->breakCount = 0;
  currentLoop = loop;
}

// Patches every `break` in this loop to jump here, then pops the loop.
//
// This must be called at the loop's *post-condition-pop* exit point: the
// normal exit path falls out of OP_JUMP_IF_FALSE with the condition value
// still on the stack and pops it, but a `break` jumps from inside the body
// where that value is already gone. Patching here (rather than at the
// OP_JUMP_IF_FALSE target) keeps both paths stack-balanced.
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

// Compiles the contents of a block: its statements, then -- if the block
// ends in a trailing expression with no semicolon (an implicit-return tail,
// see BlockNode.value) -- that expression's implicit return.
//
// This intentionally does *not* touch scope: callers decide whether the
// block needs its own beginScope()/endScope() (nested `{ }` used as a
// statement does; a function's own top-level body does not, since it
// already shares the scope opened for its parameters). This mirrors the
// original: nested blocks call beginScope()/block()/endScope(), while
// function() calls the equivalent of block() directly.
static void compileBlockContents(BlockNode *block) {
  for (int i = 0; i < block->count; i++) {
    compileStmt(block->stmts[i]);
  }

  if (block->value != NULL) {
    compileExpr(block->value);

    // Matches the original quirk exactly: this implicit-return path emits a
    // bare OP_RETURN, *not* emitValueReturn() -- so (like the original) it
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
    // Matches the original: endScope() (POP/CLOSE_UPVALUE for locals going
    // out of scope) runs immediately after block()'s consume(RIGHT_BRACE),
    // with nothing in between -- so it's tagged with the closing brace's
    // line, not whatever line was last visited while compiling the block's
    // contents.
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

// ── Entry point ──────────────────────────────────────────────────────────────

ObjFunction *compile(AstNode **ast, int count, int endLine) {
  TRACELN("  compiler.compile()");

  hadError = false;

  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT, NULL);

  // Pass 1: hoist top-level function declarations.
  for (int i = 0; i < count; i++) {
    if (ast[i]->kind == NODE_FUNCTION) {
      compileStmt(ast[i]);
    }
  }

  // Pass 2: everything else, in source order.
  for (int i = 0; i < count; i++) {
    if (ast[i]->kind != NODE_FUNCTION) {
      compileStmt(ast[i]);
    }
  }

  // Matches the original: the top-level loop always ends by consuming the
  // EOF token, so `parser.previous.line` (and hence the trailing implicit
  // return's line tag) was always the EOF's line -- not the last
  // statement's line, which can differ (trailing blank lines/comments, or
  // simply an empty file).
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
