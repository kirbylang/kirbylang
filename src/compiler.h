#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "object.h"
#include "scanner.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

typedef enum { VIS_PRIVATE, VIS_PUBLIC } Visibility;

typedef struct {
  Token name;
  Visibility visibility;
  int slot;
} FieldCompiler;

typedef struct {
  Token name;
  Visibility visibility;
} MethodCompiler;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or || ??
  PREC_AND,        // and &&
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Scanner *scanner, bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

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

typedef struct ClassCompiler {
  Token name;
  FieldCompiler fields[256];
  int fieldCount;
  MethodCompiler methods[256];
  int methodCount;

  struct ClassCompiler *enclosing;
} ClassCompiler;

/**
 * Compile source code in to a compiled function object.
 *
 * This compiled object can be ran by the VM.
 */
ObjFunction *compile(Scanner *scanner, const char *source);

/**
 * Mark all objects that are referenced by the compiler as roots
 */
void markCompilerRoots();

#endif