#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "hashtable.h"
#include "object.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

/**
 * Bytecode Virtual Machine
 */
struct VM {
  // A copy of `argc` passed to `main`
  int argc;

  // A copy of `argv` passed to `main`
  char **argv;

  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Chunk *chunk;
  uint8_t *ip;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table globals;
  Table strings;

  ObjString *initString;

  ObjUpvalue *openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;

  Obj *objects;

  int grayCount;
  int grayCapacity;
  Obj **grayStack;
};

extern VM vm;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

/**
 * Initialize the VM.
 *
 * Initializes:
 *
 * - Stack
 * - Garbage Collector
 * - Globals Table
 * - Internerned String Table
 * - Intern Class Initializer String
 * - Define Native Functions
 */
void initVM(int argc, char *argv[]);

/**
 * Shutdown the VM.
 *
 * - Free globals
 * - Free interned strings
 * - Garbage collect class initializer string
 * - Free all objects
 */
void freeVM(void);

/**
 * Interpret a compiled function's bytecode
 *
 * @return One of INTERPRET_OK, INTERPRET_COMPILE_ERROR, INTERPRET_RUNTIME_ERROR
 */
InterpretResult interpretFunction(ObjFunction *function);

/**
 * Compile and interpret source code
 */
InterpretResult interpret(const char *source);

void runtimeError(VM *vm, const char *format, ...);

// Push a value on to the VM's stack
void pushOnStack(Value value);

// Pop a value from the VM's stack and returns it
// @return The popped value
Value popFromStack(void);

// Pop n values from the VM's stack
void popNFromStack(int count);

#endif
