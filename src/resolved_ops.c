#include <stdio.h>
#include <stdlib.h>

#include "resolved_ops.h"

typedef struct {
  AstNode *node;
  ResolvedOp op;
} ResolvedOpEntry;

static ResolvedOpEntry *entries = NULL;
static int entryCount = 0;
static int entryCapacity = 0;

void resolvedOpsReset(void) {
  free(entries);
  entries = NULL;
  entryCount = 0;
  entryCapacity = 0;
}

void resolvedOpsRecord(AstNode *node, ResolvedOp op) {
  if (entryCount + 1 > entryCapacity) {
    entryCapacity = entryCapacity < 8 ? 8 : entryCapacity * 2;
    entries = (ResolvedOpEntry *)realloc(entries, (size_t)entryCapacity *
                                                      sizeof(ResolvedOpEntry));
    if (entries == NULL) {
      fprintf(stderr, "realloc failed in resolvedOpsRecord\n");
      exit(1);
    }
  }

  entries[entryCount].node = node;
  entries[entryCount].op = op;
  entryCount++;
}

const ResolvedOp *resolvedOpsLookup(AstNode *node) {
  // Entry count stays small in practice (one per statically-resolved
  // primitive method call in a single compilation unit), so a linear
  // scan is simpler than a hash table and plenty fast.
  for (int i = 0; i < entryCount; i++) {
    if (entries[i].node == node)
      return &entries[i].op;
  }
  return NULL;
}
