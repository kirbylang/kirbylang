#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// Importing *.c file so the tests can set up globals directly.
#include "../src/vm.c"

static void test_no_main(void) {
  initVM(0, NULL);

  assert(interpretMain() == INTERPRET_RUNTIME_ERROR);

  // Nothing is left on the stack after the error.
  assert(vm.stackTop == vm.stack);

  freeVM();
}

static void test_main_is_not_a_function(void) {
  initVM(0, NULL);

  ObjString *name = copyString(vm.gc, "main", 4);

  // tableSet can grow the table and collect, so name must stay rooted.
  pushOnStack(OBJ_VAL(name));
  tableSet(vm.gc, &vm.globals, name, NUMBER_F64_VAL(1));
  popFromStack();

  assert(interpretMain() == INTERPRET_RUNTIME_ERROR);
  assert(vm.stackTop == vm.stack);

  freeVM();
}

static void test_main_takes_a_parameter(void) {
  initVM(0, NULL);

  ObjString *name = copyString(vm.gc, "main", 4);
  pushOnStack(OBJ_VAL(name));

  ObjFunction *function = newFunction(vm.gc);
  function->arity = 1;
  pushOnStack(OBJ_VAL(function));

  ObjClosure *closure = newClosure(vm.gc, function);
  pushOnStack(OBJ_VAL(closure));

  tableSet(vm.gc, &vm.globals, name, OBJ_VAL(closure));
  resetStack();

  // interpretMain passes no arguments, so the call is refused.
  assert(interpretMain() == INTERPRET_RUNTIME_ERROR);
  assert(vm.stackTop == vm.stack);

  freeVM();
}

int main(void) {
  test_no_main();
  test_main_is_not_a_function();
  test_main_takes_a_parameter();

  printf("vm_interpret_main: ok\n");
  return 0;
}
