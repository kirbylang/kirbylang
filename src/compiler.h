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
  FNTYPE_FUNCTION,
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
typedef struct FnCompiler FnCompiler;

struct FnCompiler {
  /**
   * The outer function that this compiler is compiling for.
   */
  FnCompiler *enclosing;

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
  /**
   * Bytecode offset that `continue` jumps back to:
   *
   * while: the condition check
   * for: the increment (falling back to the condition)
   */
  int continueTarget;
  /**
   * Scope depth `continue` pops locals down to.
   *
   * For Loops:
   *
   * Shallower than `scopeDepth` when a `for` loop's init clause declares a
   * variable, since `continue` must keep that variable alive across iterations
   * while `break` discards it on the way out of the loop.
   */
  int continueDepth;
} LoopCompiler;

/**
 * Compile an AST into a CompiledUnit.
 *
 * Caller owns the unit (freeCompiledUnit()). Must be called between a
 * matching compilerSessionBegin()/compilerSessionEnd() pair.
 */
CompiledUnit *compile(AstNode **ast, int count, int endLine);

/**
 * End a compiler session, freeing everything tracked during it. Must be
 * called before the process exits or before starting an unrelated session
 * that should not see this one's `let` names.
 */
void compilerSessionEnd(void);

#endif
