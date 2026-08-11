#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiled_unit.h"

#define CU_FN_GROW_SIZE 2
#define CU_FN_MIN_SIZE 8

#define CU_FN_CODE_GROW_SIZE 2
#define CU_FN_CODE_MIN_SIZE 8

#define CU_FN_CONST_GROW_SIZE 2
#define CU_FN_CONST_MIN_SIZE 8

static void *xrealloc(void *ptr, size_t size);
static void cuInitFn(CompiledFn *compiledfn);

void cuInit(CompiledUnit *compiledUnit) {
  compiledUnit->strings = NULL;
  compiledUnit->stringsLen = 0;
  compiledUnit->stringsCapacity = 0;

  compiledUnit->functions = NULL;
  compiledUnit->functionCount = 0;
  compiledUnit->functionCapacity = 0;

  compiledUnit->finishOrder = NULL;
  compiledUnit->finishOrderCount = 0;
  compiledUnit->finishOrderCapacity = 0;
}

void cuMarkFinished(CompiledUnit *compiledUnit, int index) {
  if (compiledUnit->finishOrderCount + 1 > compiledUnit->finishOrderCapacity) {
    int cap = compiledUnit->finishOrderCapacity < 8
                  ? 8
                  : compiledUnit->finishOrderCapacity * 2;
    compiledUnit->finishOrder =
        (int *)xrealloc(compiledUnit->finishOrder, sizeof(int) * (size_t)cap);
    compiledUnit->finishOrderCapacity = cap;
  }
  compiledUnit->finishOrder[compiledUnit->finishOrderCount++] = index;
}

int cuInternString(CompiledUnit *compiledUnit, const char *chars, int length) {
  if (compiledUnit->stringsLen + length > compiledUnit->stringsCapacity) {
    int needed = compiledUnit->stringsLen + length;
    int cap =
        compiledUnit->stringsCapacity < 16 ? 16 : compiledUnit->stringsCapacity;
    while (cap < needed)
      cap *= 2;
    compiledUnit->strings =
        (char *)xrealloc(compiledUnit->strings, (size_t)cap);
    compiledUnit->stringsCapacity = cap;
  }

  int offset = compiledUnit->stringsLen;
  memcpy(compiledUnit->strings + offset, chars, (size_t)length);
  compiledUnit->stringsLen += length;
  return offset;
}

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

void cuWriteByte(CompiledFn *compiledFn, uint8_t byte, int line) {
  if (compiledFn->codeCount + 1 > compiledFn->codeCapacity) {
    int cap = compiledFn->codeCapacity < CU_FN_CODE_MIN_SIZE
                  ? CU_FN_CODE_MIN_SIZE
                  : compiledFn->codeCapacity * CU_FN_CODE_GROW_SIZE;

    compiledFn->code =
        (uint8_t *)xrealloc(compiledFn->code, sizeof(uint8_t) * (size_t)cap);
    compiledFn->lines =
        (int *)xrealloc(compiledFn->lines, sizeof(int) * (size_t)cap);
    compiledFn->codeCapacity = cap;
  }

  compiledFn->code[compiledFn->codeCount] = byte;
  compiledFn->lines[compiledFn->codeCount] = line;
  compiledFn->codeCount++;
}

int cuAddConstant(CompiledFn *compiledFn, CompiledConst compiledConst) {
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
    free(fn->lines);
    free(fn->constants);
    free(fn->upvalues);
  }

  free(compiledUnit->functions);
}

static void freeCompiledUnitStrings(CompiledUnit *compiledUnit) {
  free(compiledUnit->strings);
}

static void freeCompiledUnitFinishOrder(CompiledUnit *compiledUnit) {
  free(compiledUnit->finishOrder);
}

void freeCompiledUnit(CompiledUnit *compiledUnit) {
  freeCompiledUnitFns(compiledUnit);
  freeCompiledUnitStrings(compiledUnit);
  freeCompiledUnitFinishOrder(compiledUnit);
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
  compiledFn->lines = NULL;
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
    exit(1);
  }
  return result;
}
