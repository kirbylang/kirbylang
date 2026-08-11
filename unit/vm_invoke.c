#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

// Importing *.c file to access the static `invoke` function
#include "../src/vm.c"

/**
 * Regression test for invoke() replacing the receiver when an instance field
 * holds a callable.
 *
 * peekStack(d) is vm.stackTop[-1 - d], so the receiver sits at
 * vm.stackTop[-argCount - 1]. Writing to vm.stackTop[argCount - 1] instead is
 * correct only when argCount == 0; for argCount >= 1 it writes above the stack
 * top and leaves the receiver in place.
 *
 * call() derives frame->slots from stackTop independently, so slot 0 of the new
 * frame is exactly the slot invoke() was supposed to overwrite. Asserting on it
 * observes the bug directly -- no kirby program can, because lambdas never read
 * slot 0.
 *
 * Every intermediate below is pushed onto the stack before the next allocation,
 * because a C local is not a GC root and DEBUG_STRESS_GC collects on every
 * growing allocation.
 */
static void assertReceiverReplaced(int argCount) {
  initVM(0, NULL);

  ObjString *fieldName = copyString(vm.gc, "f", 1);
  pushOnStack(OBJ_VAL(fieldName));

  ObjString *structName = copyString(vm.gc, "Box", 3);
  pushOnStack(OBJ_VAL(structName));

  // newStruct allocates, so structName must already be rooted.
  ObjStruct *struct_ = newStruct(vm.gc, structName);
  pushOnStack(OBJ_VAL(struct_));

  // tableSet can grow the table and collect, so fieldName must stay rooted.
  tableSet(vm.gc, &struct_->fields, fieldName, NUMBER_VAL(0));
  struct_->fieldCount = 1;
  struct_->fieldPublic[0] = true;

  ObjFunction *function = newFunction(vm.gc);
  function->arity = argCount;
  pushOnStack(OBJ_VAL(function));

  ObjClosure *closure = newClosure(vm.gc, function);
  pushOnStack(OBJ_VAL(closure));

  ObjInstance *instance = newInstance(vm.gc, struct_);
  pushOnStack(OBJ_VAL(instance));

  instance->fields[0] = OBJ_VAL(closure);

  // Everything is now reachable from the instance:
  //   instance -> struct_ -> name, and -> fields table -> fieldName
  //   instance -> fields[0] -> closure -> function
  // so the scratch entries can be dropped. resetStack does not allocate, and
  // nothing allocates before the receiver goes back on.
  resetStack();

  // Stack shape at OP_INVOKE: receiver, then argCount arguments.
  pushOnStack(OBJ_VAL(instance));
  for (int i = 0; i < argCount; i++) {
    pushOnStack(NUMBER_VAL(i));
  }

  bool ok = invoke(fieldName, argCount);
  assert(ok);

  // call() pushed a frame; its slot 0 is the receiver slot invoke() rewrote.
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
  Value slotZero = frame->slots[0];

  assert(IS_OBJ(slotZero));
  assert(AS_OBJ(slotZero) == (Obj *)closure);

  freeVM();
}

static void test_zero_args(void) { assertReceiverReplaced(0); }
static void test_one_arg(void) { assertReceiverReplaced(1); }
static void test_two_args(void) { assertReceiverReplaced(2); }
static void test_three_args(void) { assertReceiverReplaced(3); }

int main(void) {
  // Passes before and after the fix: argCount - 1 == -argCount - 1 at zero.
  test_zero_args();

  // Fail before the fix.
  test_one_arg();
  test_two_args();
  test_three_args();

  printf("vm_invoke: ok\n");
  return 0;
}
