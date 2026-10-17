#ifndef kirby_number_ops_h
#define kirby_number_ops_h

#include "number_kind.h"

typedef enum {
  NUMBER_OP_ADD,
  NUMBER_OP_SUBTRACT,
  NUMBER_OP_MULTIPLY,
  NUMBER_OP_DIVIDE,
  NUMBER_OP_MODULO,
} NumberOp;

typedef enum {
  NUMBER_CAST_OK,
  NUMBER_CAST_NOT_WHOLE,
  NUMBER_CAST_OUT_OF_RANGE,
} NumberCastResult;

bool numberIsZero(NumberKind kind, NumberPayload value);

// Integers wrap on overflow and divide toward zero. The caller must reject a
// zero integer divisor first.
NumberPayload numberArithmetic(NumberKind kind, NumberOp op, NumberPayload a,
                               NumberPayload b);

NumberPayload numberNegateValue(NumberKind kind, NumberPayload value);

bool numberOpLessThan(NumberKind kind, NumberPayload a, NumberPayload b);

// Floats must be whole and in range to become integers, and integers must be
// in range. Converting to a float rounds, and fails only on overflow.
NumberCastResult numberCast(NumberKind from, NumberPayload value, NumberKind to,
                            NumberPayload *result);

#endif
