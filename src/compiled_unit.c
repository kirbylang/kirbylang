#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiled_unit.h"
#include "stringset.h"

#define CU_FN_GROW_SIZE 2
#define CU_FN_MIN_SIZE 8

#define CU_FN_CODE_GROW_SIZE 2
#define CU_FN_CODE_MIN_SIZE 8

#define CU_FN_CONST_GROW_SIZE 2
#define CU_FN_CONST_MIN_SIZE 8

static void *xrealloc(void *ptr, size_t size);
static void cuInitFn(CompiledFn *compiledfn);
static bool compiledConstantsAreEqual(CompiledConst a, CompiledConst b);

void cuInit(CompiledUnit *compiledUnit) {
  stringSetInit(&compiledUnit->strings);

  compiledUnit->functions = NULL;
  compiledUnit->functionCount = 0;
  compiledUnit->functionCapacity = 0;
}

/**
 * Add a string in the compiled unit's iterned strings.
 *
 * Returns the interned string's index.
 */
int cuInternString(CompiledUnit *compiledUnit, const char *chars, int length) {
  return stringSetIntern(&compiledUnit->strings, chars, length);
}

/**
 * Initialize a new compiled function.
 *
 * Returns the index of the new function in compiled unit's functions.
 */
int cuAddFunction(CompiledUnit *compiledUnit) {
  if (compiledUnit->functionCount + 1 > compiledUnit->functionCapacity) {
    int cap = compiledUnit->functionCapacity < CU_FN_MIN_SIZE
                  ? CU_FN_MIN_SIZE
                  : compiledUnit->functionCapacity * CU_FN_GROW_SIZE;

    compiledUnit->functions = (CompiledFn *)xrealloc(
        compiledUnit->functions, sizeof(CompiledFn) * (size_t)cap);
    compiledUnit->functionCapacity = cap;
  }

  int index = compiledUnit->functionCount++;
  cuInitFn(&compiledUnit->functions[index]);
  return index;
}

CompiledFn *cuGetFnByIndex(CompiledUnit *compiledUnit, int index) {
  return &compiledUnit->functions[index];
}

/**
 * Write a single byte to a compiled function's bytecode.
 */
void cuWriteByte(CompiledFn *compiledFn, uint8_t byte, int line) {
  if (compiledFn->codeCount + 1 > compiledFn->codeCapacity) {
    int cap = compiledFn->codeCapacity < CU_FN_CODE_MIN_SIZE
                  ? CU_FN_CODE_MIN_SIZE
                  : compiledFn->codeCapacity * CU_FN_CODE_GROW_SIZE;

    compiledFn->code =
        (uint8_t *)xrealloc(compiledFn->code, sizeof(uint8_t) * (size_t)cap);
    compiledFn->codeLines =
        (int *)xrealloc(compiledFn->codeLines, sizeof(int) * (size_t)cap);
    compiledFn->codeCapacity = cap;
  }

  compiledFn->code[compiledFn->codeCount] = byte;
  compiledFn->codeLines[compiledFn->codeCount] = line;
  compiledFn->codeCount++;
}

int cuAddConstant(CompiledFn *compiledFn, CompiledConst compiledConst) {
  for (int i = 0; i < compiledFn->constantCount; i++) {
    if (compiledConstantsAreEqual(compiledFn->constants[i], compiledConst))
      return i;
  }

  if (compiledFn->constantCount + 1 > compiledFn->constantCapacity) {
    int cap = compiledFn->constantCapacity < CU_FN_CONST_MIN_SIZE
                  ? CU_FN_CONST_MIN_SIZE
                  : compiledFn->constantCapacity * CU_FN_CONST_GROW_SIZE;

    compiledFn->constants = (CompiledConst *)xrealloc(
        compiledFn->constants, sizeof(CompiledConst) * (size_t)cap);
    compiledFn->constantCapacity = cap;
  }

  compiledFn->constants[compiledFn->constantCount] = compiledConst;
  return compiledFn->constantCount++;
}

static bool compiledConstantsAreEqual(CompiledConst a, CompiledConst b) {
  if (a.kind != b.kind)
    return false;

  switch (a.kind) {
  case CONST_NUMBER:
    // Compare the bits, so 0 and -0 stay separate and NaN matches itself.
    return memcmp(&a.as.number, &b.as.number, sizeof(double)) == 0;
  case CONST_BOOL:
    return a.as.boolean == b.as.boolean;
  case CONST_NIL:
    return true;
  case CONST_STRING:
    // Strings are interned, so equal strings share an offset.
    return a.as.string.offset == b.as.string.offset &&
           a.as.string.length == b.as.string.length;
  case CONST_FUNCTION:
    return false;
  }

  return false;
}

#define CU_FN_UPVALUE_GROW_SIZE 2
#define CU_FN_UPVALUE_MIN_SIZE 8

int cuAddUpvalue(CompiledFn *compiledFn, bool isLocal, uint8_t index) {
  if (compiledFn->upvalueDescCount + 1 > compiledFn->upvalueDescCapacity) {
    int cap = compiledFn->upvalueDescCapacity < CU_FN_UPVALUE_MIN_SIZE
                  ? CU_FN_UPVALUE_MIN_SIZE
                  : compiledFn->upvalueDescCapacity * CU_FN_UPVALUE_GROW_SIZE;

    compiledFn->upvalues = (CompiledUpvalue *)xrealloc(
        compiledFn->upvalues, sizeof(CompiledUpvalue) * (size_t)cap);
    compiledFn->upvalueDescCapacity = cap;
  }

  compiledFn->upvalues[compiledFn->upvalueDescCount].isLocal = isLocal;
  compiledFn->upvalues[compiledFn->upvalueDescCount].index = index;
  return compiledFn->upvalueDescCount++;
}

static void freeCompiledUnitFns(CompiledUnit *compiledUnit) {
  for (int i = 0; i < compiledUnit->functionCount; i++) {
    CompiledFn *fn = &compiledUnit->functions[i];
    free(fn->code);
    free(fn->codeLines);
    free(fn->constants);
    free(fn->upvalues);
  }

  free(compiledUnit->functions);
}

static void freeCompiledUnitStrings(CompiledUnit *compiledUnit) {
  stringSetFree(&compiledUnit->strings);
}

void freeCompiledUnit(CompiledUnit *compiledUnit) {
  freeCompiledUnitFns(compiledUnit);
  freeCompiledUnitStrings(compiledUnit);
  cuInit(compiledUnit);
}

/**
 * Initialize a function for a compiled unit
 */
static void cuInitFn(CompiledFn *compiledFn) {
  compiledFn->arity = 0;
  compiledFn->upvalueCount = 0;
  compiledFn->code = NULL;
  compiledFn->codeCount = 0;
  compiledFn->codeCapacity = 0;
  compiledFn->codeLines = NULL;
  compiledFn->constants = NULL;
  compiledFn->constantCount = 0;
  compiledFn->constantCapacity = 0;
  compiledFn->upvalues = NULL;
  compiledFn->upvalueDescCount = 0;
  compiledFn->upvalueDescCapacity = 0;
  compiledFn->nameOffset = 0;
  compiledFn->nameLength = -1;
  compiledFn->isStatic = false;
  compiledFn->isPublic = false;
}

static void *xrealloc(void *ptr, size_t size) {
  void *result = realloc(ptr, size);
  if (result == NULL && size != 0) {
    fprintf(stderr, "realloc failed in compiled_unit");
    exit(EXIT_CODE_OS_ERR);
  }
  return result;
}
