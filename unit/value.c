#include <assert.h>
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
TEST(test_number, NUMBER_VAL(123), "123.000000")
TEST(test_bool_true, BOOL_VAL(true), "true")
TEST(test_bool_false, BOOL_VAL(false), "false")

int main(void) {
  test_nil();
  test_number();
  test_bool_true();
  test_bool_false();

  return 0;
}
