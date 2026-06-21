#include "ast.h"
#include <string.h>

#define SLAB_SIZE 8192

typedef struct Slab {
  struct Slab *next;
  uint8_t data[SLAB_SIZE];
} Slab;

static Slab *arenaHead = NULL;
static size_t arenaOffset = 0;

void *astAllocRaw(size_t size) {
  // Align size to 8 bytes for alignment
  size = (size + 7) & ~(size_t)7;

  if (arenaHead == NULL || arenaOffset + size > SLAB_SIZE) {
    Slab *slab = (Slab *)malloc(sizeof(Slab));
    slab->next = arenaHead;
    arenaHead = slab;
    arenaOffset = 0;
  }

  void *ptr = &arenaHead->data[arenaOffset];
  arenaOffset += size;
  return ptr;
}

AstNode *astAlloc(NodeKind kind, int line) {
  AstNode *node = (AstNode *)astAllocRaw(sizeof(AstNode));
  memset(node, 0, sizeof(AstNode));
  node->kind = kind;
  node->line = line;
  return node;
}

void astFreeAll(void) {
  Slab *s = arenaHead;
  while (s != NULL) {
    Slab *next = s->next;
    free(s);
    s = next;
  }
  arenaHead = NULL;
  arenaOffset = 0;
}
