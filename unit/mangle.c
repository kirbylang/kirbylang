#include <assert.h>
#include <string.h>

#include "../src/mangle.h"
#include "../src/token.h"

static Token makeToken(const char *text) {
  Token t;
  t.type = TOKEN_IDENTIFIER;
  t.start = text;
  t.length = (int)strlen(text);
  t.line = 1;
  return t;
}

static void test_plain_method_has_no_trait_segment(void) {
  char buffer[MANGLED_NAME_MAX];
  Token method = makeToken("helper");

  int len = mangledPrimitiveMethodName(buffer, "f64", 3, NULL, &method);

  assert(len == (int)strlen("@f64.helper"));
  assert(strcmp(buffer, "@f64.helper") == 0);
}

static void test_trait_method_includes_trait_segment(void) {
  char buffer[MANGLED_NAME_MAX];
  Token trait = makeToken("Display");
  Token method = makeToken("toString");

  int len = mangledPrimitiveMethodName(buffer, "f64", 3, &trait, &method);

  assert(len == (int)strlen("@f64.Display.toString"));
  assert(strcmp(buffer, "@f64.Display.toString") == 0);
}

static void test_different_primitive_kinds(void) {
  char buffer[MANGLED_NAME_MAX];
  Token method = makeToken("default");

  mangledPrimitiveMethodName(buffer, "string", 6, NULL, &method);
  assert(strcmp(buffer, "@string.default") == 0);

  mangledPrimitiveMethodName(buffer, "bool", 4, NULL, &method);
  assert(strcmp(buffer, "@bool.default") == 0);

  mangledPrimitiveMethodName(buffer, "unit", 4, NULL, &method);
  assert(strcmp(buffer, "@unit.default") == 0);
}

// Same (primitive, method) pair, plain vs. trait -- the trait segment is
// the only thing that can possibly disambiguate them, so it must always
// be present when a trait name is given, never dropped or merged.
static void test_plain_and_trait_names_never_collide(void) {
  char plainBuffer[MANGLED_NAME_MAX];
  char traitBuffer[MANGLED_NAME_MAX];
  Token trait = makeToken("Display");
  Token method = makeToken("toString");

  mangledPrimitiveMethodName(plainBuffer, "f64", 3, NULL, &method);
  mangledPrimitiveMethodName(traitBuffer, "f64", 3, &trait, &method);

  assert(strcmp(plainBuffer, traitBuffer) != 0);
}

static void test_result_always_null_terminated_and_in_bounds(void) {
  char buffer[MANGLED_NAME_MAX];
  // Longer than any real Kirby identifier could be, to exercise the
  // truncation path without ever actually overflowing `buffer`.
  char longName[300];
  memset(longName, 'a', sizeof(longName) - 1);
  longName[sizeof(longName) - 1] = '\0';

  Token trait = makeToken(longName);
  Token method = makeToken(longName);

  int len = mangledPrimitiveMethodName(buffer, "f64", 3, &trait, &method);

  assert(len >= 0);
  assert(len < MANGLED_NAME_MAX);
  assert((int)strlen(buffer) == len);
}

int main(void) {
  test_plain_method_has_no_trait_segment();
  test_trait_method_includes_trait_segment();
  test_different_primitive_kinds();
  test_plain_and_trait_names_never_collide();
  test_result_always_null_terminated_and_in_bounds();

  return 0;
}
