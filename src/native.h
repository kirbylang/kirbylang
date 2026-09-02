#ifndef kirby_native_h
#define kirby_native_h

#include "hashtable.h"
#include "object.h"
#include "vm.h"

typedef struct {
  const char *name;
  NativeFn function;
} NativeDefinition;

extern const NativeDefinition nativeDefinitions[];
extern const int nativeDefinitionCount;

void defineNative(VM *vm, const char *name, NativeFn function);
void defineAllNatives(VM *vm);

#endif
