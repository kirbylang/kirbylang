#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/ast.h"
#include "../src/compiler.h"
#include "../src/parser.h"
#include "../src/stdlib_source.h"
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
  typchkResetError();

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

// Starts a session that has the real standard library loaded
static void beginWithStdlib(void);

// Like `format`, with up to four values as `%1$s` to `%4$s`
static char *formatMany(const char *pattern, const char *first,
                        const char *second, const char *third,
                        const char *fourth) {
  char *out = (char *)malloc(1024);
  assert(out != NULL);
  snprintf(out, 1024, pattern, first, second, third, fourth);
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

static void beginWithStdlib(void) {
  begin();
  assert(run(KIRBY_STDLIB, true) == INTERPRET_OK);
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

static const char *tagTrait = "trait Tag {\n"
                              "  fun tag(self): string;\n"
                              "  fun make(): Self;\n"
                              "}";

// Runs the source and returns what it printed to stderr, which the caller frees
static char *errorsOf(const char *source, bool isStdlib) {
  fflush(stderr);
  int saved = dup(STDERR_FILENO);
  FILE *capture = tmpfile();
  assert(saved >= 0 && capture != NULL);
  dup2(fileno(capture), STDERR_FILENO);

  run(source, isStdlib);

  fflush(stderr);
  dup2(saved, STDERR_FILENO);
  close(saved);

  long size = ftell(capture);
  char *text = (char *)calloc((size_t)size + 1, 1);
  rewind(capture);
  size_t got = fread(text, 1, (size_t)size, capture);
  assert((long)got == size);
  fclose(capture);
  return text;
}

static void expectOk(const char *source, bool isStdlib) {
  assert(run(source, isStdlib) == INTERPRET_OK);
}

static void expectError(const char *source, bool isStdlib) {
  assert(run(source, isStdlib) == INTERPRET_COMPILE_ERROR);
}

static void test_every_kind_can_implement_a_trait(void) {
  begin();
  expectOk(tagTrait, true);

  for (int i = 0; i < kindCount; i++) {
    char *impl = format("impl Tag for %1$s {\n"
                        "  fun tag(self): string = \"%1$s\";\n"
                        "  fun make(): Self = %2$s;\n"
                        "}",
                        kinds[i].type, kinds[i].value);
    expectOk(impl, true);
    free(impl);
  }

  for (int i = 0; i < kindCount; i++) {
    char *program = format("if (true) {\n"
                           "let v: %1$s = %2$s;\n"
                           "@assert(v.tag() == \"%1$s\", \"tag\");\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    expectOk(program, false);
    free(program);
  }

  end();
}

static void test_a_trait_impl_must_match_its_trait(void) {
  begin();
  expectOk(tagTrait, true);

  // A trait method is missing
  expectError("impl Tag for u8 { fun tag(self): string = \"u8\"; }", true);

  // A method the trait doesn't have
  expectError("impl Tag for u8 {\n"
              "  fun tag(self): string = \"u8\";\n"
              "  fun make(): Self = 1;\n"
              "  fun extra(self): u8 = self;\n"
              "}",
              true);

  // The wrong signature
  expectError("impl Tag for u8 {\n"
              "  fun tag(self): u8 = self;\n"
              "  fun make(): Self = 1;\n"
              "}",
              true);

  // An instance method written as a static one
  expectError("impl Tag for u8 {\n"
              "  fun tag(): string = \"u8\";\n"
              "  fun make(): Self = 1;\n"
              "}",
              true);

  // Nothing above registered the trait
  expectOk("impl Tag for u8 {\n"
           "  fun tag(self): string = \"u8\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);

  end();
}

static void test_a_kind_implements_a_trait_once(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl Tag for string {\n"
           "  fun tag(self): string = \"a\";\n"
           "  fun make(): Self = \"\";\n"
           "}",
           true);
  expectError("impl Tag for string {\n"
              "  fun tag(self): string = \"b\";\n"
              "  fun make(): Self = \"\";\n"
              "}",
              true);
  end();
}

static void test_a_supertrait_needs_its_own_impl(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("trait Pair: Tag { fun pair(self): string; }", true);

  expectError("impl Pair for u8 { fun pair(self): string = \"u8\"; }", true);

  // The order inside a unit doesn't matter
  expectOk("impl Pair for i8 { fun pair(self): string = \"i8\"; }\n"
           "impl Tag for i8 {\n"
           "  fun tag(self): string = \"i8\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);

  end();
}

static void test_an_inherent_method_shadows_a_trait_method(void) {
  begin();
  expectOk(tagTrait, true);

  expectOk("impl u8 { pub fun tag(self): string = \"inherent\"; }\n"
           "impl Tag for u8 {\n"
           "  fun tag(self): string = \"trait\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);
  expectOk("impl Tag for u16 {\n"
           "  fun tag(self): string = \"trait\";\n"
           "  fun make(): Self = 1;\n"
           "}\n"
           "impl u16 { pub fun tag(self): string = \"inherent\"; }",
           true);

  expectOk("if (true) {\n"
           "let a: u8 = 1;\n"
           "let b: u16 = 1;\n"
           "@assert(a.tag() == \"inherent\", \"u8\");\n"
           "@assert(b.tag() == \"inherent\", \"u16\");\n"
           "}",
           false);

  end();
}

static void test_method_names_can_only_clash_by_shadowing(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("trait Other { fun tag(self): string; }", true);
  expectOk("trait Maker { fun tag(): Self; }", true);

  expectOk("impl Tag for u8 {\n"
           "  fun tag(self): string = \"u8\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);

  // Two traits with the same method name
  expectError("impl Other for u8 { fun tag(self): string = \"other\"; }", true);

  // An instance method and a static method with one name
  expectError("impl Maker for u8 { fun tag(): Self = 1; }", true);
  expectError("impl u8 { pub fun make(self): u8 = self; }", true);

  end();
}

static void test_a_trait_method_must_be_called(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl Tag for u8 {\n"
           "  fun tag(self): string = \"u8\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);

  expectError("let v: u8 = 1;\nlet f = v.tag;", false);

  end();
}

static void test_a_trait_method_belongs_to_its_kind(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl Tag for u8 {\n"
           "  fun tag(self): string = \"u8\";\n"
           "  fun make(): Self = 1;\n"
           "}",
           true);

  expectError("let v: i64 = 1;\n@println(v.tag());", false);

  end();
}

static void test_only_the_stdlib_implements_traits_for_primitives(void) {
  begin();
  expectOk(tagTrait, true);

  for (int i = 0; i < kindCount; i++) {
    char *program = format("impl Tag for %1$s {\n"
                           "  fun tag(self): string = \"x\";\n"
                           "  fun make(): Self = %2$s;\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    expectError(program, false);
    free(program);
  }

  expectError("impl Tag for unit {\n"
              "  fun tag(self): string = \"x\";\n"
              "  fun make(): Self = ();\n"
              "}",
              true);

  end();
}

static void test_a_primitive_can_not_fill_a_bound_yet(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl Tag for string {\n"
           "  fun tag(self): string = \"string\";\n"
           "  fun make(): Self = \"\";\n"
           "}",
           true);

  char *errors = errorsOf("fun show[T: Tag](x: T): string = x.tag();\n"
                          "@println(show(\"a\"));",
                          false);
  assert(strstr(errors, "string implements Tag, but a primitive can't be used "
                        "with a bound yet, required by T.") != NULL);
  free(errors);

  // A string without the impl keeps the usual message
  errors = errorsOf("trait Other { fun other(self): string; }\n"
                    "fun show[T: Other](x: T): string = x.other();\n"
                    "@println(show(\"a\"));",
                    false);
  assert(strstr(errors, "string doesn't implement Other, required by T.") !=
         NULL);
  free(errors);

  end();
}

static void test_self_calls_a_static_method_of_the_primitive(void) {
  begin();
  expectOk("trait Pair {\n"
           "  fun one(): Self;\n"
           "  fun two(): Self;\n"
           "}",
           true);

  for (int i = 0; i < kindCount; i++) {
    char *impl = format("impl Pair for %1$s {\n"
                        "  fun one(): Self = %2$s;\n"
                        "  fun two(): Self = Self.one();\n"
                        "}",
                        kinds[i].type, kinds[i].value);
    expectOk(impl, true);
    free(impl);

    char *program = format("if (true) {\n"
                           "let a: %1$s = %2$s;\n"
                           "@assert(%1$s.two() == a, \"two\");\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    expectOk(program, false);
    free(program);
  }

  // A user file can't use `Self` this way, and an unknown name is an error
  expectError("fun main(): unit { @println(Self.one()); }", false);
  expectError("fun main(): unit { @println(u8.three()); }", false);

  end();
}

// A run time error ends the process, so the source runs in a child process
// that has the real standard library loaded. Returns the first line it wrote to
// stderr, which the caller frees.
static char *firstErrorLine(const char *source) {
  fflush(stderr);
  FILE *capture = tmpfile();
  assert(capture != NULL);

  pid_t child = fork();
  assert(child >= 0);

  if (child == 0) {
    dup2(fileno(capture), STDERR_FILENO);
    beginWithStdlib();
    run(source, false);
    fflush(stderr);
    _exit(0);
  }

  int status = 0;
  waitpid(child, &status, 0);

  fseek(capture, 0, SEEK_END);
  long size = ftell(capture);
  char *text = (char *)calloc((size_t)size + 1, 1);
  rewind(capture);
  size_t got = fread(text, 1, (size_t)size, capture);
  assert((long)got == size);
  fclose(capture);

  char *end = strchr(text, '\n');
  if (end != NULL)
    *end = '\0';
  return text;
}

typedef struct {
  const char *type;
  const char *min;
  const char *max;
  bool isSigned;
} IntegerKind;

static const IntegerKind integerKinds[] = {
    {"i8", "-128", "127", true},
    {"i16", "-32768", "32767", true},
    {"i32", "-2147483648", "2147483647", true},
    {"i64", "-9223372036854775807 - 1", "9223372036854775807", true},
    {"u8", "0", "255", false},
    {"u16", "0", "65535", false},
    {"u32", "0", "4294967295", false},
    {"u64", "0", "18446744073709551615", false},
};

// A method and its operator must fail with the same message
static void expectSameError(const char *type, const char *left,
                            const char *right, const char *method,
                            const char *op) {
  char *viaMethod = formatMany("if (true) {\n"
                               "let a: %1$s = %2$s;\n"
                               "let b: %1$s = %3$s;\n"
                               "@println(a.%4$s(b));\n"
                               "}",
                               type, left, right, method);
  char *viaOperator = formatMany("if (true) {\n"
                                 "let a: %1$s = %2$s;\n"
                                 "let b: %1$s = %3$s;\n"
                                 "@println(a %4$s b);\n"
                                 "}",
                                 type, left, right, op);

  char *fromMethod = firstErrorLine(viaMethod);
  char *fromOperator = firstErrorLine(viaOperator);

  assert(fromMethod[0] != '\0');
  assert(strcmp(fromMethod, fromOperator) == 0);

  free(viaMethod);
  free(viaOperator);
  free(fromMethod);
  free(fromOperator);
}

static void test_methods_fail_like_their_operators(void) {
  for (size_t i = 0; i < sizeof(integerKinds) / sizeof(integerKinds[0]); i++) {
    const IntegerKind *kind = &integerKinds[i];

    expectSameError(kind->type, kind->max, "1", "add", "+");
    expectSameError(kind->type, kind->min, "1", "sub", "-");
    expectSameError(kind->type, kind->max, "2", "mul", "*");
    expectSameError(kind->type, "1", "0", "div", "/");
    expectSameError(kind->type, "1", "0", "mod", "%");

    if (kind->isSigned) {
      char *viaMethod = format("if (true) {\n"
                               "let a: %1$s = %2$s;\n"
                               "@println(a.neg());\n"
                               "}",
                               kind->type, kind->min);
      char *viaOperator = format("if (true) {\n"
                                 "let a: %1$s = %2$s;\n"
                                 "@println(-a);\n"
                                 "}",
                                 kind->type, kind->min);
      char *fromMethod = firstErrorLine(viaMethod);
      char *fromOperator = firstErrorLine(viaOperator);

      assert(fromMethod[0] != '\0');
      assert(strcmp(fromMethod, fromOperator) == 0);

      free(viaMethod);
      free(viaOperator);
      free(fromMethod);
      free(fromOperator);
    }
  }
}

int main(void) {
  test_every_kind_can_have_methods();
  test_a_method_belongs_to_one_kind();
  test_only_the_stdlib_adds_methods();
  test_unit_has_no_methods();
  test_every_kind_can_implement_a_trait();
  test_methods_fail_like_their_operators();
  test_self_calls_a_static_method_of_the_primitive();
  test_a_trait_impl_must_match_its_trait();
  test_a_kind_implements_a_trait_once();
  test_a_supertrait_needs_its_own_impl();
  test_an_inherent_method_shadows_a_trait_method();
  test_method_names_can_only_clash_by_shadowing();
  test_a_trait_method_must_be_called();
  test_a_trait_method_belongs_to_its_kind();
  test_only_the_stdlib_implements_traits_for_primitives();
  test_a_primitive_can_not_fill_a_bound_yet();

  printf("primitive method tests passed\n");
  return 0;
}
