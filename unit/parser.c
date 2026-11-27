#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../src/ast.h"
#include "../src/gc.h"
#include "../src/parser.h"
#include "../src/strbuf.h"
#include "../src/token.h"

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

void assert_ast(const char *path, const char *expected) {
  const char *source = readFile(path);

  int outCount = 0;
  bool hadError = false;
  int endLineOut = 0;

  AstNode **ast = parse(source, &outCount, &hadError, &endLineOut);

  StrBuf ast_sb = {0};
  sb_init(&ast_sb);

  for (int i = 0; i < outCount; i++) {
    StrBuf ast_node_sb;
    sb_init(&ast_node_sb);

    print_ast(&ast_node_sb, ast[i]);
    sb_appendf(&ast_sb, "%s\n", ast_node_sb.data);
    sb_free(&ast_node_sb);
  }

  bool passed = strcmp(expected, ast_sb.data) == 0;

  StrBuf sb_diff = {0};
  sb_init(&sb_diff);

  sb_appendf(
      &sb_diff,
      "AST didn't match expected AST\n\nExpected:\n%s\n\nActual:\n%s\n\n",
      expected, ast_sb.data);

  if (!passed) {
    fprintf(stderr, "%s", sb_diff.data);
  }

  assert(passed);

  sb_free(&sb_diff);
  sb_free(&ast_sb);
  astFreeAll();
  free(ast);
}

// Parses `source` as the standard library or as a user file. Returns the
// syntax tree as text and the errors it printed, which the caller frees.
typedef struct {
  char *ast;
  char *errors;
  bool hadError;
} ParseResult;

static ParseResult parseText(const char *source, bool isStdlib) {
  fflush(stderr);
  int saved = dup(STDERR_FILENO);
  FILE *capture = tmpfile();
  assert(saved >= 0 && capture != NULL);
  dup2(fileno(capture), STDERR_FILENO);

  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = isStdlib ? parseStdlib(source, &count, &hadError, &endLine)
                           : parse(source, &count, &hadError, &endLine);

  fflush(stderr);
  dup2(saved, STDERR_FILENO);
  close(saved);

  StrBuf text;
  sb_init(&text);

  for (int i = 0; i < count; i++) {
    StrBuf node;
    sb_init(&node);
    print_ast(&node, ast[i]);
    sb_appendf(&text, "%s\n", node.data);
    sb_free(&node);
  }

  long size = ftell(capture);
  char *errors = (char *)calloc((size_t)size + 1, 1);
  rewind(capture);
  size_t got = fread(errors, 1, (size_t)size, capture);
  assert((long)got == size);
  fclose(capture);

  ParseResult result = {strdup(text.data), errors, hadError};
  sb_free(&text);
  astFreeAll();
  free(ast);
  return result;
}

static void freeResult(ParseResult *result) {
  free(result->ast);
  free(result->errors);
}

static void expectStdlibAst(const char *source, const char *expected) {
  ParseResult result = parseText(source, true);
  assert(!result.hadError);

  if (strcmp(result.ast, expected) != 0)
    fprintf(stderr, "Expected:\n%s\nActual:\n%s\n", expected, result.ast);

  assert(strcmp(result.ast, expected) == 0);
  freeResult(&result);
}

static void expectStdlibError(const char *source, const char *message) {
  ParseResult result = parseText(source, true);
  assert(result.hadError);

  if (strstr(result.errors, message) == NULL)
    fprintf(stderr, "Wanted '%s' in:\n%s\n", message, result.errors);

  assert(strstr(result.errors, message) != NULL);
  freeResult(&result);
}

static const char *listImplMessage =
    "You can't write an impl for a list or array in user code.";

// The only error a user gets for a list or array impl, however the rest of the
// block reads
static void expectUserListError(const char *source) {
  ParseResult result = parseText(source, false);
  assert(result.hadError);

  if (strstr(result.errors, listImplMessage) == NULL)
    fprintf(stderr, "Wanted the list impl error in:\n%s\n", result.errors);

  assert(strstr(result.errors, listImplMessage) != NULL);
  freeResult(&result);
}

static void test_the_stdlib_can_impl_lists_and_arrays(void) {
  expectStdlibAst("impl[T] [T] { pub fun first(self): T = self[0]; }",
                  "(impl[T] [T] (pub-method first))\n");
  expectStdlibAst(
      "impl[T] Display for [T] { fun toString(self): string = \"\"; }",
      "(impl[T] Display for [T] (pub-method toString))\n");
  expectStdlibAst(
      "impl[T, const N] [T; N] { pub fun first(self): T = self[0]; }",
      "(impl[T const N] [T; N] (pub-method first))\n");
  expectStdlibAst("impl[T] [T; _] { pub fun first(self): T = self[0]; }",
                  "(impl[T] [T; _] (pub-method first))\n");
  expectStdlibAst("impl[const N, T] Display for [T; N] { fun toString(self): "
                  "string = \"\"; }",
                  "(impl[const N T] Display for [T; N] (pub-method "
                  "toString))\n");

  // A number is read, and the checker rejects it
  expectStdlibAst("impl[T] [T; 3] { pub fun first(self): T = self[0]; }",
                  "(impl[T] [T; 3] (pub-method first))\n");
}

static void test_a_list_or_array_impl_has_to_be_well_formed(void) {
  expectStdlibError("impl [T] { }", "Declare the type parameter after 'impl', "
                                    "as in 'impl[T] [T]'.");
  expectStdlibError("impl [T; N] { }",
                    "Declare the parameters after 'impl', as in "
                    "'impl[T, const N] [T; N]'.");
  expectStdlibError("impl Display for [T] { }",
                    "Declare the type parameter after 'impl', as in "
                    "'impl[T] Display for [T]'.");
  expectStdlibError("impl[T] [] { }", "Expect the item type's name after '['.");
  expectStdlibError("impl[T] [T {\n}", "Expect ']' after the type.");
  expectStdlibError("impl[T] [T, U] { }", "Expect ']' after the type.");
  expectStdlibError("impl[T] [T; ] { }",
                    "Expect a name or '_' for the array's length.");
  expectStdlibError("impl[T: Display] [T] { }",
                    "A list or array impl's parameters can't have bounds.");
  expectStdlibError("impl[T] Box[T] { }",
                    "A struct's impl takes its type parameters from the "
                    "struct, as in 'impl Box[T]'.");
}

static void test_self_can_not_name_a_type_parameter(void) {
  const char *message = "'Self' can't be the name of a type parameter.";

  expectStdlibError("fun f[Self](x: Self): Self = x;", message);
  expectStdlibError("impl[Self] [Self] { }", message);
  expectStdlibError("impl[T, const Self] [T; Self] { }", message);
}

static void test_functions_can_declare_const_parameters(void) {
  expectStdlibAst("native fun @x[T, const N](a: [T; N]): [T; N];",
                  "(native-fun @x[T const N] (a : [T; N]) : [T; N])\n");

  expectStdlibError("fun f[const N: Display](): unit {}",
                    "A const parameter can't have bounds.");
  expectStdlibError("fun f[const](): unit {}",
                    "Expect generic parameter name.");
  expectStdlibError("struct S[const N] {}",
                    "Only functions and list or array impls can declare const "
                    "parameters.");
  expectStdlibError("type A[const N] = i64;",
                    "Only functions and list or array impls can declare const "
                    "parameters.");
  expectStdlibError("struct S {}\nimpl S[const N] { }",
                    "Only functions and list or array impls can declare const "
                    "parameters.");
}

static const char *primitiveNames[] = {"unit", "bool", "string", "f64", "f32",
                                       "i64",  "i32",  "i16",    "i8",  "u64",
                                       "u32",  "u16",  "u8"};

static void test_the_stdlib_can_declare_natives(void) {
  expectStdlibAst("native fun @listPop[T](list: [T]): T;",
                  "(native-fun @listPop[T] (list : [T]) : T)\n");
}

static void test_a_native_declaration_has_to_be_well_formed(void) {
  expectStdlibError("native fun len(): i64;",
                    "A native's name starts with '@'.");
  expectStdlibError("native fun @x(): unit {}",
                    "Expect ';' after native declaration.");
  expectStdlibError("native fun @x(): unit = nil;",
                    "Expect ';' after native declaration.");
  expectStdlibError("native let @x = 1;", "Expect 'fun' after 'native'.");
  expectStdlibError("fun @x(): unit {}", "'@' is a reserved character.");
  expectStdlibError("fun f(): unit;", "Expect '{' before function body.");
  expectStdlibError("fun f(): unit { native fun @x(): unit; }",
                    "'native' declarations can only appear at the top level.");
}

static void test_user_code_can_impl_a_primitive(void) {
  int count = (int)(sizeof(primitiveNames) / sizeof(primitiveNames[0]));
  char source[256];

  for (int i = 0; i < count; i++) {
    snprintf(source, sizeof(source), "impl %s { pub fun a(self): %s = self; }",
             primitiveNames[i], primitiveNames[i]);
    ParseResult plain = parseText(source, false);
    assert(!plain.hadError);
    freeResult(&plain);

    snprintf(source, sizeof(source),
             "impl Label for %s { fun label(self): string = \"\"; }",
             primitiveNames[i]);
    ParseResult trait = parseText(source, false);
    assert(!trait.hadError);
    freeResult(&trait);

    snprintf(source, sizeof(source), "impl[T] Label for %s { }",
             primitiveNames[i]);
    ParseResult generic = parseText(source, false);
    assert(generic.hadError);
    assert(strstr(generic.errors,
                  "An impl for a built-in type has no type parameters.") !=
           NULL);
    freeResult(&generic);
  }
}

static void test_user_code_can_not_impl_a_list_or_array(void) {
  const char *others[] = {
      "impl [T] { }",
      "impl [T; N] { }",
      "impl [T; _] { }",
      "impl Display for [T] { }",
      "impl Display for [T; N] { }",
      "impl[T] [T] { }",
      "impl[T, const N] [T; N] { }",
      "impl[T] Display for [T] { }",
      // However the target is spelled, the user sees this error alone
      "impl [T; 3] { }",
      "impl [T, U] { }",
      "impl [] { }",
      "impl [T; ] { }",
      "impl [T {\n}",
  };

  for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
    expectUserListError(others[i]);

    ParseResult result = parseText(others[i], false);
    assert(strstr(result.errors, "Expect") == NULL);
    assert(strstr(result.errors, "array impl") == NULL);
    freeResult(&result);
  }

  // Parameters declared after `impl` for a struct are a different mistake
  const char *structs[] = {
      "impl[T] Box[T] { }",
      "impl[T] Display for Box[T] { }",
  };

  for (size_t i = 0; i < sizeof(structs) / sizeof(structs[0]); i++) {
    ParseResult result = parseText(structs[i], false);
    assert(strstr(result.errors, "A struct's impl takes its type parameters "
                                 "from the struct") != NULL);
    freeResult(&result);
  }
}

static void test_user_code_can_impl_structs(void) {
  ParseResult result =
      parseText("struct Box { pub var value: i64; }\n"
                "impl Box { pub fun get(self): i64 = self.value; }\n"
                "impl Display for Box { fun toString(self): string = \"\"; }\n",
                false);
  assert(!result.hadError);
  assert(strcmp(result.errors, "") == 0);
  freeResult(&result);
}

int main(void) {
  test_the_stdlib_can_declare_natives();
  test_a_native_declaration_has_to_be_well_formed();
  test_the_stdlib_can_impl_lists_and_arrays();
  test_a_list_or_array_impl_has_to_be_well_formed();
  test_self_can_not_name_a_type_parameter();
  test_functions_can_declare_const_parameters();
  test_user_code_can_impl_a_primitive();
  test_user_code_can_not_impl_a_list_or_array();
  test_user_code_can_impl_structs();

  assert_ast("../tests/run/empty.krb", "(fun main () : unit (block))\n");

  assert_ast("../tests/run/lists/array_empty.krb",
             "(let empty : [i64] (array))\n"
             "(fun main () : unit (block))\n");

  assert_ast("../tests/run/comments.krb",
             "(fun main () : unit (block (call @println \"Hello World\")))\n");

  assert_ast("../tests/run/method_invoke_on_non_instance_string.krb",
             "(fun main () : unit (block (call (get \"Hello World\" fn))))\n");

  assert_ast("../tests/run/blocks/block_expression_operation_sum.krb",
             "(fun main () : unit (block (call @println (+ 10 (block (value "
             "20))))))\n");

  assert_ast("../tests/run/strings/string_concat.krb",
             "(fun main () : unit (block (call @println (+ (+ \"Hello\" \" \") "
             "\"World\"))))\n");

  assert_ast("../tests/run/primitives/bool_false.krb",
             "(fun main () : unit (block (call @println false)))\n");

  assert_ast("../tests/run/native_functions/native_fn_str_len_call.krb",
             "(fun main () : unit (block (call @println (call @strLen \"Hello "
             "World\")) (call @println (call @strLen \"\"))))\n");

  assert_ast("../tests/run/flow_control/for.krb",
             "(fun main () : unit (block (for (var i 0) (< i 10) (assign i (+ "
             "i 1)) (block (call @println \"done\")))))\n");

  assert_ast("../tests/run/flow_control/if/expression/if_else.krb",
             "(fun main () : unit (block (call @println (call test true)) "
             "(call @println (call test false))))\n"
             "(fun test (a : bool) : string (if a (block (value \"Hello\")) "
             "\"World\"))\n");

  assert_ast("../tests/run/functions/function_body_expressions.krb",
             "(var n 10)\n"
             "(fun sum (a : i64) : i64 (+ a n))\n"
             "(fun main () : unit (block (call @println (call sum 50))))\n");

  assert_ast("../tests/run/functions/function_implicit_return.krb",
             "(fun sum (a : f64 b : f64) : f64 (block (value (+ a b))))\n"
             "(fun main () : unit (block (call @println (call sum 1 2))))\n");

  assert_ast("../tests/run/functions/function_return_semicolon.krb",
             "(fun function () : unit (block (return)))\n"
             "(fun main () : unit (block (call @println function) (call "
             "@println (call function))))\n");

  assert_ast("../tests/run/closures/upvalue_closed.krb",
             "(fun outer () : (fun () => unit) (block (var x \"outside\") (fun "
             "inner () : unit (block (call @println x))) (return inner)))\n"
             "(fun main () : unit (block (var closure (call outer)) (call "
             "closure)))\n");

  assert_ast("../tests/run/assignments/block_assignment_edge_case.krb",
             "(fun main () : unit (block (value (block (var a \"outer\") "
             "(value (block (var a a)))))))\n");

  assert_ast(
      "../tests/run/lists/array_index_get.krb",
      "(var array (array 1 2 3))\n"
      "(fun main () : unit (block (call @println (index-get array 2))))\n");

  assert_ast("../tests/run/lists/array_index_set.krb",
             "(var array (array 1 2 3))\n"
             "(fun main () : unit (block (index-set array 2 100) (call "
             "@println (index-get array 2))))\n");

  assert_ast("../tests/run/lists/array_multidimensional.krb",
             "(var array (array (array 10 20) (array 30 40)))\n"
             "(fun main () : unit (block (call @println (index-get (index-get "
             "array 0) 0)) (call @println (index-get (index-get array 0) 1)) "
             "(call @println (index-get (index-get array 1) 0)) (call @println "
             "(index-get (index-get array 1) 1))))\n");

  assert_ast("../tests/run/types/var_type_annotation.krb",
             "(var name : string \"Hello\")\n"
             "(fun main () : unit (block (call @println name)))\n");

  assert_ast("../tests/run/types/let_type_annotation.krb",
             "(let count : f64 3)\n"
             "(fun main () : unit (block (call @println count)))\n");

  assert_ast("../tests/run/types/var_no_annotation.krb",
             "(var x 5)\n"
             "(fun main () : unit (block (call @println x)))\n");

  assert_ast("../tests/run/types/struct_field_annotation.krb",
             "(struct Point (pub-field x : f64) (pub-field y : f64))\n"
             "(impl Point (pub-static new))\n"
             "(fun main () : unit (block (var p (call (get Point new) 1 2)) "
             "(call @println (get p x)) (call @println (get p y))))\n");

  assert_ast("../tests/run/types/struct_field_no_annotation.krb",
             "(struct Point (pub-field x) (pub-field y))\n"
             "(impl Point (pub-static new))\n"
             "(fun main () : unit (block (var p (call (get Point new) 1 2)) "
             "(call @println (get p x)) (call @println (get p y))))\n");

  assert_ast("../tests/run/types/function_typed_params.krb",
             "(fun add (a : f64 b : f64) : f64 (+ a b))\n"
             "(fun main () : unit (block (call @println (call add 1 2))))\n");

  assert_ast("../tests/run/types/function_return_type.krb",
             "(fun add (a : f64 b : f64) : f64 (+ a b))\n"
             "(fun main () : unit (block (call @println (call add 1 2))))\n");

  assert_ast("../tests/run/types/function_typed_params_and_return.krb",
             "(fun add (a : f64 b : f64) : f64 (+ a b))\n"
             "(fun main () : unit (block (call @println (call add 1 2))))\n");

  assert_ast("../tests/run/types/lambda_typed_params.krb",
             "(var add (lambda (a : f64 b : f64) (block (value (+ a b)))))\n"
             "(fun main () : unit (block (call @println (call add 1 2))))\n");

  assert_ast("../tests/run/types/function_no_annotation.krb",
             "(fun add (a b) (+ a b))\n"
             "(fun main () : unit (block (call @println (call add 1 2))))\n");

  assert_ast("../tests/run/types/generic_type_var.krb",
             "(let value : Wrapper[f64] 123)\n"
             "(fun main () : unit (block (call @println value)))\n");

  assert_ast(
      "../tests/run/types/generic_type_nested.krb",
      "(let value : Array[Array[f64]] (array (array 1) (array 2) (array 3)))\n"
      "(fun main () : unit (block (call @println value)))\n");

  assert_ast("../tests/run/types/generic_type_struct.krb",
             "(struct Box (pub-field value : Wrapper[f64]))\n"
             "(let box (struct-init Box (field value 100)))\n"
             "(fun main () : unit (block (call @println (get box value))))\n");

  assert_ast(
      "../tests/run/types/generic_type_and_array_brackets_dont_conflict.krb",
      "(var typed : Array[f64] (array))\n"
      "(var numbers (array 1 2 3))\n"
      "(fun main () : unit (block (call @println (index-get numbers 0)) (call "
      "@println (index-get numbers 1))))\n");

  assert_ast("../tests/run/types/type_alias_simple.krb",
             "(type Wrapper f64)\n"
             "(fun main () : unit (block))\n");

  assert_ast("../tests/run/types/type_alias_generic.krb",
             "(type Wrapper[T] T)\n"
             "(fun main () : unit (block))\n");

  assert_ast("../tests/run/types/function_type_var.krb",
             "(var handler : (fun (f64) => f64) (lambda (x) (block (value (+ x "
             "1)))))\n"
             "(fun main () : unit (block (call @println (call handler 1))))\n");

  assert_ast("../tests/run/types/function_type_no_params.krb",
             "(fun main () : unit (block (var greeter : (fun () => unit)) "
             "(assign greeter greet) (call greeter)))\n"
             "(fun greet () : unit (block (call @println \"hi\")))\n");

  assert_ast("../tests/run/types/function_type_nested.krb",
             "(fun main () : unit (block (var apply : (fun ((fun (f64) => f64) "
             "f64) => f64)) (assign apply run) (call @println (call apply "
             "(lambda (x) (block (value (+ x 1)))) 5))))\n"
             "(fun run (f : (fun (f64) => f64) x : f64) : f64 (call f x))\n");

  assert_ast("../tests/run/types/generic_struct.krb",
             "(struct Box[T] (pub-field value : T))\n"
             "(impl Box[T] (pub-static new) (pub-method get))\n"
             "(fun main () : unit (block (var b (call (get Box new) 5)) (call "
             "@println (call (get b get)))))\n");

  assert_ast("../tests/run/types/generic_function.krb",
             "(fun sum[T] (a : T b : T) : T (+ a b))\n"
             "(fun main () : unit (block (call @println (call sum 1 2))))\n");

  assert_ast(
      "../tests/run/types/generic_function_multi_param.krb",
      "(fun first[T U] (a : T b : U) : T a)\n"
      "(fun main () : unit (block (call @println (call first 1 \"two\"))))\n");

  assert_ast(
      "../tests/run/types/trait_basic.krb",
      "(struct Point (pub-field x : f64) (pub-field y : f64))\n"
      "(impl Display for Point (pub-method toString))\n"
      "(var p (struct-init Point (field x 1) (field y 2)))\n"
      "(fun main () : unit (block (call @println (call (get p toString)))))\n");

  assert_ast("../tests/run/types/trait_supertrait.krb",
             "(trait MyEq (method equals (self other : Self) : bool))\n"
             "(trait MyOrd : MyEq (method cmp (self other : Self) : f64))\n"
             "(fun main () : unit (block))\n");

  assert_ast("../tests/run/types/trait_static_method.krb",
             "(trait Constructible (static-method make () : Self))\n"
             "(fun main () : unit (block))\n");

  return 0;
}
