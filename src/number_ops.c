#include "number_ops.h"

#include <math.h>

// Truncate a raw unsigned 64bit integer into its number kind's width
static NumberPayload truncateRawInteger(NumberKind kind, uint64_t raw) {
  const NumberKindInfo *info = numberKindInfo(kind);
  NumberPayload result = {.u64 = raw};

  if (info->size == 8)
    return result;

  uint64_t bits = (uint64_t)info->size * 8;
  uint64_t mask = ((uint64_t)1 << bits) - 1;
  raw &= mask;

  if (info->isSigned) {
    uint64_t signBit = (uint64_t)1 << (bits - 1);
    raw = (raw ^ signBit) - signBit;
  }

  result.u64 = raw;
  return result;
}

static double roundFloat(NumberKind kind, double value) {
  return kind == NUMBER_KIND_F32 ? (double)(float)value : value;
}

bool numberIsZero(NumberKind kind, NumberPayload value) {
  if (numberKindInfo(kind)->isFloat)
    return value.f64 == 0;

  return value.u64 == 0;
}

static NumberPayload floatArithmetic(NumberKind kind, NumberOp op, double a,
                                     double b) {
  double result = 0;

  switch (op) {
  case NUMBER_OP_ADD:
    result = a + b;
    break;
  case NUMBER_OP_SUBTRACT:
    result = a - b;
    break;
  case NUMBER_OP_MULTIPLY:
    result = a * b;
    break;
  case NUMBER_OP_DIVIDE:
    result = a / b;
    break;
  case NUMBER_OP_MODULO:
    result = fmod(a, b);
    break;
  }

  return (NumberPayload){.f64 = roundFloat(kind, result)};
}

static NumberPayload signedArithmetic(NumberKind kind, NumberOp op,
                                      NumberPayload a, NumberPayload b) {
  // Get the raw 64bit representation of the integers
  uint64_t a_raw = a.u64;
  uint64_t b_raw = b.u64;

  switch (op) {
  case NUMBER_OP_ADD:
    return truncateRawInteger(kind, a_raw + b_raw);
  case NUMBER_OP_SUBTRACT:
    return truncateRawInteger(kind, a_raw - b_raw);
  case NUMBER_OP_MULTIPLY:
    return truncateRawInteger(kind, a_raw * b_raw);
  case NUMBER_OP_DIVIDE:
    if (b.i64 == -1)
      return truncateRawInteger(kind, 0 - a_raw);

    return truncateRawInteger(kind, (uint64_t)(a.i64 / b.i64));
  case NUMBER_OP_MODULO:
    if (b.i64 == -1)
      return (NumberPayload){.i64 = 0};

    return truncateRawInteger(kind, (uint64_t)(a.i64 % b.i64));
  }

  return a;
}

static NumberPayload unsignedArithmetic(NumberKind kind, NumberOp op,
                                        NumberPayload a, NumberPayload b) {
  switch (op) {
  case NUMBER_OP_ADD:
    return truncateRawInteger(kind, a.u64 + b.u64);
  case NUMBER_OP_SUBTRACT:
    return truncateRawInteger(kind, a.u64 - b.u64);
  case NUMBER_OP_MULTIPLY:
    return truncateRawInteger(kind, a.u64 * b.u64);
  case NUMBER_OP_DIVIDE:
    return truncateRawInteger(kind, a.u64 / b.u64);
  case NUMBER_OP_MODULO:
    return truncateRawInteger(kind, a.u64 % b.u64);
  }

  return a;
}

NumberPayload numberArithmetic(NumberKind kind, NumberOp op, NumberPayload a,
                               NumberPayload b) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat)
    return floatArithmetic(kind, op, a.f64, b.f64);

  if (info->isSigned)
    return signedArithmetic(kind, op, a, b);

  return unsignedArithmetic(kind, op, a, b);
}

NumberPayload numberNegateValue(NumberKind kind, NumberPayload value) {
  if (numberKindInfo(kind)->isFloat)
    return (NumberPayload){.f64 = -value.f64};

  return truncateRawInteger(kind, 0 - value.u64);
}

bool numberOpLessThan(NumberKind kind, NumberPayload a, NumberPayload b) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat)
    return a.f64 < b.f64;

  if (info->isSigned)
    return a.i64 < b.i64;

  return a.u64 < b.u64;
}

static NumberCastResult numberCastFloatToInteger(double value, NumberKind to,
                                                 NumberPayload *result) {
  const NumberKindInfo *info = numberKindInfo(to);

  if (isnan(value) || (!isinf(value) && value != floor(value)))
    return NUMBER_CAST_NOT_WHOLE;

  int bits = info->size * 8 - (info->isSigned ? 1 : 0);
  double upper = ldexp(1.0, bits);
  double lower = info->isSigned ? -upper : 0.0;

  if (value < lower || value >= upper)
    return NUMBER_CAST_OUT_OF_RANGE;

  if (info->isSigned)
    result->i64 = (int64_t)value;
  else
    result->u64 = (uint64_t)value;

  return NUMBER_CAST_OK;
}

static NumberCastResult numberCastIntegerToInteger(NumberKind from,
                                                   NumberPayload value,
                                                   NumberKind to,
                                                   NumberPayload *result) {
  const NumberKindInfo *source = numberKindInfo(from);
  const NumberKindInfo *target = numberKindInfo(to);
  bool negative = source->isSigned && value.i64 < 0;

  if (negative) {
    if (!target->isSigned || value.i64 < numberKindMin(to))
      return NUMBER_CAST_OUT_OF_RANGE;
  } else if (value.u64 > numberKindMax(to)) {
    return NUMBER_CAST_OUT_OF_RANGE;
  }

  *result = value;
  return NUMBER_CAST_OK;
}

NumberCastResult numberCast(NumberKind from, NumberPayload value, NumberKind to,
                            NumberPayload *result) {
  const NumberKindInfo *source = numberKindInfo(from);
  const NumberKindInfo *target = numberKindInfo(to);

  if (target->isFloat) {
    double converted = numberToDouble(from, value);
    double rounded = roundFloat(to, converted);

    if (isinf(rounded) && !isinf(converted))
      return NUMBER_CAST_OUT_OF_RANGE;

    result->f64 = rounded;
    return NUMBER_CAST_OK;
  }

  if (source->isFloat)
    return numberCastFloatToInteger(value.f64, to, result);

  return numberCastIntegerToInteger(from, value, to, result);
}
