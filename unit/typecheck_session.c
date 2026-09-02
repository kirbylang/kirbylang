#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/parser.h"
#include "../src/typecheck.h"
#include "../src/types.h"

// Checks a unit the way main.c does: the source buffer and the AST arena
// are both released before the next unit is checked, so anything the
// session kept has to have outlived them.
static bool checkUnit(const char *source) {
  size_t length = strlen(source);
  char *owned = (char *)malloc(length + 1);
  assert(owned != NULL);
  memcpy(owned, source, length + 1);

  int count = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(owned, &count, &hadParseError, &endLine);
  assert(!hadParseError);

  bool ok = typchkCheckProgram(ast, count);

  astFreeAll();
  free(ast);
  free(owned);

  return ok;
}

static void test_function_signatures_survive_the_unit_that_declared_them(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkUnit("fun double(x: f64): f64 = x * 2;"));

  typchkResetError();
  assert(!checkUnit("print double(1, 2);"));

  typchkSessionEnd();
  typchkResetError();
}

static void test_struct_types_survive_the_unit_that_declared_them(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkUnit("struct Point {\n"
                   "  pub var x: f64;\n"
                   "}\n"
                   "impl Point {\n"
                   "  pub fun new(x: f64): Point = Point { x: x };\n"
                   "}"));

  assert(checkUnit("let p: Point = Point.new(1);\nprint p.x;"));

  typchkResetError();
  assert(!checkUnit("print Point.new(1).nope;"));

  typchkSessionEnd();
  typchkResetError();
}

static void test_an_error_in_one_unit_doesnt_fail_the_next(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(!checkUnit("let x: f64 = \"no\";"));
  assert(checkUnit("print 1 + 2;"));

  typchkSessionEnd();
}

static void test_units_are_independent_without_a_session(void) {
  typchkResetError();

  assert(checkUnit("fun double(x: f64): f64 = x * 2;"));

  // No session, so the second unit never learned `double` -- it falls
  // through to the unresolved-callee path and stays unchecked.
  assert(checkUnit("print double(1, 2);"));

  typchkResetError();
}

int main(void) {
  test_function_signatures_survive_the_unit_that_declared_them();
  test_struct_types_survive_the_unit_that_declared_them();
  test_an_error_in_one_unit_doesnt_fail_the_next();
  test_units_are_independent_without_a_session();

  typesFreeAll();
  astFreeAll();

  printf("typecheck session tests passed\n");
  return 0;
}
