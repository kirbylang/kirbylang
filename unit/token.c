#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/token.h"

static void test_tokens_equal(void) {
  Token abc = tokenFromCString("abc");
  Token abcAgain = tokenFromCString("abc");
  Token abd = tokenFromCString("abd");
  Token ab = tokenFromCString("ab");

  assert(tokensEqual(&abc, &abcAgain));
  assert(!tokensEqual(&abc, &abd));
  assert(!tokensEqual(&abc, &ab));
}

static void test_token_text_equals(void) {
  Token self = tokenFromCString("Self");

  assert(tokenTextEquals(&self, "Self"));
  assert(!tokenTextEquals(&self, "Sel"));
  assert(!tokenTextEquals(&self, "Selff"));
  assert(!tokenTextEquals(&self, "self"));
}

static void test_token_from_c_string(void) {
  Token token = tokenFromCString("hello");

  assert(token.type == TOKEN_IDENTIFIER);
  assert(token.length == 5);
  assert(memcmp(token.start, "hello", 5) == 0);
  assert(token.line == 0);
}

static void test_primitive_type_names(void) {
  const char *primitives[] = {"unit", "bool", "string", "f64", "Array"};
  for (int i = 0; i < 5; i++) {
    Token name = tokenFromCString(primitives[i]);
    assert(tokenIsPrimitiveTypeName(&name));
  }

  Token point = tokenFromCString("Point");
  Token number = tokenFromCString("number");
  assert(!tokenIsPrimitiveTypeName(&point));
  assert(!tokenIsPrimitiveTypeName(&number));
}

int main(void) {
  assert(NumberOfDefinedTokens == 56);
  assert(strcmp(tokenTypeToString(TOKEN_AND), "TOKEN_AND") == 0);
  test_tokens_equal();
  test_token_text_equals();
  test_token_from_c_string();
  test_primitive_type_names();
  return 0;
}
