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

// Perform arithmetic on number kinds. Returns false when an integer result
// doesn't fit the kind, and leaves `result` unchanged.
//
// The caller must reject a zero integer divisor first.
bool numberArithmetic(NumberKind kind, NumberOp op, NumberPayload a,
                      NumberPayload b, NumberPayload *result);

// Returns false when the negated integer doesn't fit the kind, such as the
// smallest i64, and leaves `result` unchanged.
bool numberNegateValue(NumberKind kind, NumberPayload value,
                       NumberPayload *result);

bool numberOpLessThan(NumberKind kind, NumberPayload a, NumberPayload b);

// Cast from one number kind to another at runtime
NumberCastResult numberCast(NumberKind from, NumberPayload value, NumberKind to,
                            NumberPayload *result);

#endif
