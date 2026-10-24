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

// Each integer operation stores its exact result in `result` and returns
// true when the result fits `kind`, or returns false when it doesn't. The
// checks run before the operation, so C never overflows.

static bool signedArithmetic(NumberKind kind, NumberOp op, int64_t a, int64_t b,
                             NumberPayload *result) {
  // i64 is the common case, so skip looking up its range
  bool isI64 = kind == NUMBER_KIND_I64;
  int64_t min = isI64 ? INT64_MIN : numberKindMin(kind);
  int64_t max = isI64 ? INT64_MAX : (int64_t)numberKindMax(kind);
  int64_t value = 0;

  switch (op) {
  case NUMBER_OP_ADD:
    if ((b > 0 && a > max - b) || (b < 0 && a < min - b))
      return false;
    value = a + b;
    break;
  case NUMBER_OP_SUBTRACT:
    if ((b < 0 && a > max + b) || (b > 0 && a < min + b))
      return false;
    value = a - b;
    break;
  case NUMBER_OP_MULTIPLY:
    if (a > 0 ? (b > 0 ? a > max / b : b < min / a)
              : (b > 0 ? a < min / b : a != 0 && b < max / a))
      return false;
    value = a * b;
    break;
  case NUMBER_OP_DIVIDE:
    if (a == min && b == -1)
      return false;
    value = a / b;
    break;
  case NUMBER_OP_MODULO:
    // `min % -1` is 0, but C can't work it out
    value = b == -1 ? 0 : a % b;
    break;
  }

  *result = truncateRawInteger(kind, (uint64_t)value);
  return true;
}

static bool unsignedArithmetic(NumberKind kind, NumberOp op, uint64_t a,
                               uint64_t b, NumberPayload *result) {
  uint64_t max = numberKindMax(kind);
  uint64_t value = 0;

  switch (op) {
  case NUMBER_OP_ADD:
    if (b > max - a)
      return false;
    value = a + b;
    break;
  case NUMBER_OP_SUBTRACT:
    if (b > a)
      return false;
    value = a - b;
    break;
  case NUMBER_OP_MULTIPLY:
    if (a != 0 && b > max / a)
      return false;
    value = a * b;
    break;
  case NUMBER_OP_DIVIDE:
    value = a / b;
    break;
  case NUMBER_OP_MODULO:
    value = a % b;
    break;
  }

  *result = truncateRawInteger(kind, value);
  return true;
}

bool numberArithmetic(NumberKind kind, NumberOp op, NumberPayload a,
                      NumberPayload b, NumberPayload *result) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat) {
    *result = floatArithmetic(kind, op, a.f64, b.f64);
    return true;
  }

  if (info->isSigned)
    return signedArithmetic(kind, op, a.i64, b.i64, result);

  return unsignedArithmetic(kind, op, a.u64, b.u64, result);
}

bool numberNegateValue(NumberKind kind, NumberPayload value,
                       NumberPayload *result) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat) {
    *result = (NumberPayload){.f64 = -value.f64};
    return true;
  }

  if (info->isSigned)
    return signedArithmetic(kind, NUMBER_OP_SUBTRACT, 0, value.i64, result);

  return unsignedArithmetic(kind, NUMBER_OP_SUBTRACT, 0, value.u64, result);
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
