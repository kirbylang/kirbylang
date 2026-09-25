#include <assert.h>
#include <stdio.h>

#include "../src/opcode.h"

int main(void) {
  for (int op = 0; op < OP_COUNT; op++) {
    const OpInfo *info = opInfo((OpCode)op);

    if (!info->isKnownOp) {
      fprintf(stderr, "opcode %d has no entry in opInfos\n", op);
    }
    assert(info->isKnownOp);
  }

  assert(opInfo(OP_COUNT) == NULL);

  printf("opcode tests passed\n");
  return 0;
}
