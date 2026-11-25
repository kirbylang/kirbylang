#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/compiler.h"
#include "../src/parser.h"
#include "../src/typecheck.h"
#include "../src/vm.h"

typedef struct {
  const char *type;
  const char *value;
} Kind;

static const Kind kinds[] = {
    {"bool", "true"}, {"string", "\"hi\""}, {"i64", "7"}, {"i32", "7"},
    {"i16", "7"},     {"i8", "7"},          {"u64", "7"}, {"u32", "7"},
    {"u16", "7"},     {"u8", "7"},          {"f64", "7"}, {"f32", "7"},
};

static const int kindCount = (int)(sizeof(kinds) / sizeof(kinds[0]));

// Parses, checks, compiles and runs one unit the way the command line does.
// The standard library's checks are the ones that allow primitive impls.
static InterpretResult run(const char *source, bool isStdlib) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadError, &endLine);
  assert(!hadError);

  bool ok =
      isStdlib ? typchkCheckStdlib(ast, count) : typchkCheckProgram(ast, count);
  CompiledUnit *unit = ok ? compile(ast, count, endLine) : NULL;

  astFreeAll();
  free(ast);

  if (unit == NULL)
    return INTERPRET_COMPILE_ERROR;

  return interpret(unit, false);
}

// `%1$s` in the pattern is the type and `%2$s` is a value of it
static char *format(const char *pattern, const char *type, const char *value) {
  char *out = (char *)malloc(1024);
  assert(out != NULL);
  snprintf(out, 1024, pattern, type, value);
  return out;
}

static void begin(void) {
  initVM(0, NULL);
  typchkResetError();
  typchkSessionBegin();
}

static void end(void) {
  typchkSessionEnd();
  typchkResetError();
  compilerSessionEnd();
  freeVM();
}

static void test_every_kind_can_have_methods(void) {
  begin();

  for (int i = 0; i < kindCount; i++) {
    char *stdlib =
        format("impl %1$s {\n"
               "  pub fun same(self, other: Self): bool = self == other;\n"
               "  pub fun first(self, other: %1$s): Self = self;\n"
               "  pub fun me(self): %1$s {\n"
               "    let copy: Self = self;\n"
               "    copy\n"
               "  }\n"
               "}",
               kinds[i].type, kinds[i].value);
    assert(run(stdlib, true) == INTERPRET_OK);
    free(stdlib);
  }

  for (int i = 0; i < kindCount; i++) {
    char *program = format("if (true) {\n"
                           "let v: %1$s = %2$s;\n"
                           "@assert(v.same(v), \"same\");\n"
                           "@assert(v.me().same(v), \"me\");\n"
                           "@assert((v.first(v)).same(v), \"first\");\n"
                           "@assert(v.first(v).me().same(v), \"chained\");\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    assert(run(program, false) == INTERPRET_OK);
    free(program);
  }

  end();
}

static void test_a_method_belongs_to_one_kind(void) {
  begin();

  assert(run("impl u8 { pub fun twice(self): u8 = self * 2; }", true) ==
         INTERPRET_OK);

  assert(run("let a: u8 = 4;\n@assert(a.twice() == 8, \"u8\");", false) ==
         INTERPRET_OK);

  typchkResetError();
  assert(run("let a: i64 = 4;\n@println(a.twice());", false) ==
         INTERPRET_COMPILE_ERROR);

  typchkResetError();
  assert(run("let a: bool = true;\n@println(a.twice());", false) ==
         INTERPRET_COMPILE_ERROR);

  end();
}

static void test_only_the_stdlib_adds_methods(void) {
  begin();

  for (int i = 0; i < kindCount; i++) {
    char *program = format("impl %1$s { pub fun nope(self): %1$s = self; }",
                           kinds[i].type, kinds[i].value);
    typchkResetError();
    assert(run(program, false) == INTERPRET_COMPILE_ERROR);
    free(program);
  }

  end();
}

static void test_unit_has_no_methods(void) {
  begin();

  typchkResetError();
  assert(run("impl unit { pub fun nope(self): unit {} }", true) ==
         INTERPRET_COMPILE_ERROR);

  end();
}

int main(void) {
  test_every_kind_can_have_methods();
  test_a_method_belongs_to_one_kind();
  test_only_the_stdlib_adds_methods();
  test_unit_has_no_methods();

  printf("primitive method tests passed\n");
  return 0;
}
