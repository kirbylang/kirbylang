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
} LoopCompiler;

/**
 * Compile an AST into a CompiledUnit.
 *
 * Caller owns the unit (freeCompiledUnit()). Must be called between a
 * matching compilerSessionBegin()/compilerSessionEnd() pair.
 */
CompiledUnit *compile(AstNode **ast, int count, int endLine);

/** Diagnostic: true while a compiler is on the stack. */
bool compilerIsActive(void);

/**
 * Begin a compiler session: compile() calls made until the matching
 * compilerSessionEnd() share immutable-global (`let`) tracking. Call once
 * before any related sequence of compile() calls -- e.g. once per VM
 * lifetime in the CLI, bracketing both the stdlib compile and every REPL
 * line, so a `let` declared on one line is still protected on the next.
 *
 * The compiler has no dependency on the VM or GC, so this is not tied to
 * initVM()/freeVM() -- the caller orchestrating a sequence of compiles (e.g.
 * main.c) owns the session boundary explicitly, independent of any other
 * module's lifecycle.
 */
void compilerSessionBegin(void);

/**
 * End a compiler session, freeing everything tracked during it. Must be
 * called before the process exits or before starting an unrelated session
 * that should not see this one's `let` names.
 */
void compilerSessionEnd(void);

#endif
