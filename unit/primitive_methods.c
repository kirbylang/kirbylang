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
    {"unit", "()"},
};

static const int kindCount = (int)(sizeof(kinds) / sizeof(kinds[0]));

// Parses, checks, compiles and runs one unit the way the command line does.
// The standard library's parser and checks are the ones that allow primitive
// impls. A unit that doesn't parse is a compile error.
static InterpretResult run(const char *source, bool isStdlib) {
  typchkResetError();

  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = isStdlib ? parseStdlib(source, &count, &hadError, &endLine)
                           : parse(source, &count, &hadError, &endLine);

  if (hadError) {
    astFreeAll();
    free(ast);
    return INTERPRET_COMPILE_ERROR;
  }

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

static const char *labelTrait = "trait Label { fun label(self): string; }";

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
           "@assert(Tag.tag(a) == \"trait\", \"u8 through the trait\");\n"
           "@assert(Tag.tag(b) == \"trait\", \"u16 through the trait\");\n"
           "}",
           false);

  end();
}

static void test_a_trait_names_its_method_on_every_kind(void) {
  begin();
  expectOk(tagTrait, true);

  for (int i = 0; i < kindCount; i++) {
    char *impl = format("impl %1$s { pub fun tag(self): string = \"own\"; }\n"
                        "impl Tag for %1$s {\n"
                        "  fun tag(self): string = \"%1$s\";\n"
                        "  fun make(): Self = %2$s;\n"
                        "}",
                        kinds[i].type, kinds[i].value);
    expectOk(impl, true);
    free(impl);

    char *program = format("if (true) {\n"
                           "let v: %1$s = %2$s;\n"
                           "@assert(v.tag() == \"own\", \"own\");\n"
                           "@assert(Tag.tag(v) == \"%1$s\", \"trait\");\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    expectOk(program, false);
    free(program);
  }

  // A trait the kind doesn't implement, and a static method
  expectOk("trait Other { fun other(self): string; }", true);
  char *errors =
      errorsOf("if (true) { let v: u8 = 1; Other.other(v); }", false);
  assert(strstr(errors, "u8 doesn't implement Other.") != NULL);
  free(errors);

  errors = errorsOf("if (true) { let v: u8 = 1; Tag.make(v); }", false);
  assert(strstr(errors, "'make' is a static method of 'Tag'.") != NULL);
  free(errors);

  end();
}

static void test_two_traits_can_share_a_method_name(void) {
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
  expectOk("impl Other for u8 { fun tag(self): string = \"other\"; }", true);

  // The call has to say which trait it means
  char *errors = errorsOf("let v: u8 = 1;\nv.tag();", false);
  assert(strstr(errors, "Method 'tag' is in both Tag and Other. Call it as "
                        "'Tag.tag(value)'.") != NULL);
  free(errors);

  expectOk("if (true) {\n"
           "let v: u8 = 1;\n"
           "@assert(Tag.tag(v) == \"u8\", \"Tag\");\n"
           "@assert(Other.tag(v) == \"other\", \"Other\");\n"
           "}",
           false);

  // One impl can't declare the name twice
  expectError("impl Other for u16 {\n"
              "  fun tag(self): string = \"a\";\n"
              "  fun tag(self): string = \"b\";\n"
              "}",
              true);

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

static void test_user_code_implements_its_own_traits_for_primitives(void) {
  begin();
  expectOk(tagTrait, false);

  for (int i = 0; i < kindCount; i++) {
    char *program = formatMany("impl Tag for %1$s {\n"
                               "  fun tag(self): string = \"%1$s\";\n"
                               "  fun make(): Self = %2$s;\n"
                               "}\n"
                               "if (true) {\n"
                               "  let v: %1$s = %1$s.make();\n"
                               "  @assert(v.tag() == \"%1$s\", \"tag\");\n"
                               "}",
                               kinds[i].type, kinds[i].value, "", "");
    expectOk(program, false);
    free(program);
  }

  end();
}

static void test_only_the_stdlib_implements_builtin_traits(void) {
  beginWithStdlib();

  for (int i = 0; i < kindCount; i++) {
    char *program = format("impl Display for %1$s {\n"
                           "  fun toString(self): string = \"x\";\n"
                           "}",
                           kinds[i].type, kinds[i].value);
    char *errors = errorsOf(program, false);
    assert(strstr(errors,
                  "Only the standard library can implement 'Display'") != NULL);
    free(errors);
    free(program);
  }

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

static void test_lists_can_have_methods_and_traits(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl[T] [T] {\n"
           "  pub fun first(self): T = self[0];\n"
           "  pub fun other(self, other: Self): Self = other;\n"
           "  pub fun pick[U](self, extra: U): U = extra;\n"
           "  pub fun both[U](self, extra: U): [U] = [extra, extra];\n"
           "  fun hidden(self): T = self[0];\n"
           "  pub fun shown(self): T = self.hidden();\n"
           "}",
           true);
  expectOk("impl[T] Tag for [T] {\n"
           "  fun tag(self): string = \"list\";\n"
           "  fun make(): Self = [];\n"
           "}",
           true);

  expectOk("if (true) {\n"
           "let xs: [i64] = [4, 5];\n"
           "let names: [string] = [\"a\"];\n"
           "@assert(xs.first() == 4, \"first\");\n"
           "@assert(names.first() == \"a\", \"first of strings\");\n"
           "@assert(xs.other(xs).first() == 4, \"self type\");\n"
           "@assert(xs.pick(\"s\") == \"s\", \"own parameter\");\n"
           "@assert(names.pick(7) == 7, \"own parameter again\");\n"
           "@assert(xs.both(true)[1], \"own parameter in a list\");\n"
           "@assert(xs.shown() == 4, \"private method\");\n"
           "@assert(xs.tag() == \"list\", \"trait method\");\n"
           "@assert(Tag.tag(xs) == \"list\", \"trait method by name\");\n"
           "}",
           false);

  // The item type has to match
  expectError("let xs: [i64] = [1];\n"
              "let names: [string] = [\"a\"];\n"
              "xs.other(names);",
              false);
  expectError("let xs: [i64] = [1];\n"
              "let s: string = xs.first();",
              false);

  // A private method is private to the impl, and a method is called
  expectError("let xs: [i64] = [1];\nxs.hidden();", false);
  expectError("let xs: [i64] = [1];\nlet f = xs.first;", false);
  expectError("let xs: [i64] = [1];\nxs.missing();", false);

  end();
}

static void test_a_list_impl_is_checked(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("impl[T] [T] { pub fun first(self): T = self[0]; }", true);

  // The item type is a parameter, not a type
  char *errors =
      errorsOf("impl[T] [string] { pub fun a(self): i64 = 1; }", true);
  assert(strstr(errors, "'string' is a type. An impl names the item type with "
                        "a type parameter, as in 'impl[T] [T]'.") != NULL);
  free(errors);

  // A name is declared once, and a body only does what works for any T
  errors = errorsOf("impl[T] [T] { pub fun first(self): T = self[0]; }", true);
  assert(strstr(errors, "'first' is already declared on 'list'.") != NULL);
  free(errors);

  errors = errorsOf("impl[T] [T] { pub fun bad(self): i64 = self[0]; }", true);
  assert(strstr(errors, "Expected i64, got type parameter T.") != NULL);
  free(errors);

  errors = errorsOf("impl[T] [T] { pub fun make(): i64 = 1; }", true);
  assert(strstr(errors, "Static methods on 'list' aren't supported yet.") !=
         NULL);
  free(errors);

  // A trait is implemented once, and has to be complete
  expectOk("impl[T] Tag for [T] {\n"
           "  fun tag(self): string = \"a\";\n"
           "  fun make(): Self = [];\n"
           "}",
           true);
  errors = errorsOf("impl[T] Tag for [T] {\n"
                    "  fun tag(self): string = \"b\";\n"
                    "  fun make(): Self = [];\n"
                    "}",
                    true);
  assert(strstr(errors, "'[T]' already implements 'Tag'.") != NULL);
  free(errors);

  errors = errorsOf("trait Half { fun a(self): i64; fun b(self): i64; }\n"
                    "impl[T] Half for [T] { fun a(self): i64 = 1; }",
                    true);
  assert(strstr(errors, "'b'") != NULL);
  free(errors);

  // Only the standard library writes them
  expectError("impl[T] [T] { pub fun other(self): T = self[0]; }", false);
  expectError("impl[T] Tag for [T] { fun tag(self): string = \"\";"
              " fun make(): Self = []; }",
              false);

  // A list fills a bounded parameter no better than a string does
  errors = errorsOf("fun show[T: Tag](x: T): string = x.tag();\n"
                    "let xs: [i64] = [1];\n"
                    "@println(show(xs));",
                    false);
  assert(strstr(errors, "[i64] implements Tag, but a primitive can't be used "
                        "with a bound yet, required by T.") != NULL);
  free(errors);

  end();
}

static void test_a_list_impl_needs_its_trait(void) {
  begin();
  expectOk(tagTrait, true);
  expectOk("trait Pair: Tag { fun pair(self): string; }", true);

  char *errors =
      errorsOf("impl[T] Pair for [T] { fun pair(self): string = \"\"; }", true);
  assert(strstr(errors, "also needs 'impl Tag for [T]'") != NULL);
  free(errors);

  end();
}

static void test_arrays_can_have_methods_and_traits(void) {
  begin();
  expectOk(labelTrait, true);
  expectOk("impl[T, const N] [T; N] {\n"
           "  pub fun first(self): T = self[0];\n"
           "  pub fun same(self): [T; N] = self;\n"
           "  pub fun pick[U](self, extra: U): U = extra;\n"
           "  pub fun twin[U](self, other: [U; N]): [U; N] = other;\n"
           "  pub fun join[U, const M](self, other: [U; M]): [T] = "
           "@arrToList(self);\n"
           "}",
           true);
  expectOk("impl[T, const N] Label for [T; N] {\n"
           "  fun label(self): string = \"array\";\n"
           "}",
           true);

  // A list and an array can each have a method of the same name
  expectOk(
      "impl[T] [T] { pub fun first(self): T = self[0]; pub fun kind(self): "
      "string = \"list\"; }",
      true);

  expectOk("if (true) {\n"
           "let xs = [4, 5];\n"
           "let names = [\"a\", \"b\", \"c\"];\n"
           "let list: [i64] = [6];\n"
           "@assert(xs.first() == 4, \"first\");\n"
           "@assert(names.first() == \"a\", \"first of strings\");\n"
           "@assert(list.first() == 6, \"first of a list\");\n"
           "@assert(list.kind() == \"list\", \"list method\");\n"
           "let same: [i64; 2] = xs.same();\n"
           "let again: [string; 3] = names.same();\n"
           "@assert(xs.pick(\"s\") == \"s\", \"own parameter\");\n"
           "let twin: [string; 2] = xs.twin([\"s\", \"t\"]);\n"
           "@assert(twin[1] == \"t\", \"own parameter and length\");\n"
           "@assert(xs.join(names).first() == 4, \"other length\");\n"
           "@assert(xs.label() == \"array\", \"trait method\");\n"
           "@assert(Label.label(xs) == \"array\", \"trait method by name\");\n"
           "}",
           false);

  // The length is kept, and the other kind doesn't get the method
  expectError("let xs = [1, 2];\nlet wrong: [i64; 3] = xs.same();", false);
  expectError(
      "let xs = [1, 2];\nlet wrong: [string; 3] = xs.twin([\"s\", \"t\"]);",
      false);
  expectError("let xs = [1, 2];\nxs.twin([\"s\", \"t\", \"u\"]);", false);
  expectError("let xs = [1, 2];\nxs.kind();", false);
  expectError("let xs: [i64] = [1];\nxs.same();", false);

  end();
}

static void test_an_array_impl_is_checked(void) {
  begin();
  expectOk(labelTrait, true);

  // `_` is a length that is not used
  expectOk("impl[T] [T; _] { pub fun last(self): T = self[0]; }", true);

  char *errors = errorsOf("impl[T] [T; 3] { pub fun a(self): i64 = 1; }", true);
  assert(strstr(errors, "An array impl can't be for one length yet. Declare "
                        "it, as in 'impl[T, const N] [T; N]'.") != NULL);
  free(errors);

  errors =
      errorsOf("impl[const N] [string; N] { pub fun a(self): i64 = 1; }", true);
  assert(strstr(errors, "'string' is a type.") != NULL);
  free(errors);

  expectOk("impl[T, const N] [T; N] { pub fun first(self): T = self[0]; }",
           true);
  errors = errorsOf(
      "impl[T, const N] [T; N] { pub fun first(self): T = self[0]; }", true);
  assert(strstr(errors, "'first' is already declared on 'array'.") != NULL);
  free(errors);

  errors = errorsOf(
      "impl[T, const N] [T; N] { pub fun bad(self): i64 = self[0]; }", true);
  assert(strstr(errors, "Expected i64, got type parameter T.") != NULL);
  free(errors);

  // The same rules as a list for traits
  expectOk("impl[T, const N] Label for [T; N] {\n"
           "  fun label(self): string = \"a\";\n"
           "}",
           true);
  errors = errorsOf("impl[T, const N] Label for [T; N] {\n"
                    "  fun label(self): string = \"b\";\n"
                    "}",
                    true);
  assert(strstr(errors, "'[T; N]' already implements 'Label'.") != NULL);
  free(errors);

  expectError("impl[T, const N] [T; N] { pub fun other(self): T = self[0]; }",
              false);

  errors = errorsOf("fun show[T: Label](x: T): string = x.label();\n"
                    "@println(show([1, 2]));",
                    false);
  assert(strstr(errors, "[i64; 2] implements Label, but a primitive can't be "
                        "used with a bound yet, required by T.") != NULL);
  free(errors);

  end();
}

// Checks one standard library unit and expects `message` among its errors
static void expectStdlibMessage(const char *source, const char *message) {
  begin();
  char *errors = errorsOf(source, true);

  if (strstr(errors, message) == NULL)
    fprintf(stderr, "Wanted '%s' for:\n%s\nin:\n%s\n", message, source, errors);

  assert(strstr(errors, message) != NULL);
  free(errors);
  end();
}

static void test_an_impl_parameter_hides_a_type_name(void) {
  begin();
  expectOk("impl[string] [string] { pub fun first(self): string = self[0]; }",
           true);
  expectOk("if (true) { let xs: [i64] = [1, 2]; let n: i64 = xs.first(); }",
           false);
  end();
}

static void test_an_impl_declares_what_its_target_names(void) {
  expectStdlibMessage("impl[T] [U] { }",
                      "'U' isn't declared. Declare it after 'impl', as in "
                      "'impl[T] [T]'.");
  expectStdlibMessage("impl[T] [T; N] { }",
                      "'N' isn't declared. Declare it after 'impl', as in "
                      "'impl[T, const N] [T; N]'.");
  expectStdlibMessage("impl[T, N] [T; N] { }",
                      "'N' is a type parameter, not a length. Declare it as "
                      "'const N'.");
  expectStdlibMessage("impl[T, const N] [N; N] { }",
                      "'N' is a length, not a type.");
  expectStdlibMessage("impl[T, U] [T] { }",
                      "'U' isn't used by the type the impl is for.");
  expectStdlibMessage("impl[T, const N] [T] { }",
                      "'N' isn't used by the type the impl is for.");
  expectStdlibMessage("impl[T, T] [T] { }",
                      "Generic parameter 'T' is already declared.");
}

static void test_a_length_has_to_be_declared(void) {
  expectStdlibMessage("fun f[T](a: [T; M]): unit {}",
                      "Unknown length 'M'. Declare it as 'const M'.");
  expectStdlibMessage("fun f[T](a: [T; T]): unit {}",
                      "'T' is a type, not a length.");
  expectStdlibMessage("fun f[const M](a: M): unit {}",
                      "'M' is a length, not a type.");
  expectStdlibMessage("impl[T] [T; _] { pub fun a(self): [T; _] = self; }",
                      "Unknown length '_'.");
}

static void test_a_method_can_not_reuse_an_impls_name(void) {
  expectStdlibMessage("impl[T] [T] { pub fun f[T](self, x: T): T = x; }",
                      "Generic parameter 'T' is already declared.");
  expectStdlibMessage("impl[T, const N] [T; N] {\n"
                      "  pub fun f[const N](self, x: [T; N]): unit {}\n"
                      "}",
                      "Generic parameter 'N' is already declared.");
  expectStdlibMessage("struct Box[T] { pub value: T; }\n"
                      "impl Box[T] { pub fun f[T](self, x: T): T = x; }",
                      "Generic parameter 'T' is already declared.");
}

static void test_two_lengths_are_different(void) {
  const char *message = "Expected [T; N], got [T; M].";

  expectStdlibMessage("impl[T, const N] [T; N] {\n"
                      "  pub fun f[const M](self, o: [T; M]): [T; N] = o;\n"
                      "}",
                      message);
  expectStdlibMessage("fun f[T, const N, const M](a: [T; N], b: [T; M]): "
                      "[T; N] = b;",
                      message);
  expectStdlibMessage("struct Wrap[T] { pub value: T; }\n"
                      "fun f[T, const N, const M](b: Wrap[[T; M]]): "
                      "Wrap[[T; N]] = b;",
                      "Expected Wrap[[T; N]], got Wrap[[T; M]].");
  expectStdlibMessage("fun same[T, const N](a: [T; N], b: [T; N]): bool = "
                      "true;\n"
                      "fun f[T, const N, const M](a: [T; N], b: [T; M]): "
                      "bool = same(a, b);",
                      message);
  expectStdlibMessage("fun f[const N](a: [i64; N]): [i64; 2] = a;",
                      "Expected [i64; 2], got [i64; N].");
  expectStdlibMessage("fun f[const N](a: [i64; N]): [i64; N] = [1, 2];",
                      "Expected [i64; N], got an array literal with 2 items.");
}

static void test_const_parameters_work_with_type_parameters(void) {
  begin();
  expectOk("struct Wrap[T] { pub value: T; }\n"
           "fun wrap[T, const N](a: [T; N]): Wrap[[T; N]] = "
           "Wrap { value: a };\n"
           "fun pass[const N, T](a: [T; N]): [T; N] = wrap(a).value;\n"
           "impl[const N, T] [T; N] {\n"
           "  pub fun four[A, B, C, D](self, a: A, b: B, c: C, d: D): "
           "[T; N] = self;\n"
           "  pub fun at(self): T = self[5];\n"
           "  pub fun joined[const M](self, other: [T; M]): [T] = "
           "@arrConcat(self, other);\n"
           "  pub fun again(self): [T; N] = pass(self.four(1, \"a\", true, "
           "()));\n"
           "}",
           true);
  expectOk("if (true) {\n"
           "let xs = [4, 5];\n"
           "let four: [i64; 2] = xs.four(1, \"a\", true, ());\n"
           "let again: [i64; 2] = xs.again();\n"
           "let wrapped: Wrap[[i64; 2]] = wrap(xs);\n"
           "@assert(xs.joined([6, 7, 8])[4] == 8, \"two lengths\");\n"
           "}",
           false);
  expectError("let xs = [4, 5];\nlet w: Wrap[[i64; 3]] = wrap(xs);", false);
  end();

  expectStdlibMessage("fun f[const N](): [i64; N] = f();\n"
                      "fun g(): unit { let x = f(); }",
                      "Can't tell what 'N' is from the arguments.");
}

static void test_an_array_impl_needs_its_trait(void) {
  begin();
  expectOk(labelTrait, true);
  expectOk("trait Pair: Label { fun pair(self): string; }", true);

  char *errors = errorsOf(
      "impl[T] Pair for [T; _] { fun pair(self): string = \"\"; }", true);
  assert(strstr(errors, "also needs 'impl Label for [T; _]'") != NULL);
  free(errors);

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
  test_every_kind_can_implement_a_trait();
  test_methods_fail_like_their_operators();
  test_self_calls_a_static_method_of_the_primitive();
  test_a_trait_impl_must_match_its_trait();
  test_a_kind_implements_a_trait_once();
  test_a_supertrait_needs_its_own_impl();
  test_an_inherent_method_shadows_a_trait_method();
  test_a_trait_names_its_method_on_every_kind();
  test_two_traits_can_share_a_method_name();
  test_a_trait_method_must_be_called();
  test_a_trait_method_belongs_to_its_kind();
  test_user_code_implements_its_own_traits_for_primitives();
  test_only_the_stdlib_implements_builtin_traits();
  test_a_primitive_can_not_fill_a_bound_yet();
  test_lists_can_have_methods_and_traits();
  test_a_list_impl_is_checked();
  test_a_list_impl_needs_its_trait();
  test_arrays_can_have_methods_and_traits();
  test_an_array_impl_is_checked();
  test_an_array_impl_needs_its_trait();
  test_an_impl_parameter_hides_a_type_name();
  test_an_impl_declares_what_its_target_names();
  test_a_length_has_to_be_declared();
  test_a_method_can_not_reuse_an_impls_name();
  test_two_lengths_are_different();
  test_const_parameters_work_with_type_parameters();

  printf("primitive method tests passed\n");
  return 0;
}
