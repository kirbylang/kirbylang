#ifndef clox_compiler_h
#define clox_compiler_h

#include "ast.h"
#include "chunk.h"
#include "object.h"
#include "token.h"

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef enum {
  TYPE_FUNCTION,
  TYPE_METHOD,
  TYPE_INITIALIZER,
  TYPE_SCRIPT
} FunctionType;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef struct Compiler Compiler;

struct Compiler {
  Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;

  struct LoopCompiler *enclosingLoop;
};

typedef struct {
  ObjString *name;
  uint8_t slot;
} Field;

typedef struct ClassCompiler {
  struct ClassCompiler *enclosing;

  Field fields[256];
  int fieldCount;
} ClassCompiler;

typedef struct LoopCompiler {
  struct LoopCompiler *enclosing;
  int scopeDepth;
  int breakJumps[UINT8_COUNT];
  int breakCount;
} LoopCompiler;

/**
 * Compile a AST node into a compiled function object.
 */
ObjFunction *compile(AstNode **ast, int count, int endLine);

/**
 * Mark all objects that are referenced by the compiler as roots
 */
void markCompilerRoots(void);

#endif
