#ifndef kirby_compiled_unit_h
#define kirby_compiled_unit_h

#include "common.h"

/**
 * A CompiledUnit is the flat, heap-free, serializable artifact produced by the
 * compiler. It contains no pointers into any GC heap and no Obj headers: every
 * cross-reference is either an inline scalar, a slice into the string blob, or
 * an index into a table. The loader (loader.c) is the sole consumer that turns
 * a CompiledUnit into live runtime objects.
 *
 * This is the mechanism that fully decouples the compiler from the runtime,
 * GC, and VM: compile() builds a CompiledUnit with plain malloc and never
 * touches a GC.
 */

typedef enum {
  CONST_NUMBER,
  CONST_BOOL,
  CONST_NIL,
  CONST_STRING,   // a slice into CompiledUnit.strings
  CONST_FUNCTION, // an index into CompiledUnit.functions
} CompiledConstKind;

typedef struct {
  CompiledConstKind kind;
  union {
    double number;
    bool boolean;
    struct {
      int offset;
      int length;
    } string;
    int functionIndex;
  } as;
} CompiledConst;

/**
 * Flat upvalue capture descriptor (same information the compiler already
 * tracks: whether the captured variable is a local of the enclosing function
 * and its slot/index).
 */
typedef struct {
  bool isLocal;
  uint8_t index;
} CompiledUpvalue;

/**
 * A compiled function.
 *
 * This is translated to an ObjFunction at runtime in the VM.
 */
typedef struct {
  int arity;
  int upvalueCount;

  // Function Bytecode

  uint8_t *code; // Bytecode opcodes
  int codeCount;
  int codeCapacity;
  int *codeLines; // Which line is a bytecode op on?

  // Function Constants

  CompiledConst *constants;
  int constantCount;
  int constantCapacity;

  // Function Upvalues

  CompiledUpvalue *upvalues;
  int upvalueDescCount;
  int upvalueDescCapacity;

  // Function Name

  int nameOffset; // Offset in to the compiled unit's string blob
  int nameLength; // Negative length is a lambda

  // Function Modifiers

  bool isStatic;
  bool isPublic;
} CompiledFn;

/**
 * A compiled program. A *.krb file.
 */
typedef struct {
  // Interned Strings Blob

  char *strings; // Interned strings stored contigously.  No duplicate checks
                 // in place at this time
  int stringsLen;
  int stringsCapacity;

  // Functions

  CompiledFn *functions; // functions[0] is always the implict top-level script
                         // function of the file.
  int functionCount;
  int functionCapacity;

  // Order in which functions finished compiling (innermost-first), used only to
  // reproduce debug disassembly order. Same length as functionCount.
  //
  // TODO: I don't understand how this works

  int *finishOrder;
  int finishOrderCount;
  int finishOrderCapacity;
} CompiledUnit;

/**
 * Free a CompiledUnit and everything it owns. Plain free -- the artifact is not
 * GC-managed.
 */
void freeCompiledUnit(CompiledUnit *compiledUnit);

// Builder API used by the compiler to emit a CompiledUnit. None of this touches
// the GC; it is all plain malloc-backed growth.

/**
 * Initialize a compiled unit
 */
void cuInit(CompiledUnit *compiledUnit);

/**
 * Append `length` bytes to the unit's string blob and return the offset at
 * which they start. The blob is not NUL-terminated between entries; each entry
 * is addressed by (offset, length).
 */
int cuInternString(CompiledUnit *compiledUnit, const char *chars, int length);

/**
 * Reserve a new CompiledFn slot and return its index. Returns the index; the
 * caller fills the slot in via cuFn(unit, index).
 */
int cuAddFunction(CompiledUnit *compiledUnit);

/**
 * Get a CompiledFn point at a given index in the CompiledUnit
 */
CompiledFn *cuGetFnByIndex(CompiledUnit *compiledUnit, int index);

/**
 * Write a single byte to the compiled function
 */
void cuWriteByte(CompiledFn *compiledFn, uint8_t byte, int line);

int cuAddConstant(CompiledFn *compiledFn, CompiledConst compiledConst);

int cuAddUpvalue(CompiledFn *compiledFn, bool isLocal, uint8_t index);

/**
 * Record that function `index` has finished compiling. The loader replays this
 * order to reproduce debug disassembly output.
 */
void cuMarkFinished(CompiledUnit *compiledUnit, int index);

#endif
