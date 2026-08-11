#ifndef kirby_compiler_h
#define kirby_compiler_h

#include "ast.h"
#include "compiled_unit.h"
#include "opcode.h"
#include "token.h"

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
  bool isMutable;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_METHOD,
  TYPE_STATIC_METHOD,
  TYPE_SCRIPT
} FunctionType;

typedef struct {
  uint8_t index;
  bool isLocal;
  bool isMutable;
} Upvalue;

/**
 * A function compiler
 */
typedef struct Compiler Compiler;

struct Compiler {
  /**
   * The outer function that this compiler is compiling for.
   */
  Compiler *enclosing;

  // Index of this function's CompiledFn within the unit being built, and a
  // cached pointer to it.
  int fnIndex;
  CompiledFn *fn;

  int upvalueCount;

  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;

  struct LoopCompiler *enclosingLoop;
};

typedef struct LoopCompiler {
  struct LoopCompiler *enclosing;
  int scopeDepth;
  int breakJumps[UINT8_COUNT];
  int breakCount;
} LoopCompiler;

/**
 * Compile an AST into a CompiledUnit.
 *
 * Returns NULL on error. TODO: Should it?
 *
 * Caller owns the unit (freeCompiledUnit).
 */
CompiledUnit *compile(AstNode **ast, int count, int endLine);

/** Diagnostic: true while a compiler is on the stack. */
bool compilerIsActive(void);

#endif
