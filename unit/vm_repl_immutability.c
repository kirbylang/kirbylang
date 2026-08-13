#include <assert.h>
#include <stdio.h>

#include "../src/ast.h"
#include "../src/compiler.h"
#include "../src/parser.h"
#include "../src/vm.h"

/**
 * Parse + compile source and run it, mirroring what main.c's compileSource()
 * + interpret() do. Must be called within a compilerSessionBegin()/
 * compilerSessionEnd() session.
 */
static InterpretResult run(const char *source) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadError, &endLine);

  CompiledUnit *unit = hadError ? NULL : compile(ast, count, endLine);

  astFreeAll();
  free(ast);

  if (unit == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  return interpret(unit);
}

/**
 * Regression test: immutableGlobals must persist for the life of a compiler
 * session, not just for the duration of a single compile() call.
 *
 * `let` bindings are tracked as immutable purely by name, at compile time,
 * in a table scoped to a compilerSessionBegin()/compilerSessionEnd() pair.
 * The REPL compiles each line as its own compile() call within one session
 * -- a `let` declared on one line must still be rejected for reassignment on
 * a later line in the same session.
 *
 * A single-file `.krb` E2E test can't exercise this: it's one compile()
 * call, so the bug never triggers there. This has to be driven with
 * multiple compile() calls in the same session, the way repl() does.
 */
static void test_let_reassign_rejected_across_separate_compile_calls(void) {
  initVM(0, NULL);

  assert(run("let x = 1;") == INTERPRET_OK);

  // Before the fix: immutableGlobals was cleared when the first compile()
  // returned, so this second, separate compile() call had no record that
  // 'x' was declared immutable, and let the assignment through.
  assert(run("x = 2;") == INTERPRET_COMPILE_ERROR);

  compilerSessionEnd();
  freeVM();
}

/**
 * Guards against an overcorrection: the fix must not make every global look
 * immutable across calls -- only names actually declared with `let`.
 */
static void test_var_reassign_allowed_across_separate_compile_calls(void) {
  initVM(0, NULL);

  assert(run("var y = 1;") == INTERPRET_OK);
  assert(run("y = 2;") == INTERPRET_OK);

  compilerSessionEnd();
  freeVM();
}

/**
 * Multiple `let` names declared across separate calls must each stay
 * individually protected -- not just the most recently declared one.
 */
static void test_multiple_immutable_names_all_persist(void) {
  initVM(0, NULL);

  assert(run("let a = 1;") == INTERPRET_OK);
  assert(run("let b = 2;") == INTERPRET_OK);
  assert(run("let c = 3;") == INTERPRET_OK);

  assert(run("a = 10;") == INTERPRET_COMPILE_ERROR);
  assert(run("b = 20;") == INTERPRET_COMPILE_ERROR);
  assert(run("c = 30;") == INTERPRET_COMPILE_ERROR);

  compilerSessionEnd();
  freeVM();
}

/**
 * A session must not leak into a later, unrelated session -- e.g. two
 * separate REPL runs (or two separate VM lifetimes) in the same process must
 * not share immutable-global state just because they share a process.
 */
static void test_session_state_does_not_leak_into_next_session(void) {
  initVM(0, NULL);
  assert(run("let x = 1;") == INTERPRET_OK);
  compilerSessionEnd();
  freeVM();

  initVM(0, NULL);
  // Fresh session, fresh VM -- 'x' was never declared here. If
  // compilerSessionEnd() failed to reset immutableGlobals, this would
  // incorrectly report a compile error reassigning an immutable 'x' instead
  // of running the declaration cleanly.
  assert(run("var x = 1; x = 2;") == INTERPRET_OK);
  compilerSessionEnd();
  freeVM();
}

int main(void) {
  test_let_reassign_rejected_across_separate_compile_calls();
  test_var_reassign_allowed_across_separate_compile_calls();
  test_multiple_immutable_names_all_persist();
  test_session_state_does_not_leak_into_next_session();

  printf("vm_repl_immutability: ok\n");
  return 0;
}
