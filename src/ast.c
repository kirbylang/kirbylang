#include "ast.h"
#include "memory.h"
#include "object.h"
#include "strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLAB_SIZE 8192

static void printNode(StrBuf *sb, AstNode *node);

typedef struct Slab {
  struct Slab *next;
  size_t capacity;
  uint8_t data[];
} Slab;

static Slab *arenaHead = NULL;
static size_t arenaOffset = 0;

static Slab *allocSlab(size_t capacity) {
  Slab *slab = (Slab *)malloc(sizeof(Slab) + capacity);
  if (slab == NULL) {
    fprintf(stderr, "Out of memory allocating AST arena slab.\n");
    exit(1);
  }
  slab->next = arenaHead;
  slab->capacity = capacity;
  arenaHead = slab;
  arenaOffset = 0;
  return slab;
}

void *astAllocRaw(size_t size) {
  // Align to 8 bytes.
  size = (size + 7) & ~(size_t)7;

  if (arenaHead == NULL || arenaOffset + size > arenaHead->capacity) {
    // A request bigger than the normal slab size gets a slab sized exactly
    // to fit it, so it doesn't spill past the allocated buffer. That slab is
    // then "full" (arenaOffset will equal its capacity), so the next request
    // correctly falls through to a fresh normal-sized slab rather than
    // trying to pack more into the oversized one.
    size_t capacity = size > SLAB_SIZE ? size : SLAB_SIZE;
    allocSlab(capacity);
  }

  void *ptr = &arenaHead->data[arenaOffset];
  arenaOffset += size;
  return ptr;
}

void arrayNodeDataInit(ArrayNodeData *and) {
  and->count = 0;
  and->capacity = 0;
  and->data = 0;
}

void arrayNodeDataWrite(ArrayNodeData *and, AstNode *item) {
  if (and->capacity < and->count + 1) {
    int oldCapacity = and->capacity;
    and->capacity = GROW_CAPACITY(oldCapacity);
    and->data = GROW_ARRAY(AstNode *, and->data, oldCapacity, and->capacity);
  }

  and->data[and->count] = item;
  and->count++;
}

void arrayNodeDataFree(ArrayNodeData *and) {
  free(and->data);
  arrayNodeDataInit(and);
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

static void printAstValue(StrBuf *sb, Value value) {
  if (IS_BOOL(value)) {
    sb_append(sb, AS_BOOL(value) ? "true" : "false");
  } else if (IS_NIL(value)) {
    sb_append(sb, "nil");
  } else if (IS_NUMBER(value)) {
    sb_appendf(sb, "%g", AS_NUMBER(value));
  } else if (IS_STRING(value)) {
    sb_appendf(sb, "\"%s\"", AS_CSTRING(value));
  } else {
    sb_append(sb, "<value>");
  }
}

static void printBlock(StrBuf *sb, BlockNode *block) {
  sb_append(sb, "(block");
  for (int i = 0; i < block->count; i++) {
    sb_append(sb, " ");
    printNode(sb, block->stmts[i]);
  }
  if (block->value != NULL) {
    sb_append(sb, " (value ");
    printNode(sb, block->value);
    sb_append(sb, ")");
  }
  sb_append(sb, ")");
}

static void sbAppendToken(StrBuf *sb, Token t) {
  sb_ensure(sb, (size_t)t.length);
  memcpy(sb->data + sb->len, t.start, (size_t)t.length);
  sb->len += (size_t)t.length;
  sb->data[sb->len] = '\0';
}

static void printNode(StrBuf *sb, AstNode *node) {
  if (node == NULL) {
    sb_append(sb, "<null>");
    return;
  }

  switch (node->kind) {
  case NODE_LITERAL:
    printAstValue(sb, node->as.literal.value);
    break;

  case NODE_UNARY:
    sb_append(sb, "(");
    sbAppendToken(sb, node->as.unary.op);
    sb_append(sb, " ");
    printNode(sb, node->as.unary.operand);
    sb_append(sb, ")");
    break;

  case NODE_BINARY:
    sb_append(sb, "(");
    sbAppendToken(sb, node->as.binary.op);
    sb_append(sb, " ");
    printNode(sb, node->as.binary.left);
    sb_append(sb, " ");
    printNode(sb, node->as.binary.right);
    sb_append(sb, ")");
    break;

  case NODE_GROUPING:
    sb_append(sb, "(group ");
    printNode(sb, node->as.grouping.inner);
    sb_append(sb, ")");
    break;

  case NODE_VARIABLE:
    sbAppendToken(sb, node->as.variable.name);
    break;

  case NODE_ASSIGN:
    sb_append(sb, "(assign ");
    sbAppendToken(sb, node->as.assign.name);
    sb_append(sb, " ");
    printNode(sb, node->as.assign.value);
    sb_append(sb, ")");
    break;

  case NODE_AND:
    sb_append(sb, "(and ");
    printNode(sb, node->as.logical.left);
    sb_append(sb, " ");
    printNode(sb, node->as.logical.right);
    sb_append(sb, ")");
    break;

  case NODE_OR:
    sb_append(sb, "(or ");
    printNode(sb, node->as.logical.left);
    sb_append(sb, " ");
    printNode(sb, node->as.logical.right);
    sb_append(sb, ")");
    break;

  case NODE_NULLISH:
    sb_append(sb, "(?? ");
    printNode(sb, node->as.logical.left);
    sb_append(sb, " ");
    printNode(sb, node->as.logical.right);
    sb_append(sb, ")");
    break;

  case NODE_CALL:
    sb_append(sb, "(call ");
    printNode(sb, node->as.call.callee);
    for (int i = 0; i < node->as.call.argCount; i++) {
      sb_append(sb, " ");
      printNode(sb, node->as.call.args[i]);
    }
    sb_append(sb, ")");
    break;

  case NODE_GET:
    sb_append(sb, "(get ");
    printNode(sb, node->as.get.object);
    sb_append(sb, " ");
    sbAppendToken(sb, node->as.get.name);
    sb_append(sb, ")");
    break;

  case NODE_SET:
    sb_append(sb, "(set ");
    printNode(sb, node->as.set.object);
    sb_append(sb, " ");
    sbAppendToken(sb, node->as.set.name);
    sb_append(sb, " ");
    printNode(sb, node->as.set.value);
    sb_append(sb, ")");
    break;

  case NODE_INDEX_GET:
    sb_append(sb, "(index-get ");
    printNode(sb, node->as.indexGet.object);
    sb_append(sb, " ");
    printNode(sb, node->as.indexGet.index);
    sb_append(sb, ")");
    break;

  case NODE_INDEX_SET:
    sb_append(sb, "(index-set ");
    printNode(sb, node->as.indexSet.object);
    sb_append(sb, " ");
    printNode(sb, node->as.indexSet.index);
    sb_append(sb, " ");
    printNode(sb, node->as.indexSet.value);
    sb_append(sb, ")");
    break;

  case NODE_THIS:
    sb_append(sb, "this");
    break;

  case NODE_SUPER:
    sb_append(sb, "(super ");
    sbAppendToken(sb, node->as.super_.method);
    sb_append(sb, ")");
    break;

  case NODE_EXPR_STMT:
    printNode(sb, node->as.exprStmt.expr);
    break;

  case NODE_PRINT:
    sb_append(sb, "(print ");
    printNode(sb, node->as.print.expr);
    sb_append(sb, ")");
    break;

  case NODE_VAR_DECL:
    sb_append(sb, "(var ");
    sbAppendToken(sb, node->as.varDecl.name);
    if (node->as.varDecl.initializer != NULL) {
      sb_append(sb, " ");
      printNode(sb, node->as.varDecl.initializer);
    }
    sb_append(sb, ")");
    break;

  case NODE_BLOCK:
    printBlock(sb, &node->as.block);
    break;

  case NODE_IF:
    sb_append(sb, "(if ");
    printNode(sb, node->as.if_.condition);
    sb_append(sb, " ");
    printNode(sb, node->as.if_.thenBranch);
    if (node->as.if_.elseBranch != NULL) {
      sb_append(sb, " ");
      printNode(sb, node->as.if_.elseBranch);
    }
    sb_append(sb, ")");
    break;

  case NODE_WHILE:
    sb_append(sb, "(while ");
    printNode(sb, node->as.while_.condition);
    sb_append(sb, " ");
    printNode(sb, node->as.while_.body);
    sb_append(sb, ")");
    break;

  case NODE_RETURN:
    sb_append(sb, "(return");
    if (node->as.return_.value != NULL) {
      sb_append(sb, " ");
      printNode(sb, node->as.return_.value);
    }
    sb_append(sb, ")");
    break;

  case NODE_FUNCTION:
    sb_append(sb, node->as.function.isLambda ? "(lambda (" : "(fun ");
    if (!node->as.function.isLambda) {
      sbAppendToken(sb, node->as.function.name);
      sb_append(sb, " (");
    }
    for (int i = 0; i < node->as.function.arity; i++) {
      if (i > 0)
        sb_append(sb, " ");
      sbAppendToken(sb, node->as.function.params[i]);
    }
    sb_append(sb, ") ");
    if (node->as.function.exprBody != NULL) {
      printNode(sb, node->as.function.exprBody);
    } else {
      printBlock(sb, &node->as.function.body);
    }
    sb_append(sb, ")");
    break;

  case NODE_CLASS:
    sb_append(sb, "(class ");
    sbAppendToken(sb, node->as.class_.name);
    if (node->as.class_.superclass != NULL) {
      sb_append(sb, " < ");
      sbAppendToken(sb, *node->as.class_.superclass);
    }
    for (int i = 0; i < node->as.class_.memberCount; i++) {
      ClassMember *m = &node->as.class_.members[i];
      if (m->kind == CLASS_MEMBER_METHOD) {
        sb_append(sb, " (method ");
        sbAppendToken(sb, m->as.method->name);
        sb_append(sb, ")");
      } else {
        sb_append(sb, " (field ");
        sbAppendToken(sb, m->as.field.name);
        if (m->as.field.initializer != NULL) {
          sb_append(sb, " ");
          printNode(sb, m->as.field.initializer);
        }
        sb_append(sb, ")");
      }
    }
    sb_append(sb, ")");
    break;

  case NODE_ARRAY:
    sb_append(sb, "(array");
    for (int i = 0; i < node->as.array.count; i++) {
      sb_append(sb, " ");
      printNode(sb, node->as.array.items[i]);
    }
    sb_append(sb, ")");
    break;

  case NODE_COUNT:
    sb_append(sb, "<invalid>");
    break;

  case NODE_BREAK:
    sb_append(sb, "(break)");
    break;
  }
}

const char *print_ast(AstNode *ast) {
  StrBuf sb;
  sb_init(&sb);
  printNode(&sb, ast);
  return sb.data; // TODO: caller owns this heap string — must free() it
}
