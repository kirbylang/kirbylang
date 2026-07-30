#ifndef clox_assert_h
#define clox_assert_h

#include "common.h"
#include "value.h"
#include "vm.h"

void assertArgCount(VM *vm, const char *function, int expectedCount,
                    int actualCount);
void assertArgIsBool(VM *vm, const char *function, Value *args, int index);
void assertArgIsStruct(VM *vm, const char *function, Value *args, int index);
void assertArgIsArray(VM *vm, const char *function, Value *args, int index);
void assertIsInArrayBounds(VM *vm, ObjArray *array, int index);
void assertArgIsInstance(VM *vm, const char *function, Value *args, int index);
void assertArgIsNumber(VM *vm, const char *function, Value *args, int index);
void assertArgIsString(VM *vm, const char *function, Value *args, int index);
void assertNonZero(VM *vm, const char *function, double number, int index);
void assertPositiveNumber(VM *vm, const char *function, double number,
                          int index);
void raiseError(VM *vm, const char *message);

#endif
