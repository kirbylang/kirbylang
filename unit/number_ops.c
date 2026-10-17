#include <assert.h>
#include <math.h>
#include <stdbool.h>

#include "../src/number_ops.h"

#define I64(v) ((NumberPayload){.i64 = (v)})
#define U64(v) ((NumberPayload){.u64 = (v)})
#define F64(v) ((NumberPayload){.f64 = (v)})

static void test_unsigned_arithmetic_wraps(void) {
  assert(
      numberArithmetic(NUMBER_KIND_U8, NUMBER_OP_ADD, U64(200), U64(100)).u64 ==
      44);
  assert(numberArithmetic(NUMBER_KIND_U8, NUMBER_OP_SUBTRACT, U64(1), U64(2))
             .u64 == 255);
  assert(numberArithmetic(NUMBER_KIND_U8, NUMBER_OP_MULTIPLY, U64(16), U64(17))
             .u64 == 16);
  assert(numberArithmetic(NUMBER_KIND_U16, NUMBER_OP_ADD, U64(65535), U64(2))
             .u64 == 1);
  assert(numberArithmetic(NUMBER_KIND_U32, NUMBER_OP_SUBTRACT, U64(0), U64(1))
             .u64 == 4294967295u);
  assert(
      numberArithmetic(NUMBER_KIND_U64, NUMBER_OP_ADD, U64(UINT64_MAX), U64(2))
          .u64 == 1);
  assert(numberArithmetic(NUMBER_KIND_U64, NUMBER_OP_DIVIDE, U64(UINT64_MAX),
                          U64(2))
             .u64 == 9223372036854775807ull);
  assert(numberArithmetic(NUMBER_KIND_U8, NUMBER_OP_DIVIDE, U64(255), U64(2))
             .u64 == 127);
  assert(numberArithmetic(NUMBER_KIND_U8, NUMBER_OP_MODULO, U64(255), U64(100))
             .u64 == 55);
}

static void test_signed_arithmetic_wraps(void) {
  assert(
      numberArithmetic(NUMBER_KIND_I8, NUMBER_OP_ADD, I64(127), I64(1)).i64 ==
      -128);
  assert(numberArithmetic(NUMBER_KIND_I8, NUMBER_OP_SUBTRACT, I64(-128), I64(1))
             .i64 == 127);
  assert(numberArithmetic(NUMBER_KIND_I8, NUMBER_OP_MULTIPLY, I64(64), I64(2))
             .i64 == -128);
  assert(numberArithmetic(NUMBER_KIND_I16, NUMBER_OP_ADD, I64(32767), I64(1))
             .i64 == -32768);
  assert(
      numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_ADD, I64(2147483647), I64(1))
          .i64 == -2147483648ll);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_MULTIPLY, I64(65536),
                          I64(65536))
             .i64 == 0);
  assert(
      numberArithmetic(NUMBER_KIND_I64, NUMBER_OP_ADD, I64(INT64_MAX), I64(1))
          .i64 == INT64_MIN);
  assert(numberArithmetic(NUMBER_KIND_I64, NUMBER_OP_SUBTRACT, I64(INT64_MIN),
                          I64(1))
             .i64 == INT64_MAX);
}

static void test_signed_division_truncates_toward_zero(void) {
  assert(
      numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_DIVIDE, I64(7), I64(2)).i64 ==
      3);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_DIVIDE, I64(-7), I64(2))
             .i64 == -3);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_DIVIDE, I64(7), I64(-2))
             .i64 == -3);
  assert(
      numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_MODULO, I64(7), I64(2)).i64 ==
      1);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_MODULO, I64(-7), I64(2))
             .i64 == -1);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_MODULO, I64(7), I64(-2))
             .i64 == 1);
}

static void test_signed_minimum_divided_by_minus_one(void) {
  assert(numberArithmetic(NUMBER_KIND_I64, NUMBER_OP_DIVIDE, I64(INT64_MIN),
                          I64(-1))
             .i64 == INT64_MIN);
  assert(numberArithmetic(NUMBER_KIND_I64, NUMBER_OP_MODULO, I64(INT64_MIN),
                          I64(-1))
             .i64 == 0);
  assert(numberArithmetic(NUMBER_KIND_I8, NUMBER_OP_DIVIDE, I64(-128), I64(-1))
             .i64 == -128);
  assert(numberArithmetic(NUMBER_KIND_I8, NUMBER_OP_MODULO, I64(-128), I64(-1))
             .i64 == 0);
  assert(numberArithmetic(NUMBER_KIND_I32, NUMBER_OP_DIVIDE, I64(-2147483648ll),
                          I64(-1))
             .i64 == -2147483648ll);
}

static void test_float_arithmetic(void) {
  assert(numberArithmetic(NUMBER_KIND_F64, NUMBER_OP_ADD, F64(1.5), F64(2.25))
             .f64 == 3.75);
  assert(
      numberArithmetic(NUMBER_KIND_F64, NUMBER_OP_DIVIDE, F64(7), F64(2)).f64 ==
      3.5);
  assert(numberArithmetic(NUMBER_KIND_F64, NUMBER_OP_MODULO, F64(7.5), F64(2))
             .f64 == 1.5);
  assert(isnan(
      numberArithmetic(NUMBER_KIND_F64, NUMBER_OP_MODULO, F64(1), F64(0)).f64));
  assert(numberArithmetic(NUMBER_KIND_F32, NUMBER_OP_ADD, F64(16777216.0),
                          F64(1.0))
             .f64 == 16777216.0);
  assert(
      numberArithmetic(NUMBER_KIND_F32, NUMBER_OP_DIVIDE, F64(1), F64(3)).f64 ==
      (double)(1.0f / 3.0f));
}

static void test_negate(void) {
  assert(numberNegateValue(NUMBER_KIND_F64, F64(1.5)).f64 == -1.5);
  assert(numberNegateValue(NUMBER_KIND_I32, I64(5)).i64 == -5);
  assert(numberNegateValue(NUMBER_KIND_I32, I64(-2147483648ll)).i64 ==
         -2147483648ll);
  assert(numberNegateValue(NUMBER_KIND_I8, I64(-128)).i64 == -128);
  assert(numberNegateValue(NUMBER_KIND_I64, I64(INT64_MIN)).i64 == INT64_MIN);
}

static void test_is_zero(void) {
  assert(numberIsZero(NUMBER_KIND_U8, U64(0)));
  assert(!numberIsZero(NUMBER_KIND_U8, U64(1)));
  assert(numberIsZero(NUMBER_KIND_I32, I64(0)));
  assert(numberIsZero(NUMBER_KIND_F64, F64(0.0)));
  assert(numberIsZero(NUMBER_KIND_F64, F64(-0.0)));
  assert(!numberIsZero(NUMBER_KIND_F64, F64(0.5)));
}

static void test_less(void) {
  assert(numberOpLessThan(NUMBER_KIND_I32, I64(-1), I64(0)));
  assert(!numberOpLessThan(NUMBER_KIND_I32, I64(0), I64(-1)));
  assert(numberOpLessThan(NUMBER_KIND_U64, U64(1), U64(UINT64_MAX)));
  assert(!numberOpLessThan(NUMBER_KIND_U64, U64(UINT64_MAX), U64(1)));
  assert(numberOpLessThan(NUMBER_KIND_F64, F64(-0.5), F64(0.5)));
  assert(!numberOpLessThan(NUMBER_KIND_F64, F64(NAN), F64(1)));
  assert(!numberOpLessThan(NUMBER_KIND_F64, F64(1), F64(NAN)));
}

static NumberCastResult cast(NumberKind from, NumberPayload v, NumberKind to,
                             NumberPayload *out) {
  return numberCast(from, v, to, out);
}

static void test_cast_float_to_integer(void) {
  NumberPayload out;

  assert(cast(NUMBER_KIND_F64, F64(200), NUMBER_KIND_U8, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == 200);
  assert(cast(NUMBER_KIND_F64, F64(255), NUMBER_KIND_U8, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == 255);
  assert(cast(NUMBER_KIND_F64, F64(256), NUMBER_KIND_U8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(-1), NUMBER_KIND_U8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(1.5), NUMBER_KIND_U8, &out) ==
         NUMBER_CAST_NOT_WHOLE);
  assert(cast(NUMBER_KIND_F64, F64(NAN), NUMBER_KIND_I32, &out) ==
         NUMBER_CAST_NOT_WHOLE);
  assert(cast(NUMBER_KIND_F64, F64(INFINITY), NUMBER_KIND_I32, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(-129), NUMBER_KIND_I8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(-128), NUMBER_KIND_I8, &out) ==
             NUMBER_CAST_OK &&
         out.i64 == -128);
  assert(cast(NUMBER_KIND_F64, F64(127), NUMBER_KIND_I8, &out) ==
             NUMBER_CAST_OK &&
         out.i64 == 127);
  assert(cast(NUMBER_KIND_F64, F64(128), NUMBER_KIND_I8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(9223372036854775808.0), NUMBER_KIND_I64,
              &out) == NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(-9223372036854775808.0), NUMBER_KIND_I64,
              &out) == NUMBER_CAST_OK &&
         out.i64 == INT64_MIN);
  assert(cast(NUMBER_KIND_F64, F64(18446744073709551616.0), NUMBER_KIND_U64,
              &out) == NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(4294967295.0), NUMBER_KIND_U32, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == 4294967295u);
  assert(cast(NUMBER_KIND_F64, F64(-0.0), NUMBER_KIND_U8, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == 0);
}

static void test_cast_integer_to_integer(void) {
  NumberPayload out;

  assert(cast(NUMBER_KIND_I32, I64(-1), NUMBER_KIND_U8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_I32, I64(255), NUMBER_KIND_U8, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == 255);
  assert(cast(NUMBER_KIND_I32, I64(256), NUMBER_KIND_U8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_U8, U64(200), NUMBER_KIND_I8, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_U8, U64(127), NUMBER_KIND_I8, &out) ==
             NUMBER_CAST_OK &&
         out.i64 == 127);
  assert(cast(NUMBER_KIND_I8, I64(-5), NUMBER_KIND_I64, &out) ==
             NUMBER_CAST_OK &&
         out.i64 == -5);
  assert(cast(NUMBER_KIND_U64, U64(UINT64_MAX), NUMBER_KIND_I64, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_I64, I64(-1), NUMBER_KIND_U64, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_I64, I64(INT64_MAX), NUMBER_KIND_U64, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == (uint64_t)INT64_MAX);
  assert(cast(NUMBER_KIND_U64, U64(UINT64_MAX), NUMBER_KIND_U64, &out) ==
             NUMBER_CAST_OK &&
         out.u64 == UINT64_MAX);
  assert(cast(NUMBER_KIND_I16, I64(-32768), NUMBER_KIND_I16, &out) ==
             NUMBER_CAST_OK &&
         out.i64 == -32768);
}

static void test_cast_to_float(void) {
  NumberPayload out;

  assert(cast(NUMBER_KIND_U8, U64(200), NUMBER_KIND_F64, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == 200.0);
  assert(cast(NUMBER_KIND_I32, I64(-7), NUMBER_KIND_F64, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == -7.0);
  assert(cast(NUMBER_KIND_U64, U64(UINT64_MAX), NUMBER_KIND_F64, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == 18446744073709551616.0);
  assert(cast(NUMBER_KIND_I32, I64(16777217), NUMBER_KIND_F32, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == 16777216.0);
  assert(cast(NUMBER_KIND_F64, F64(0.1), NUMBER_KIND_F32, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == (double)0.1f);
  assert(cast(NUMBER_KIND_F64, F64(1e300), NUMBER_KIND_F32, &out) ==
         NUMBER_CAST_OUT_OF_RANGE);
  assert(cast(NUMBER_KIND_F64, F64(INFINITY), NUMBER_KIND_F32, &out) ==
             NUMBER_CAST_OK &&
         isinf(out.f64));
  assert(cast(NUMBER_KIND_F32, F64(0.5), NUMBER_KIND_F64, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == 0.5);
  assert(cast(NUMBER_KIND_F64, F64(1.5), NUMBER_KIND_F64, &out) ==
             NUMBER_CAST_OK &&
         out.f64 == 1.5);
}

int main(void) {
  test_unsigned_arithmetic_wraps();
  test_signed_arithmetic_wraps();
  test_signed_division_truncates_toward_zero();
  test_signed_minimum_divided_by_minus_one();
  test_float_arithmetic();
  test_negate();
  test_is_zero();
  test_less();
  test_cast_float_to_integer();
  test_cast_integer_to_integer();
  test_cast_to_float();
  return 0;
}
