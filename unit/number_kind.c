#include <assert.h>
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

int main(void) {
  test_numbers_are_fixed();
  test_every_kind_has_a_name();
  return 0;
}
