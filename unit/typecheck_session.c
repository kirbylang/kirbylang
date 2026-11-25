#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/typecheck.h"
#include "../src/types.h"

// Checks a unit the way main.c does: the source buffer and the AST arena
// are both released before the next unit is checked, so anything the
// session kept has to have outlived them.
static bool checkUnitParsedAs(const char *source, bool parsedAsStdlib,
                              bool isStdlib) {
  size_t length = strlen(source);
  char *owned = (char *)malloc(length + 1);
  assert(owned != NULL);
  memcpy(owned, source, length + 1);

  int count = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parsedAsStdlib
                      ? parseStdlib(owned, &count, &hadParseError, &endLine)
                      : parse(owned, &count, &hadParseError, &endLine);
  assert(!hadParseError);

  bool ok =
      isStdlib ? typchkCheckStdlib(ast, count) : typchkCheckProgram(ast, count);

  astFreeAll();
  free(ast);
  free(owned);

  return ok;
}

static bool checkUnit(const char *source) {
  return checkUnitParsedAs(source, false, false);
}

static bool checkStdlibUnit(const char *source) {
  return checkUnitParsedAs(source, true, true);
}

// A unit the parser allowed to write a primitive impl, checked as user code
static bool checkStdlibParsedUnitAsUser(const char *source) {
  return checkUnitParsedAs(source, true, false);
}

static bool userParseFails(const char *source) {
  int count = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadParseError, &endLine);
  astFreeAll();
  free(ast);
  return hadParseError;
}

static bool traitIsBuiltin(const char *name) {
  Type *trait =
      typchkTypeEnvLookupTrait(typchkSessionEnv(), tokenFromCString(name));
  assert(trait != NULL);
  return typeTraitIsBuiltin(trait);
}

static void test_function_signatures_survive_the_unit_that_declared_them(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkUnit("fun double(x: f64): f64 = x * 2;"));

  typchkResetError();
  assert(!checkUnit("@println(double(1, 2));"));

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

  assert(checkUnit("let p: Point = Point.new(1);\n@println(p.x);"));

  typchkResetError();
  assert(!checkUnit("@println(Point.new(1).nope);"));

  typchkSessionEnd();
  typchkResetError();
}

static void test_an_error_in_one_unit_doesnt_fail_the_next(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(!checkUnit("let x: f64 = \"no\";"));
  assert(checkUnit("@println(1 + 2);"));

  typchkSessionEnd();
}

static void test_units_are_independent_without_a_session(void) {
  typchkResetError();

  assert(checkUnit("fun double(x: f64): f64 = x * 2;"));

  // No session, so the second unit never learned `double` -- it falls
  // through to the unresolved-callee path and stays unchecked.
  assert(checkUnit("@println(double(1, 2));"));

  typchkResetError();
}

static void test_traits_declared_by_the_stdlib_are_builtin(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkStdlibUnit("trait Shape { fun area(self): f64; }"));
  assert(checkUnit("trait Mine { fun area(self): f64; }"));

  assert(traitIsBuiltin("Shape"));
  assert(!traitIsBuiltin("Mine"));

  // A later unit can use the stdlib's trait but not declare it again
  assert(checkUnit("struct Square {}\n"
                   "impl Shape for Square {\n"
                   "  fun area(self): f64 = 1;\n"
                   "}"));

  typchkResetError();
  assert(!checkUnit("trait Shape { fun area(self): f64; }"));

  typchkSessionEnd();
  typchkResetError();
}

static void test_only_the_stdlib_adds_methods_to_primitives(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkStdlibUnit("impl string {\n"
                         "  pub fun twice(self): i64 = @strLen(self) * 2;\n"
                         "}"));

  // The method outlives the unit that declared it
  assert(checkUnit("@println(\"ab\".twice());"));

  // The parser refuses it for user code, and so does the checker
  typchkResetError();
  assert(userParseFails("impl string {\n"
                        "  pub fun thrice(self): i64 = 3;\n"
                        "}"));
  assert(!checkStdlibParsedUnitAsUser("impl string {\n"
                                      "  pub fun thrice(self): i64 = 3;\n"
                                      "}"));

  typchkResetError();
  assert(!checkUnit("@println(\"ab\".thrice());"));

  // Every primitive can have methods
  typchkResetError();
  assert(checkStdlibUnit("impl f64 {\n"
                         "  pub fun one(self): f64 = 1;\n"
                         "}"));
  assert(checkStdlibUnit("impl u8 {\n"
                         "  pub fun one(self): u8 = 1;\n"
                         "}"));
  assert(checkStdlibUnit("impl bool {\n"
                         "  pub fun yes(self): bool = true;\n"
                         "}"));

  assert(checkStdlibUnit("impl unit {\n"
                         "  pub fun nothing(self): unit {}\n"
                         "}"));

  typchkResetError();
  assert(userParseFails("impl unit {\n"
                        "  pub fun nothing(self): unit {}\n"
                        "}"));
  assert(!checkStdlibParsedUnitAsUser("impl unit {\n"
                                      "  pub fun nothing(self): unit {}\n"
                                      "}"));

  typchkSessionEnd();
  typchkResetError();
}

static void test_self_in_a_primitive_impl_is_the_primitive(void) {
  typchkResetError();
  typchkSessionBegin();

  assert(checkStdlibUnit(
      "impl string {\n"
      "  pub fun same(self, other: Self): bool = @strLen(self) == "
      "@strLen(other);\n"
      "  pub fun me(self): Self {\n"
      "    let copy: Self = self;\n"
      "    copy\n"
      "  }\n"
      "}"));

  assert(checkUnit("let s: string = \"ab\".me();\n"
                   "@println(s.same(\"cd\"));"));

  typchkResetError();
  assert(!checkUnit("@println(\"ab\".same(1));"));

  typchkSessionEnd();
  typchkResetError();
}

int main(void) {
  test_self_in_a_primitive_impl_is_the_primitive();
  test_only_the_stdlib_adds_methods_to_primitives();
  test_traits_declared_by_the_stdlib_are_builtin();
  test_function_signatures_survive_the_unit_that_declared_them();
  test_struct_types_survive_the_unit_that_declared_them();
  test_an_error_in_one_unit_doesnt_fail_the_next();
  test_units_are_independent_without_a_session();

  typesFreeAll();
  astFreeAll();

  printf("typecheck session tests passed\n");
  return 0;
}
