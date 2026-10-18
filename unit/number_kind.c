#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "../src/number_kind.h"

static void test_numbers_are_fixed(void) {
  assert(NUMBER_KIND_I64 == 0);
  assert(NUMBER_KIND_I32 == 1);
  assert(NUMBER_KIND_I16 == 2);
  assert(NUMBER_KIND_I8 == 3);
  assert(NUMBER_KIND_U64 == 4);
  assert(NUMBER_KIND_U32 == 5);
  assert(NUMBER_KIND_U16 == 6);
  assert(NUMBER_KIND_U8 == 7);
  assert(NUMBER_KIND_F64 == 8);
  assert(NUMBER_KIND_F32 == 9);
  assert(NUMBER_KIND_COUNT == 10);
}

static void test_every_kind_has_a_name(void) {
  struct {
    NumberKind kind;
    const char *name;
  } cases[] = {
      {NUMBER_KIND_I64, "i64"}, {NUMBER_KIND_I32, "i32"},
      {NUMBER_KIND_I16, "i16"}, {NUMBER_KIND_I8, "i8"},
      {NUMBER_KIND_U64, "u64"}, {NUMBER_KIND_U32, "u32"},
      {NUMBER_KIND_U16, "u16"}, {NUMBER_KIND_U8, "u8"},
      {NUMBER_KIND_F64, "f64"}, {NUMBER_KIND_F32, "f32"},
  };

  assert(sizeof(cases) / sizeof(cases[0]) == NUMBER_KIND_COUNT);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    assert(strcmp(numberKindInfo(cases[i].kind)->name, cases[i].name) == 0);
  }
}

static void test_sizes_and_signs(void) {
  struct {
    NumberKind kind;
    int size;
    bool isSigned;
    bool isFloat;
  } cases[] = {
      {NUMBER_KIND_I64, 8, true, false},  {NUMBER_KIND_I32, 4, true, false},
      {NUMBER_KIND_I16, 2, true, false},  {NUMBER_KIND_I8, 1, true, false},
      {NUMBER_KIND_U64, 8, false, false}, {NUMBER_KIND_U32, 4, false, false},
      {NUMBER_KIND_U16, 2, false, false}, {NUMBER_KIND_U8, 1, false, false},
      {NUMBER_KIND_F64, 8, true, true},   {NUMBER_KIND_F32, 4, true, true},
  };

  assert(sizeof(cases) / sizeof(cases[0]) == NUMBER_KIND_COUNT);

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    const NumberKindInfo *info = numberKindInfo(cases[i].kind);
    assert(info->size == cases[i].size);
    assert(info->isSigned == cases[i].isSigned);
    assert(info->isFloat == cases[i].isFloat);
  }
}

static void test_supported_kinds(void) {
  for (int kind = 0; kind < NUMBER_KIND_COUNT; kind++) {
    bool expected = kind == NUMBER_KIND_F64 || kind == NUMBER_KIND_U8 ||
                    kind == NUMBER_KIND_I32 ||
                    kind == NUMBER_KIND_I64 || kind == NUMBER_KIND_U64;
    assert(numberKindInfo((NumberKind)kind)->supported == expected);
  }
}

static void test_number_to_double(void) {
  assert(numberToDouble(NUMBER_KIND_U8, (NumberPayload){.u64 = 200}) == 200.0);
  assert(numberToDouble(NUMBER_KIND_I32, (NumberPayload){.i64 = -5}) == -5.0);
  assert(numberToDouble(NUMBER_KIND_U64,
                        (NumberPayload){.u64 = 4294967296ull}) == 4294967296.0);
  assert(numberToDouble(NUMBER_KIND_F64, (NumberPayload){.f64 = 1.5}) == 1.5);
}

static void test_integer_limits(void) {
  assert(numberKindMin(NUMBER_KIND_I8) == -128);
  assert(numberKindMax(NUMBER_KIND_I8) == 127);
  assert(numberKindMin(NUMBER_KIND_I16) == -32768);
  assert(numberKindMax(NUMBER_KIND_I16) == 32767);
  assert(numberKindMin(NUMBER_KIND_I32) == INT32_MIN);
  assert(numberKindMax(NUMBER_KIND_I32) == INT32_MAX);
  assert(numberKindMin(NUMBER_KIND_I64) == INT64_MIN);
  assert(numberKindMax(NUMBER_KIND_I64) == INT64_MAX);
  assert(numberKindMin(NUMBER_KIND_U8) == 0);
  assert(numberKindMax(NUMBER_KIND_U8) == 255);
  assert(numberKindMax(NUMBER_KIND_U16) == 65535);
  assert(numberKindMax(NUMBER_KIND_U32) == UINT32_MAX);
  assert(numberKindMax(NUMBER_KIND_U64) == UINT64_MAX);
}

static void test_kind_from_name(void) {
  NumberKind kind;

  assert(numberKindFromName("u8", 2, &kind) && kind == NUMBER_KIND_U8);
  assert(numberKindFromName("f64", 3, &kind) && kind == NUMBER_KIND_F64);
  assert(numberKindFromName("u8x", 2, &kind) && kind == NUMBER_KIND_U8);
  assert(!numberKindFromName("u8x", 3, &kind));
  assert(numberKindFromName("i32", 3, &kind) && kind == NUMBER_KIND_I32);
  assert(numberKindFromName("i64", 3, &kind) && kind == NUMBER_KIND_I64);
  assert(numberKindFromName("u64", 3, &kind) && kind == NUMBER_KIND_U64);
  assert(!numberKindFromName("i16", 3, &kind));
  assert(!numberKindFromName("u", 1, &kind));
}

int main(void) {
  test_integer_limits();
  test_kind_from_name();
  test_numbers_are_fixed();
  test_every_kind_has_a_name();
  test_sizes_and_signs();
  test_supported_kinds();
  test_number_to_double();
  return 0;
}
