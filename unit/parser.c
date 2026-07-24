#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/memory.h"
#include "../src/parser.h"
#include "../src/strbuf.h"
#include "../src/token.h"
#include "../src/vm.h"

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

void assert_ast(const char *path, const char *expected, int argc,
                char *argv[]) {
  int saved_argc = argc;
  char **saved_argv = malloc(sizeof(char *) * (argc + 1));
  for (int i = 0; i < argc; i++) {
    saved_argv[i] = argv[i];
  }
  saved_argv[argc] = NULL;

  initVM(saved_argc, saved_argv);

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

  freeVM();

  free(saved_argv);
}

int main(int argc, char *argv[]) {
  assert_ast("../tests/empty.lox", "", argc, argv);

  assert_ast("../tests/arrays/array_empty.lox",
             ""
             "(array)\n",
             argc, argv);

  assert_ast("../tests/comments.lox",
             ""
             "(print \"Hello World\")\n",
             argc, argv);

  assert_ast("../tests/method_invoke_on_non_instance_string.lox",
             ""
             "(call (get \"Hello World\" fn))\n",
             argc, argv);

  assert_ast("../tests/classes/class_field_access_invoke.lox",
             ""
             "(class Class (field fn))\n(var instance (call Class))\n"
             "(fun fn () (block (print \"Hello World\")))\n"
             "(set instance fn fn)\n"
             "(call (get instance fn))\n",
             argc, argv);

  assert_ast("../tests/blocks/block_expression_operation_sum.lox",
             ""
             "(print (+ 10 (block (value 20))))\n",
             argc, argv);

  assert_ast("../tests/strings/string_concat.lox",
             ""
             "(print (+ (+ \"Hello\" \" \") \"World\"))\n",
             argc, argv);

  assert_ast("../tests/primitives/bool_false.lox",
             ""
             "(print false)\n",
             argc, argv);

  assert_ast("../tests/native_functions/native_fn_len_call.lox",
             ""
             "(print (call len \"Hello World\"))\n",
             argc, argv);

  assert_ast("../tests/lambdas/lambda_iife.lox",
             ""
             "(print (call (group (lambda (message) (block (value message)))) "
             "\"Hello World\"))\n",
             argc, argv);

  assert_ast(
      "../tests/flow_control/for.lox",
      ""
      "(for (var i 0) (< i 10) (assign i (+ i 1)) (block (print \"done\")))\n",
      argc, argv);

  assert_ast("../tests/flow_control/if_and_then_false.lox",
             ""
             "(if (and true false) (block (print \"Hello\")))\n"
             "(print \"World\")\n",
             argc, argv);

  assert_ast(
      "../tests/flow_control/if_expression_else.lox",
      ""
      "(print (if true (block (value \"Hello\")) (block (value \"World\"))))\n"
      "(print (if false (block (value \"Hello\")) (block (value "
      "\"World\"))))\n",
      argc, argv);

  assert_ast("../tests/functions/function_body_expressions.lox",
             ""
             "(var n 10)\n"
             "(fun sum (a) (+ a n))\n"
             "(print (call sum 50))\n",
             argc, argv);

  assert_ast("../tests/functions/function_implicit_return.lox",
             ""
             "(fun sum (a b) (block (value (+ a b))))\n"
             "(print (call sum 1 2))\n",
             argc, argv);

  assert_ast("../tests/functions/function_return_semicolon.lox",
             ""
             "(fun function () (block (return)))\n"
             "(print function)\n"
             "(print (call function))\n",
             argc, argv);

  assert_ast("../tests/closures/upvalue_closed.lox",
             ""
             "(fun outer () (block (var x \"outside\")"
             " (fun inner () (block (print x))) (return inner)))\n"
             "(var closure (call outer))\n"
             "(call closure)\n",
             argc, argv);

  assert_ast("../tests/assignments/block_assignment_edge_case.lox",
             ""
             "(block (var a \"outer\") (block (var a a)))\n",
             argc, argv);

  assert_ast("../tests/arrays/array_index_get.lox",
             ""
             "(var array (array 1 2 3))\n"
             "(print (index-get array 2))\n",
             argc, argv);

  assert_ast("../tests/arrays/array_index_set.lox",
             ""
             "(var array (array 1 2 3))\n"
             "(index-set array 2 100)\n"
             "(print (index-get array 2))\n",
             argc, argv);

  assert_ast("../tests/arrays/array_multidimensional.lox",
             ""
             "(var array (array (array 10 20) (array 30 40)))\n"
             "(print (index-get (index-get array 0) 0))\n"
             "(print (index-get (index-get array 0) 1))\n"
             "(print (index-get (index-get array 1) 0))\n"
             "(print (index-get (index-get array 1) 1))\n",
             argc, argv);

  assert_ast("../tests/classes/class_instance_property.lox",
             ""
             "(class Class (field hello))\n"
             "(var instance (call Class))\n"
             "(set instance hello \"World\")\n"
             "(print (get instance hello))\n",
             argc, argv);

  return 0;
}
