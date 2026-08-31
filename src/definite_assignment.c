#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definite_assignment.h"
#include "typecheck.h"

static bool tokensEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

void pendingSetInit(PendingSet *set) {
  set->names = NULL;
  set->count = 0;
  set->capacity = 0;
}

void pendingSetFree(PendingSet *set) {
  free(set->names);
  set->names = NULL;
  set->count = 0;
  set->capacity = 0;
}

static void pendingSetAdd(PendingSet *set, Token name) {
  if (set->capacity < set->count + 1) {
    set->capacity = set->capacity < 8 ? 8 : set->capacity * 2;
    set->names = (Token *)realloc(set->names, sizeof(Token) * set->capacity);

    if (set->names == NULL) {
      fprintf(stderr, "realloc failed in pendingSetAdd\n");
      exit(1);
    }
  }

  set->names[set->count++] = name;
}

static bool pendingSetContains(PendingSet *set, Token name) {
  for (int i = 0; i < set->count; i++) {
    if (tokensEqual(&set->names[i], &name))
      return true;
  }

  return false;
}

static void pendingSetRemove(PendingSet *set, Token name) {
  for (int i = 0; i < set->count; i++) {
    if (tokensEqual(&set->names[i], &name)) {
      set->names[i] = set->names[set->count - 1];
      set->count--;
      return;
    }
  }
}

static PendingSet pendingSetClone(PendingSet *set) {
  PendingSet clone;
  pendingSetInit(&clone);
  for (int i = 0; i < set->count; i++) {
    pendingSetAdd(&clone, set->names[i]);
  }
  return clone;
}

// Adds every name in `from` not already present in `into`.
static void pendingSetUnionInto(PendingSet *into, PendingSet *from) {
  for (int i = 0; i < from->count; i++) {
    if (!pendingSetContains(into, from->names[i])) {
      pendingSetAdd(into, from->names[i]);
    }
  }
}

static void checkAssignmentExpr(PendingSet *pending, AstNode *node);

static bool checkAssignmentBlock(PendingSet *pending, BlockNode *block) {
  for (int i = 0; i < block->count; i++) {
    if (checkAssignmentStmt(pending, block->stmts[i])) {
      return true; // rest of the block is unreachable
    }
  }
  if (block->value != NULL) {
    checkAssignmentExpr(pending, block->value);
  }
  return false;
}

static void checkAssignmentExpr(PendingSet *pending, AstNode *node) {
  if (node == NULL)
    return;

  switch (node->kind) {
  case NODE_VARIABLE: {
    Token *name = &node->as.variable.name;

    if (pendingSetContains(pending, *name)) {
      errorAtTokenFmt(name, "'%.*s' might not be assigned yet.", name->length,
                      name->start);
    }

    break;
  }
  case NODE_ASSIGN: {
    AssignNode *assign = &node->as.assign;

    checkAssignmentExpr(pending, assign->value);
    pendingSetRemove(pending, assign->name);

    break;
  }
  case NODE_UNARY:
    checkAssignmentExpr(pending, node->as.unary.operand);

    break;
  case NODE_BINARY:
    checkAssignmentExpr(pending, node->as.binary.left);
    checkAssignmentExpr(pending, node->as.binary.right);

    break;
  case NODE_GROUPING:
    checkAssignmentExpr(pending, node->as.grouping.inner);

    break;
  case NODE_AND:
  case NODE_OR:
  case NODE_NULLISH:
    checkAssignmentExpr(pending, node->as.logical.left);
    checkAssignmentExpr(pending, node->as.logical.right);

    break;
  case NODE_CALL: {
    CallNode *call = &node->as.call;
    checkAssignmentExpr(pending, call->callee);

    for (int i = 0; i < call->argCount; i++)
      checkAssignmentExpr(pending, call->args[i]);

    break;
  }
  case NODE_GET:
    checkAssignmentExpr(pending, node->as.get.object);

    break;
  case NODE_SET: {
    SetNode *set = &node->as.set;

    checkAssignmentExpr(pending, set->object);
    checkAssignmentExpr(pending, set->value);

    break;
  }
  case NODE_INDEX_GET: {
    IndexGetNode *indexGet = &node->as.indexGet;

    checkAssignmentExpr(pending, indexGet->object);
    checkAssignmentExpr(pending, indexGet->index);

    break;
  }
  case NODE_INDEX_SET: {
    IndexSetNode *indexSet = &node->as.indexSet;

    checkAssignmentExpr(pending, indexSet->object);
    checkAssignmentExpr(pending, indexSet->index);
    checkAssignmentExpr(pending, indexSet->value);

    break;
  }
  case NODE_STRUCT_INIT: {
    StructInitNode *structInit = &node->as.structInit;

    for (int i = 0; i < structInit->fieldCount; i++)
      checkAssignmentExpr(pending, structInit->fields[i].value);

    break;
  }
  case NODE_ARRAY: {
    ArrayNode *array = &node->as.array;

    for (int i = 0; i < array->count; i++)
      checkAssignmentExpr(pending, array->items[i]);

    break;
  }
  case NODE_IF:
  case NODE_BLOCK:
    checkAssignmentStmt(pending, node);
    break;
  case NODE_FUNCTION:
    break;
  case NODE_LITERAL:
  case NODE_SELF:
    break;
  default:
    break;
  }
}

bool checkAssignmentStmt(PendingSet *pending, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    checkAssignmentExpr(pending, node->as.exprStmt.expr);
    return false;

  case NODE_PRINT:
    checkAssignmentExpr(pending, node->as.print.expr);
    return false;

  case NODE_VAR_DECL: {
    VarDeclNode *varDecl = &node->as.varDecl;

    if (varDecl->initializer != NULL) {
      checkAssignmentExpr(pending, varDecl->initializer);
    } else {
      pendingSetAdd(pending, varDecl->name);
    }

    return false;
  }

  case NODE_WHILE: {
    WhileNode *while_ = &node->as.while_;
    checkAssignmentExpr(pending, while_->condition);
    // The body might run zero times -- whatever it assigns isn't
    // definite afterward, so it's checked against a pending set clone
    PendingSet bodyPending = pendingSetClone(pending);
    checkAssignmentStmt(&bodyPending, while_->body);
    pendingSetFree(&bodyPending);

    return false;
  }

  case NODE_FOR: {
    ForNode *for_ = &node->as.for_;

    if (for_->init != NULL)
      checkAssignmentStmt(pending, for_->init); // always runs once, for real

    if (for_->condition != NULL)
      checkAssignmentExpr(pending, for_->condition); // always evaluated once

    PendingSet bodyPending = pendingSetClone(pending);
    checkAssignmentStmt(&bodyPending, for_->body);

    if (for_->increment != NULL)
      checkAssignmentExpr(&bodyPending, for_->increment);

    pendingSetFree(&bodyPending);

    return false;
  }

  case NODE_RETURN: {
    ReturnNode *return_ = &node->as.return_;
    if (return_->value != NULL)
      checkAssignmentExpr(pending, return_->value);
    return true;
  }

  case NODE_BREAK:
  case NODE_CONTINUE:
    return true;

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    checkAssignmentExpr(pending, if_->condition);

    PendingSet thenPending = pendingSetClone(pending);
    bool thenEscapes = checkAssignmentStmt(&thenPending, if_->thenBranch);

    PendingSet elsePending = pendingSetClone(pending);

    bool elseEscapes = false;
    if (if_->elseBranch != NULL) {
      elseEscapes = checkAssignmentStmt(&elsePending, if_->elseBranch);
    }

    if (thenEscapes && elseEscapes) {
      pendingSetFree(&thenPending);
      pendingSetFree(&elsePending);
      return true;
    }

    if (thenEscapes) {
      pendingSetFree(pending);
      *pending = elsePending;
      pendingSetFree(&thenPending);
      return false;
    }

    if (elseEscapes) {
      pendingSetFree(pending);
      *pending = thenPending;
      pendingSetFree(&elsePending);
      return false;
    }

    pendingSetFree(pending);
    pendingSetInit(pending);
    pendingSetUnionInto(pending, &thenPending);
    pendingSetUnionInto(pending, &elsePending);
    pendingSetFree(&thenPending);
    pendingSetFree(&elsePending);

    return false;
  }

  case NODE_BLOCK:
    return checkAssignmentBlock(pending, &node->as.block);

  case NODE_FUNCTION:
    return false;

  default:
    checkAssignmentExpr(pending, node);
    return false;
  }
}

void checkDefiniteAssignment(FunctionNode *fn) {
  if (fn->exprBody != NULL)
    return;

  PendingSet pending;
  pendingSetInit(&pending);
  checkAssignmentBlock(&pending, &fn->body);
  pendingSetFree(&pending);
}
