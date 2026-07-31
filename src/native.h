#ifndef kirby_native_h
#define kirby_native_h

#include "hashtable.h"
#include "object.h"
#include "vm.h"

void defineNative(VM *vm, const char *name, NativeFn function);
void defineAllNatives(VM *vm);

#endif
