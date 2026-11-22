#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

int main(void) {
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
