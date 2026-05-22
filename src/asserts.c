#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "vm.h"

void raiseError(VM *vm, const char *message) {
  runtimeError(vm, message);
  exit(70);
}

/**
 * Assert a function call arg count matches the function's arity
 */
void assertArgCount(VM *vm, const char *function, int expectedCount,
                    int actualCount) {
  if (expectedCount != actualCount) {
    runtimeError(vm, "function %s expects %d argument(s) but got %d instead.",
                 function, expectedCount, actualCount);
    exit(70);
  }
}

void assertArgIsBool(VM *vm, const char *function, Value *args, int index) {
  if (!IS_BOOL(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be a boolean value.",
                 function, index + 1);
    exit(70);
  }
}

void assertArgIsClass(VM *vm, const char *function, Value *args, int index) {
  if (!IS_CLASS(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be a class.", function,
                 index + 1);
    exit(70);
  }
}

void assertArgIsArray(VM *vm, const char *function, Value *args, int index) {
  if (!IS_ARRAY(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be an array.",
                 function, index + 1);
    exit(70);
  }
}

void assertIsInArrayBounds(VM *vm, ObjArray *array, int index) {
  if (index < 0 || index > array->count) {
    runtimeError(vm, "index %d out of bounds. array length: %d", index,
                 array->count);
    exit(70);
  }
}

void assertArgIsNumber(VM *vm, const char *function, Value *args, int index) {
  if (!IS_NUMBER(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be a number.",
                 function, index + 1);
    exit(70);
  }
}

void assertArgIsString(VM *vm, const char *function, Value *args, int index) {
  if (!IS_STRING(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be a string.",
                 function, index + 1);
    exit(70);
  }
}

void assertArgIsInstance(VM *vm, const char *function, Value *args, int index) {
  if (!IS_INSTANCE(args[index])) {
    runtimeError(vm, "function %s expects argument %d to be an instance.",
                 function, index + 1);
    exit(70);
  }
}

void assertNonZero(VM *vm, const char *function, double number, int index) {
  if (number == 0) {
    if (index < 0)
      runtimeError(
          vm,
          "function %s expects receiver to be a non-zero number but got %g.",
          function, number);
    else
      runtimeError(
          vm,
          "function %s expects argument %d to be a non-zero number but got %g.",
          function, index, number);
    exit(70);
  }
}

void assertPositiveNumber(VM *vm, const char *function, double number,
                          int index) {
  if (number <= 0) {
    if (index < 0)
      runtimeError(
          vm,
          "function %s expects receiver to be a positive number but got %g.",
          function, number);
    else
      runtimeError(
          vm,
          "function %s expects argument %d to be a positive number but got %g.",
          function, index, number);
    exit(70);
  }
}
