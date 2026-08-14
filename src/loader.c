#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "gc.h"
#include "loader.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

static void pass1(GC *gc, const CompiledUnit *compiledUnit,
                  ObjFunction **byIndex, int n);
static void writeByteCodeOpToChunk(GC *gc, const CompiledFn *compiledFn,
                                   Chunk *chunk, int byteCodeIndex);
static void pass2(GC *gc, const CompiledUnit *compiledUnit,
                  ObjFunction **byIndex, int n);
static Value resolveConstToValue(GC *gc, const CompiledUnit *compiledUnit,
                                 const CompiledConst *compiledConst,
                                 ObjFunction **byIndex);
static void cleanup(int n);

/**
 * Load a CompiledUnit as a ObjFunction
 */
ObjFunction *loadUnit(VM *vm, const CompiledUnit *compiledUnit) {
  int functionCount = compiledUnit->functionCount;

  // Track the ObjFunctions by index so CONST_FUNCTION references resolve, and
  // keep them reachable during loading by parking them on the value stack (the
  // GC can run while we allocate later functions and their constants).
  ObjFunction **byIndex =
      (ObjFunction **)malloc(sizeof(ObjFunction *) * (size_t)functionCount);

  if (byIndex == NULL) {
    fprintf(stderr, "malloc failed in loadUnit");
    exit(EXIT_CODE_OS_ERR);
  }

  pass1(vm->gc, compiledUnit, byIndex, functionCount);
  pass2(vm->gc, compiledUnit, byIndex, functionCount);

#ifdef DEBUG_PRINT_CODE
  for (int i = 0; i < functionCount; i++) {
    ObjFunction *fn = byIndex[i];
    disassembleChunk(&fn->chunk,
                     fn->name != NULL ? fn->name->chars : "<script>");
  }
#endif

  cleanup(functionCount);

  ObjFunction *script = byIndex[0];
  free(byIndex);
  return script;
}

/**
 * Map every compiled unit's compiled function in to function objects
 * Allocate every function and copy its bytecode/lines verbatim.
 */
static void pass1(GC *gc, const CompiledUnit *compiledUnit,
                  ObjFunction **byIndex, int functionCount) {
  for (int i = 0; i < functionCount; i++) {
    const CompiledFn *compiledFn = &compiledUnit->functions[i];
    ObjFunction *fnObj = newFunction(gc);
    pushOnStack(OBJ_VAL(fnObj));

    fnObj->arity = compiledFn->arity;
    fnObj->upvalueCount = compiledFn->upvalueCount;
    fnObj->isStatic = compiledFn->isStatic;
    fnObj->isPublic = compiledFn->isPublic;

    if (compiledFn->nameLength >= 0) {
      fnObj->name =
          copyString(gc, compiledUnit->strings.arena + compiledFn->nameOffset,
                     compiledFn->nameLength);
    } else {
      fnObj->name = NULL;
    }

    for (int j = 0; j < compiledFn->codeCount; j++) {
      writeByteCodeOpToChunk(gc, compiledFn, &fnObj->chunk, j);
    }

    byIndex[i] = fnObj;
  }
}

static void writeByteCodeOpToChunk(GC *gc, const CompiledFn *compiledFn,
                                   Chunk *chunk, int byteCodeIndex) {
  uint8_t byte = compiledFn->code[byteCodeIndex];
  int line = compiledFn->codeLines[byteCodeIndex];

  writeChunk(gc, chunk, byte, line);
}

/**
 * Resolve each function's constant pool into real Values.
 *
 * Done after all functions exist so CONST_FUNCTION indices resolve to real
 * pointers.
 */
static void pass2(GC *gc, const CompiledUnit *compiledUnit,
                  ObjFunction **byIndex, int functionCount) {
  for (int i = 0; i < functionCount; i++) {
    const CompiledFn *cfn = &compiledUnit->functions[i];
    ObjFunction *fn = byIndex[i];

    for (int j = 0; j < cfn->constantCount; j++) {
      Value resolvedValue =
          resolveConstToValue(gc, compiledUnit, &cfn->constants[j], byIndex);
      pushOnStack(resolvedValue);
      writeValueArray(gc, &fn->chunk.constants, resolvedValue);
      popFromStack();
    }
  }
}

/**
 * Resolve a CompiledConst into a runtime Value.
 *
 * Strings are interned into the GC's string table here (the compiler never
 * interns). Nested-function references (CONST_FUNCTION) resolve to the
 * ObjFunction the loader has already built for that index.
 */
static Value resolveConstToValue(GC *gc, const CompiledUnit *compiledUnit,
                                 const CompiledConst *compiledConst,
                                 ObjFunction **byIndex) {
  switch (compiledConst->kind) {
  case CONST_NUMBER:
    return NUMBER_VAL(compiledConst->as.number);
  case CONST_BOOL:
    return BOOL_VAL(compiledConst->as.boolean);
  case CONST_NIL:
    return NIL_VAL;
  case CONST_STRING: {
    char *chars = compiledUnit->strings.arena + compiledConst->as.string.offset;
    int length = compiledConst->as.string.length;
    ObjString *strObj = copyString(gc, chars, length);
    return OBJ_VAL(strObj);
  }
  case CONST_FUNCTION:
    return OBJ_VAL(byIndex[compiledConst->as.functionIndex]);
  }

  return NIL_VAL; // unreachable
}

/**
 * Everything is now reachable through byIndex[0]'s constant graph; drop the
 * stack parking (functions were pushed in pass 1).
 */
static void cleanup(int n) {
  for (int i = 0; i < n; i++) {
    popFromStack();
  }
}
