#ifndef kirby_native_signatures_h
#define kirby_native_signatures_h

// This file is seperate from native.h to avoid typecheck.h pulling in files
// like gc.h, vm.h

#include "typecheck.h"
#include "types.h"

#define NATIVE_SIGNATURE_MAX_PARAMS 2

// The types a native signature can name today. Deliberately narrower than
// TypeKind
typedef enum {
  NATIVE_UNIT,
  NATIVE_BOOL,
  NATIVE_STRING,
  NATIVE_F64,
} NativePrimitive;

typedef struct {
  const char *name;
  NativePrimitive paramTypes[NATIVE_SIGNATURE_MAX_PARAMS];
  int paramCount;
  NativePrimitive returnType;
} NativeSignature;

extern const NativeSignature nativeSignatures[];
extern const int nativeSignatureCount;

void defineAllNativeSignatures(TypeEnv *env);

#endif
