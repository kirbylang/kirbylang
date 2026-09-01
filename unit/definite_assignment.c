#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/definite_assignment.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/typecheck.h"

static void test_daa_starts_empty(void) {
  DaaSet daa;
  daaSetInit(&daa);
  assert(daa.count == 0);
  daaSetFree(&daa);
}

// --- daaCheckFn, called directly on a parsed FunctionNode ---

static FunctionNode *parseFirstFunction(const char *source) {
  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  assert(outCount >= 1);
  assert(ast[0]->kind == NODE_FUNCTION);
  return &ast[0]->as.function;
}

static void test_check_definite_assignment_direct_read_before_assign(void) {
  typchkResetError();
  FunctionNode *fn =
      parseFirstFunction("fun f(): f64 { var x: f64; return x; }");
  daaCheckFn(fn);
  assert(typchkHadError());
  typchkResetError();
}

static void test_check_definite_assignment_direct_sequential_is_fine(void) {
  typchkResetError();
  FunctionNode *fn =
      parseFirstFunction("fun f(): f64 { var x: f64; x = 5; return x; }");
  daaCheckFn(fn);
  assert(!typchkHadError());
}

// --- whole-program driver tests (typchkCheckProgram already runs this pass)
// ---

static bool typecheckSource(const char *source) {
  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  return typchkCheckProgram(ast, outCount);
}

static void test_definite_assignment_sequential(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(): f64 { var x: f64; x = 5; return x; }");
  assert(ok);
}

static void test_definite_assignment_read_before_assignment_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(): f64 { var x: f64; return x; }");
  assert(!ok);
}

static void test_definite_assignment_self_reference_errors(void) {
  typchkResetError();
  // x = x + 1 -- the RHS is evaluated before the assignment takes
  // effect, so this must still be caught as a read-before-assignment.
  bool ok =
      typecheckSource("fun f(): f64 { var x: f64; x = x + 1; return x; }");
  assert(!ok);
}

static void test_definite_assignment_initialized_var_never_tracked(void) {
  typchkResetError();
  // Has an initializer -- never pending, safe to read immediately.
  bool ok = typecheckSource("fun f(): f64 { var x: f64 = 1; return x; }");
  assert(ok);
}

static void test_definite_assignment_if_both_branches_assign(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(cond: bool): f64 {\n"
                            "  var x: f64;\n"
                            "  if (cond) { x = 1; } else { x = 2; }\n"
                            "  return x;\n"
                            "}\n");
  assert(ok);
}

static void test_definite_assignment_if_only_one_branch_assigns_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(cond: bool): f64 {\n"
                            "  var x: f64;\n"
                            "  if (cond) { x = 1; }\n"
                            "  return x;\n"
                            "}\n");
  assert(!ok);
}

static void test_definite_assignment_if_no_else_at_all_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(cond: bool): f64 {\n"
                            "  var x: f64;\n"
                            "  if (cond) { x = 1; }\n"
                            "  return x;\n"
                            "}\n");
  assert(!ok);
}

static void
test_definite_assignment_early_return_narrows_to_other_branch(void) {
  typchkResetError();
  // Only reachable via the else branch, which assigns -- the then
  // branch's early return means it never falls through to `return x;`.
  bool ok = typecheckSource("fun f(cond: bool): f64 {\n"
                            "  var x: f64;\n"
                            "  if (cond) { return 0; } else { x = 1; }\n"
                            "  return x;\n"
                            "}\n");
  assert(ok);
}

static void test_definite_assignment_while_body_not_definite_after(void) {
  typchkResetError();
  // The loop might run zero times -- its assignment isn't definite once
  // the loop is done, even though it looks like it "obviously" runs.
  bool ok = typecheckSource("fun f(cond: bool): f64 {\n"
                            "  var x: f64;\n"
                            "  while (cond) { x = 1; }\n"
                            "  return x;\n"
                            "}\n");
  assert(!ok);
}

static void test_definite_assignment_for_body_not_definite_after(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(): f64 {\n"
                            "  var x: f64;\n"
                            "  for (var i = 0; i < 10; i = i + 1) { x = i; }\n"
                            "  return x;\n"
                            "}\n");
  assert(!ok);
}

static void test_definite_assignment_nested_function_independent_scope(void) {
  typchkResetError();
  // Intraprocedural by design: an assignment inside a *called* function
  // isn't visible to the caller's own analysis (matches Java/C#/Rust's
  // definite-assignment, none of which trace effects through a call).
  bool ok = typecheckSource("fun outer(): unit {\n"
                            "  var x: f64;\n"
                            "  fun inner(): unit { x = 1; }\n"
                            "  inner();\n"
                            "}\n");
  assert(ok);
}

static void test_definite_assignment_top_level_uninitialized_var(void) {
  typchkResetError();
  // A top-level uninitialized var, assigned and read at the top level
  // (not inside any function) -- the ordinary case, no cross-function
  // reasoning involved.
  bool ok = typecheckSource("var x: f64;\n"
                            "x = 5;\n"
                            "print x;\n");
  assert(ok);
}

static void test_definite_assignment_top_level_read_before_assign_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("var x: f64;\n"
                            "print x;\n");
  assert(!ok);
}

static void test_definite_assignment_cross_function_pattern_is_rejected(void) {
  typchkResetError();
  // The exact shape upvalue_global.krb uses: a top-level uninitialized
  // var, assigned *inside* a function body, read at the top level after
  // that function is called. A properly-scoped intraprocedural analysis
  // can't trace the assignment through the call, so this is correctly
  // rejected even though the runtime behavior is fine.
  bool ok = typecheckSource("var globalSet: fun () => unit;\n"
                            "fun main(): unit {\n"
                            "  fun setIt(): unit {}\n"
                            "  globalSet = setIt;\n"
                            "}\n"
                            "main();\n"
                            "globalSet();\n");
  assert(!ok);
}

int main(void) {
  test_daa_starts_empty();

  test_check_definite_assignment_direct_read_before_assign();
  test_check_definite_assignment_direct_sequential_is_fine();

  test_definite_assignment_sequential();
  test_definite_assignment_read_before_assignment_errors();
  test_definite_assignment_self_reference_errors();
  test_definite_assignment_initialized_var_never_tracked();
  test_definite_assignment_if_both_branches_assign();
  test_definite_assignment_if_only_one_branch_assigns_errors();
  test_definite_assignment_if_no_else_at_all_errors();
  test_definite_assignment_early_return_narrows_to_other_branch();
  test_definite_assignment_while_body_not_definite_after();
  test_definite_assignment_for_body_not_definite_after();
  test_definite_assignment_nested_function_independent_scope();
  test_definite_assignment_top_level_uninitialized_var();
  test_definite_assignment_top_level_read_before_assign_errors();
  test_definite_assignment_cross_function_pattern_is_rejected();

  astFreeAll();

  return 0;
}
