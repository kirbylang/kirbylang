#include <stddef.h>

#include "native_signatures.h"

static bool _isNonNegative(double value) { return value >= 0; }
static bool _isLessThanOrEqualTo255(double value) { return value <= 255; }

const NativeArgConstraint nativeArgConstraints[] = {
    {"@sqrt", 0, _isNonNegative, "a non-negative number"},
    {"@exit", 0, _isNonNegative, "a non-negative number"},
    {"@exit", 0, _isLessThanOrEqualTo255, "a number less than or equal to 255"},
};

const int nativeArgConstraintCount =
    (int)(sizeof(nativeArgConstraints) / sizeof(nativeArgConstraints[0]));
