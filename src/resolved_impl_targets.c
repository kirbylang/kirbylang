#include <stdio.h>
#include <stdlib.h>

#include "resolved_impl_targets.h"

typedef struct {
  AstNode *node;
  InternedName structName;
} ResolvedImplTargetEntry;

static ResolvedImplTargetEntry *entries = NULL;
static int entryCount = 0;
static int entryCapacity = 0;

void resolvedImplTargetsReset(void) {
  free(entries);
  entries = NULL;
  entryCount = 0;
  entryCapacity = 0;
}

void resolvedImplTargetsRecord(AstNode *implNode, InternedName structName) {
  if (entryCount + 1 > entryCapacity) {
    entryCapacity = entryCapacity < 8 ? 8 : entryCapacity * 2;
    entries = (ResolvedImplTargetEntry *)realloc(
        entries, (size_t)entryCapacity * sizeof(ResolvedImplTargetEntry));

    if (entries == NULL) {
      fprintf(stderr, "realloc failed in resolvedImplTargetsRecord\n");
      exit(1);
    }
  }

  entries[entryCount].node = implNode;
  entries[entryCount].structName = structName;
  entryCount++;
}

const Token *resolvedImplTargetsLookup(AstNode *implNode) {
  // Rebuilt on every call, from the arena's current base pointer -- not
  // cached from record time, when a later realloc could have moved it.
  static Token resolved;

  for (int i = 0; i < entryCount; i++) {
    if (entries[i].node == implNode) {
      resolved.type = TOKEN_IDENTIFIER;
      resolved.start = internedNameChars(entries[i].structName);
      resolved.length = entries[i].structName.length;
      resolved.line = 0;
      return &resolved;
    }
  }

  return NULL;
}
