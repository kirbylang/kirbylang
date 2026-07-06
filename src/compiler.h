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

/**
 * Compile a fully-parsed AST into a compiled function object.
 *
 * `ast`/`count` are exactly what parse() (see parser.h) returns: the
 * top-level declaration nodes. `endLine` is parse()'s *outEndLine --
 * needed to tag the script's trailing implicit return with the same line
 * the original single-pass compiler would have (the line of the last
 * token in the file), rather than whatever line was last visited while
 * compiling the final statement. compile() does not take ownership of the
 * AST -- the caller still frees it via astFreeAll(), same as before.
 *
 * This compiled object can be run by the VM.
 *
 * Returns NULL if any compile-time error was reported.
 */
ObjFunction *compile(AstNode **ast, int count, int endLine);

/**
 * Mark all objects that are referenced by the compiler as roots
 */
void markCompilerRoots(void);

#endif
