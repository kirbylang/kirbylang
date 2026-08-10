#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../src/chunk.h"
#include "../src/hashtable.h"
#include "../src/object.h"
#include "../src/vm.h"

static ObjFunction *buildFieldChunk(ObjString *structName,
                                    ObjString *fieldName) {
  ObjFunction *function = newFunction();

  // addConstantToChunk and writeChunk both allocate, and either can trigger a
  // collection. Until interpretFunction pushes it, this function is reachable
  // only from a C local, which is not a root.
  pushOnStack(OBJ_VAL(function));

  Chunk *chunk = &function->chunk;

  int structConst = addConstantToChunk(chunk, OBJ_VAL(structName));
  int fieldConst = addConstantToChunk(chunk, OBJ_VAL(fieldName));

  // Stack shape OP_FIELD expects: struct at peek(1), initializer at peek(0).
  writeChunk(chunk, OP_GET_GLOBAL, 1);
  writeChunk(chunk, (uint8_t)structConst, 1);

  writeChunk(chunk, OP_NIL, 1);

  writeChunk(chunk, OP_FIELD, 1);
  writeChunk(chunk, (uint8_t)fieldConst, 1);
  writeChunk(chunk, 1, 1); // public

  writeChunk(chunk, OP_POP, 1);
  writeChunk(chunk, OP_NIL, 1);
  writeChunk(chunk, OP_RETURN, 1);

  // Safe to drop: interpretFunction pushes it again before anything allocates.
  popFromStack();

  return function;
}

static void test_field_limit_is_a_runtime_error(void) {
  initVM(0, NULL);

  ObjString *structName = copyString("Big", 3);

  pushOnStack(OBJ_VAL(structName));
  ObjStruct *struct_ = newStruct(structName);
  popFromStack();

  pushOnStack(OBJ_VAL(struct_));
  tableSet(&vm.globals, structName, OBJ_VAL(struct_));
  popFromStack();

  // Reaching this by executing 256 OP_FIELDs is not possible from a single
  // chunk -- OP_FIELD's operand is one byte.
  struct_->fieldCount = 256;

  ObjString *fieldName = copyString("overflow", 8);
  pushOnStack(OBJ_VAL(fieldName));
  ObjFunction *function = buildFieldChunk(structName, fieldName);
  popFromStack();

  InterpretResult result = interpretFunction(function);

  // Before the fix: runtimeError was called but execution continued, so the
  // opcode completed and this was not a runtime error.
  assert(result == INTERPRET_RUNTIME_ERROR);

  // Before the fix: tableSet ran ahead of the guard, leaving an entry that
  // points at slot 256.
  Value slot;
  assert(!tableGet(&struct_->fields, fieldName, &slot));

  assert(struct_->fieldCount == 256);

  freeVM();
}

int main(void) {
  test_field_limit_is_a_runtime_error();

  printf("vm_field_limit: ok\n");
  return 0;
}
