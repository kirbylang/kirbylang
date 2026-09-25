#ifndef kirby_compiler_h
#define kirby_compiler_h

#include "ast.h"
#include "compiled_unit.h"
#include "opcode.h"
#include "token.h"

/**
 * A local's depth from its declaration until its initializer finishes.
 */
#define LOCAL_UNINITIALIZED (-1)

typedef struct {
  Token name;
  /**
   * The scope depth the local belongs to, or LOCAL_UNINITIALIZED.
   */
  int depth;
  /**
   * Where the local's value lives in the call frame. This is not always its
   * index in `locals`, because a block expression's locals can sit above
   * operands, e.g. the callee in `f({ let n = 1; n })`.
   */
  int slot;
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
  /**
   * Values on the stack when the instruction at `bytesCounted` runs,
   * counting slot 0. A new local's slot is this height, so it counts
   * operands of an unfinished expression as well as locals. Read and write
   * it through currentStackHeight and resetStackHeight, which bring it up to
   * date with the bytecode emitted so far.
   */
  int stackHeight;
  /**
   * How much of the function's bytecode `stackHeight` accounts for.
   */
  int bytesCounted;
  /**
   * False when the last instruction was a jump, loop, or return. Nothing
   * falls through to the next instruction, so its stack height comes from
   * the jump that lands on it (see patchJump).
   */
  bool isReachable;

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
