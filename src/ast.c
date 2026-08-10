#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "memory.h"
#include "object.h"
#include "strbuf.h"

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

static void printAstLiteral(StrBuf *sb, LiteralNode *lit) {
  switch (lit->kind) {
  case LITERAL_NIL:
    sb_append(sb, "nil");
    break;
  case LITERAL_BOOL:
    sb_append(sb, lit->as.boolean ? "true" : "false");
    break;
  case LITERAL_NUMBER:
    sb_appendf(sb, "%g", lit->as.number);
    break;
  case LITERAL_STRING:
    sb_appendf(sb, "\"%s\"", lit->as.string.chars);
    break;
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
    printAstLiteral(sb, &node->as.literal);
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

  case NODE_SELF:
    sb_append(sb, "self");
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
    sb_append(sb, node->as.varDecl.isMutable ? "(var " : "(let ");
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

  case NODE_FOR:
    sb_append(sb, "(for ");
    printNode(sb, node->as.for_.init);
    sb_append(sb, " ");
    printNode(sb, node->as.for_.condition);
    sb_append(sb, " ");
    printNode(sb, node->as.for_.increment);
    sb_append(sb, " ");
    printNode(sb, node->as.for_.body);
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

    if (node->as.function.hasSelf) {
      sb_append(sb, "self");
      if (node->as.function.arity > 0)
        sb_append(sb, " ");
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

  case NODE_STRUCT:
    sb_append(sb, "(struct ");
    sbAppendToken(sb, node->as.struct_.name);

    for (int i = 0; i < node->as.struct_.fieldCount; i++) {
      VarDeclNode *field = &node->as.struct_.fields[i];

      sb_append(sb, field->isPublic ? " (pub-field " : " (field ");
      sbAppendToken(sb, field->name);

      if (field->initializer != NULL) {
        sb_append(sb, " ");
        printNode(sb, field->initializer);
      }

      sb_append(sb, ")");
    }

    sb_append(sb, ")");
    break;

  case NODE_STRUCT_INIT:
    sb_append(sb, "(struct-init ");
    sbAppendToken(sb, node->as.structInit.name);

    for (int i = 0; i < node->as.structInit.fieldCount; i++) {
      StructInitFieldNode *field = &node->as.structInit.fields[i];

      sb_append(sb, " (field ");
      sbAppendToken(sb, field->name);
      sb_append(sb, " ");
      printNode(sb, field->value);
      sb_append(sb, ")");
    }

    sb_append(sb, ")");
    break;

  case NODE_IMPL:
    sb_append(sb, "(impl ");
    sbAppendToken(sb, node->as.impl.name);

    for (int i = 0; i < node->as.impl.methodCount; i++) {
      FunctionNode *method = node->as.impl.methods[i];

      if (method->isPublic) {
        sb_append(sb, method->hasSelf ? " (pub-method " : " (pub-static ");
      } else {
        sb_append(sb, method->hasSelf ? " (method " : " (static ");
      }

      sbAppendToken(sb, method->name);
      sb_append(sb, ")");
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

void print_ast(StrBuf *sb, AstNode *ast) { printNode(sb, ast); }
