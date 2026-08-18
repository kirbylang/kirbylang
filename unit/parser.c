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
  assert_ast("../tests/empty.krb", "");

  assert_ast("../tests/arrays/array_empty.krb", "(array)\n");

  assert_ast("../tests/comments.krb", "(print \"Hello World\")\n");

  assert_ast("../tests/method_invoke_on_non_instance_string.krb",
             "(call (get \"Hello World\" fn))\n");

  assert_ast("../tests/blocks/block_expression_operation_sum.krb",
             "(print (+ 10 (block (value 20))))\n");

  assert_ast("../tests/strings/string_concat.krb",
             "(print (+ (+ \"Hello\" \" \") \"World\"))\n");

  assert_ast("../tests/primitives/bool_false.krb", "(print false)\n");

  assert_ast("../tests/native_functions/native_fn_len_call.krb",
             "(print (call len \"Hello World\"))\n");

  assert_ast("../tests/flow_control/for.krb",
             "(for (var i 0) (< i 10) (assign i (+ i 1)) (block (print "
             "\"done\")))\n");

  assert_ast("../tests/flow_control/if/expression/if_else.krb",
             ""
             "(print (call test true))\n"
             "(print (call test false))\n"
             "(fun test (a) (if a (block (value \"Hello\")) \"World\"))\n");

  assert_ast("../tests/functions/function_body_expressions.krb",
             "(var n 10)\n"
             "(fun sum (a) (+ a n))\n"
             "(print (call sum 50))\n");

  assert_ast("../tests/functions/function_implicit_return.krb",
             "(fun sum (a b) (block (value (+ a b))))\n"
             "(print (call sum 1 2))\n");

  assert_ast("../tests/functions/function_return_semicolon.krb",
             "(fun function () (block (return)))\n"
             "(print function)\n"
             "(print (call function))\n");

  assert_ast("../tests/closures/upvalue_closed.krb",
             "(fun outer () (block (var x \"outside\")"
             " (fun inner () (block (print x))) (return inner)))\n"
             "(var closure (call outer))\n"
             "(call closure)\n");

  assert_ast("../tests/assignments/block_assignment_edge_case.krb",
             "(block (var a \"outer\") (block (var a a)))\n");

  assert_ast("../tests/arrays/array_index_get.krb",
             "(var array (array 1 2 3))\n"
             "(print (index-get array 2))\n");

  assert_ast("../tests/arrays/array_index_set.krb",
             "(var array (array 1 2 3))\n"
             "(index-set array 2 100)\n"
             "(print (index-get array 2))\n");

  assert_ast("../tests/arrays/array_multidimensional.krb",
             "(var array (array (array 10 20) (array 30 40)))\n"
             "(print (index-get (index-get array 0) 0))\n"
             "(print (index-get (index-get array 0) 1))\n"
             "(print (index-get (index-get array 1) 0))\n"
             "(print (index-get (index-get array 1) 1))\n");

  assert_ast("../tests/types/var_type_annotation.krb",
             "(var name : String \"Hello\")\n"
             "(print name)\n");

  assert_ast("../tests/types/let_type_annotation.krb", "(let count : u8 3)\n"
                                                       "(print count)\n");

  assert_ast("../tests/types/var_no_annotation.krb", "(var x 5)\n"
                                                     "(print x)\n");

  assert_ast("../tests/types/struct_field_annotation.krb",
             "(struct Point (pub-field x : i64) (pub-field y : i64))\n"
             "(impl Point (pub-static new))\n"
             "(var p (call (get Point new) 1 2))\n"
             "(print (get p x))\n"
             "(print (get p y))\n");

  assert_ast("../tests/types/struct_field_no_annotation.krb",
             "(struct Point (pub-field x) (pub-field y))\n"
             "(impl Point (pub-static new))\n"
             "(var p (call (get Point new) 1 2))\n"
             "(print (get p x))\n"
             "(print (get p y))\n");

  assert_ast("../tests/types/function_typed_params.krb",
             "(fun add (a : i64 b : i64) (+ a b))\n"
             "(print (call add 1 2))\n");

  assert_ast("../tests/types/function_return_type.krb",
             "(fun add (a b) : i64 (+ a b))\n"
             "(print (call add 1 2))\n");

  assert_ast("../tests/types/function_typed_params_and_return.krb",
             "(fun add (a : i64 b : i64) : i64 (+ a b))\n"
             "(print (call add 1 2))\n");

  assert_ast("../tests/types/lambda_typed_params.krb",
             "(var add (lambda (a : i64 b : i64) (block (value (+ a "
             "b)))))\n"
             "(print (call add 1 2))\n");

  assert_ast("../tests/types/function_no_annotation.krb",
             "(fun add (a b) (+ a b))\n"
             "(print (call add 1 2))\n");

  assert_ast("../tests/types/generic_type_var.krb",
             "(let value : Wrapper[f64] 123)\n"
             "(print value)\n");

  assert_ast(
      "../tests/types/generic_type_nested.krb",
      "(let value : List[List[i64]] (array (array 1) (array 2) (array 3)))\n"
      "(print value)\n");

  assert_ast("../tests/types/generic_type_struct.krb",
             "(struct Box (pub-field value : Wrapper[f64]))\n"
             "(let box (struct-init Box (field value 100)))\n"
             "(print (get box value))\n");

  assert_ast("../tests/types/generic_type_function.krb",
             "(fun identity (x : String) : List[String] x)\n"
             "(print (group (call identity (array \"Hello\" \"World\"))))\n");

  assert_ast("../tests/types/type_alias_simple.krb", "(type Wrapper f64)\n");

  assert_ast("../tests/types/type_alias_generic.krb", "(type Wrapper[T] T)\n");

  return 0;
}
