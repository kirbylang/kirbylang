#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../src/value.h"

#define TEST(name, value, expected)                                            \
  static void name() {                                                         \
    char buffer[sizeof(expected)];                                             \
    valueToString(value, buffer, sizeof(buffer));                              \
    assert(strcmp(expected, buffer) == 0);                                     \
  }

TEST(test_nil, NIL_VAL, "nil")
TEST(test_number, NUMBER_VAL(123), "123")
TEST(test_bool_true, BOOL_VAL(true), "true")
TEST(test_bool_false, BOOL_VAL(false), "false")

// Shortest digits that read back as the same number, in plain decimal form
// for 1e-6 <= |x| < 1e21 and exponent form outside it.
static void test_format_number(void) {
  struct {
    double value;
    const char *expected;
  } cases[] = {
      {0, "0"},
      {-0.0, "-0"},
      {3, "3"},
      {10, "10"},
      {100, "100"},
      {-1.5, "-1.5"},
      {123.456, "123.456"},
      {0.1 + 0.2, "0.30000000000000004"},
      {1.0 / 3, "0.3333333333333333"},
      {0.000001, "0.000001"},
      {0.000025, "0.000025"},
      {1e-7, "1e-7"},
      {1e20, "100000000000000000000"},
      {123456789012345678.0, "123456789012345680"},
      {1e21, "1e+21"},
      {1.5e300, "1.5e+300"},
      {5e-324, "5e-324"},
      {INFINITY, "inf"},
      {-INFINITY, "-inf"},
      {NAN, "nan"},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    char buffer[NUMBER_STRING_MAX];
    formatNumber(cases[i].value, buffer, sizeof(buffer));

    if (strcmp(cases[i].expected, buffer) != 0) {
      fprintf(stderr, "formatNumber: expected %s, got %s\n",
              cases[i].expected, buffer);
    }
    assert(strcmp(cases[i].expected, buffer) == 0);
  }
}

int main(void) {
  test_format_number();
  test_nil();
  test_number();
  test_bool_true();
  test_bool_false();

  return 0;
}
