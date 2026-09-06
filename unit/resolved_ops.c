#include <assert.h>

#include "../src/ast.h"
#include "../src/resolved_ops.h"

static void test_lookup_on_unrecorded_node_returns_null(void) {
  resolvedOpsReset();

  AstNode *node = astAlloc(NODE_CALL, 1);
  assert(resolvedOpsLookup(node) == NULL);

  astFreeAll();
}

static void test_record_then_lookup_returns_same_op(void) {
  resolvedOpsReset();

  AstNode *node = astAlloc(NODE_CALL, 1);

  ResolvedOp op;
  op.kind = RESOLVED_OP_PRIMITIVE_CALL;
  op.hasSelf = true;
  op.mangledName = "@f64.Display.toString";
  op.mangledLength = 22;

  resolvedOpsRecord(node, op);

  const ResolvedOp *found = resolvedOpsLookup(node);
  assert(found != NULL);
  assert(found->kind == RESOLVED_OP_PRIMITIVE_CALL);
  assert(found->hasSelf);
  assert(found->mangledName == op.mangledName); // same pointer, not a copy
  assert(found->mangledLength == 22);

  astFreeAll();
}

static void test_different_nodes_dont_collide(void) {
  resolvedOpsReset();

  AstNode *nodeA = astAlloc(NODE_CALL, 1);
  AstNode *nodeB = astAlloc(NODE_CALL, 2);

  ResolvedOp opA;
  opA.kind = RESOLVED_OP_PRIMITIVE_CALL;
  opA.hasSelf = true;
  opA.mangledName = "@f64.helper";
  opA.mangledLength = 11;

  resolvedOpsRecord(nodeA, opA);

  // nodeB was never recorded -- must stay unresolved even though nodeA,
  // an entirely different node, has an entry.
  assert(resolvedOpsLookup(nodeB) == NULL);
  assert(resolvedOpsLookup(nodeA) != NULL);

  astFreeAll();
}

static void test_multiple_entries_looked_up_independently(void) {
  resolvedOpsReset();

  AstNode *nodeA = astAlloc(NODE_CALL, 1);
  AstNode *nodeB = astAlloc(NODE_CALL, 2);

  ResolvedOp opA;
  opA.kind = RESOLVED_OP_PRIMITIVE_CALL;
  opA.hasSelf = true;
  opA.mangledName = "@f64.Display.toString";
  opA.mangledLength = 22;

  ResolvedOp opB;
  opB.kind = RESOLVED_OP_PRIMITIVE_CALL;
  opB.hasSelf = false;
  opB.mangledName = "@f64.Default.default";
  opB.mangledLength = 21;

  resolvedOpsRecord(nodeA, opA);
  resolvedOpsRecord(nodeB, opB);

  const ResolvedOp *foundA = resolvedOpsLookup(nodeA);
  const ResolvedOp *foundB = resolvedOpsLookup(nodeB);

  assert(foundA->hasSelf);
  assert(!foundB->hasSelf);
  assert(foundA->mangledName == opA.mangledName);
  assert(foundB->mangledName == opB.mangledName);

  astFreeAll();
}

static void test_reset_clears_all_entries(void) {
  resolvedOpsReset();

  AstNode *node = astAlloc(NODE_CALL, 1);

  ResolvedOp op;
  op.kind = RESOLVED_OP_PRIMITIVE_CALL;
  op.hasSelf = true;
  op.mangledName = "@f64.helper";
  op.mangledLength = 11;

  resolvedOpsRecord(node, op);
  assert(resolvedOpsLookup(node) != NULL);

  resolvedOpsReset();

  // Same pointer -- a fresh AST could reuse this exact address, so a
  // stale entry surviving reset would silently misattribute it.
  assert(resolvedOpsLookup(node) == NULL);

  astFreeAll();
}

int main(void) {
  test_lookup_on_unrecorded_node_returns_null();
  test_record_then_lookup_returns_same_op();
  test_different_nodes_dont_collide();
  test_multiple_entries_looked_up_independently();
  test_reset_clears_all_entries();

  resolvedOpsReset();

  return 0;
}
