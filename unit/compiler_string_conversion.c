#include <assert.h>
#include <stdio.h>

#include "../src/ast.h"
#include "../src/compiler.h"
#include "../src/parser.h"
#include "../src/typecheck.h"
#include "../src/vm.h"

/**
 * Compiles `source`, optionally type checking it first. Returns whether
 * compiling succeeded.
 */
static bool compiles(const char *source, bool typecheck) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadError, &endLine);
  assert(!hadError);

  if (typecheck) {
    typchkSessionBegin();
    bool ok = typchkCheckProgram(ast, count);
    typchkSessionEnd();
    assert(ok);
  }

  CompiledUnit *unit = compile(ast, count, endLine);
  bool ok = unit != NULL;

  if (unit != NULL) {
    freeCompiledUnit(unit);
    free(unit);
  }

  astFreeAll();
  free(ast);

  return ok;
}

/**
 * The type checker picks each placeholder's conversion. Skipping it, as a
 * type checker bug that misses a node would, leaves every part unset. The
 * compiler must refuse to compile an unset part rather than assume it is
 * already a string.
 */
static void assertUnsetConversionRejected(const char *source) {
  initVM(0, NULL);
  assert(compiles(source, true));
  compilerSessionEnd();
  freeVM();

  initVM(0, NULL);
  assert(!compiles(source, false));
  compilerSessionEnd();
  freeVM();
}

static void test_unset_conversion_rejected(void) {
  assertUnsetConversionRejected("let n = 1;\n"
                                "let s = $\"n={n}\";");
}

/**
 * A single-part interpolated string compiles without the join, a separate
 * branch in the compiler.
 */
static void test_unset_conversion_rejected_single_part(void) {
  assertUnsetConversionRejected("let n = 1;\n"
                                "let s = $\"{n}\";");
}

/**
 * A part that is already a string still needs its conversion chosen.
 */
static void test_unset_conversion_rejected_string_part(void) {
  assertUnsetConversionRejected("let s = $\"a{\"b\"}\";");
}

int main(void) {
  test_unset_conversion_rejected();
  test_unset_conversion_rejected_single_part();
  test_unset_conversion_rejected_string_part();

  printf("compiler_string_conversion: ok\n");
  return 0;
}
