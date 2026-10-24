#ifndef kirby_native_signatures_h
#define kirby_native_signatures_h

// This file is seperate from native.h to avoid typecheck.h pulling in files
// like gc.h, vm.h

#include "typecheck.h"
#include "types.h"

// A native's type, written the way Kirby writes function types. Signatures
// can also name generic parameters T, U, V and W, and `number` and `integer`,
// which accept any number or integer kind.
typedef struct {
  const char *name;
  const char *signature;
} NativeSignature;

extern const NativeSignature nativeSignatures[];
extern const int nativeSignatureCount;

typedef bool (*NativeArgPredicate)(double value);

typedef struct {
  const char *name;
  int paramIndex; // Zero-based
  NativeArgPredicate isValid;
  const char *expectation;
} NativeArgConstraint;

extern const NativeArgConstraint nativeArgConstraints[];
extern const int nativeArgConstraintCount;

// Returns NULL, after reporting the error, if `signature` isn't a valid type.
Type *parseNativeSignature(TypeEnv *env, const char *signature);

void defineAllNativeSignatures(TypeEnv *env);

#endif
