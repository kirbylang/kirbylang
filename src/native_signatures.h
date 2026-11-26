#ifndef kirby_native_signatures_h
#define kirby_native_signatures_h

#include <stdbool.h>

// Limits on a native argument's value that its type can't express. The types
// are declared in the standard library.

typedef bool (*NativeArgPredicate)(double value);

typedef struct {
  const char *name;
  int paramIndex; // Zero-based
  NativeArgPredicate isValid;
  const char *expectation;
} NativeArgConstraint;

extern const NativeArgConstraint nativeArgConstraints[];
extern const int nativeArgConstraintCount;

#endif
