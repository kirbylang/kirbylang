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

void daaSetInit(DaaSet *daa) {
  daa->names = NULL;
  daa->count = 0;
  daa->capacity = 0;
}

void daaSetFree(DaaSet *daa) {
  free(daa->names);
  daa->names = NULL;
  daa->count = 0;
  daa->capacity = 0;
}

static void daaSetAdd(DaaSet *daa, Token name) {
  if (daa->capacity < daa->count + 1) {
    daa->capacity = daa->capacity < 8 ? 8 : daa->capacity * 2;
    daa->names = (Token *)realloc(daa->names, sizeof(Token) * daa->capacity);

    if (daa->names == NULL) {
      fprintf(stderr, "realloc failed in daaSetAdd\n");
      exit(1);
    }
  }

  daa->names[daa->count++] = name;
}

static bool daaSetContains(DaaSet *daa, Token name) {
  for (int i = 0; i < daa->count; i++) {
    if (tokensEqual(&daa->names[i], &name))
      return true;
  }

  return false;
}

static void daaSetRemove(DaaSet *daa, Token name) {
  for (int i = 0; i < daa->count; i++) {
    if (tokensEqual(&daa->names[i], &name)) {
      daa->names[i] = daa->names[daa->count - 1];
      daa->count--;
      return;
    }
  }
}

static DaaSet daaSetClone(DaaSet *daa) {
  DaaSet clone;
  daaSetInit(&clone);
  for (int i = 0; i < daa->count; i++) {
    daaSetAdd(&clone, daa->names[i]);
  }
  return clone;
}

// Adds every name in `daaFrom` not already present in `daaInto`.
static void daaSetUnionInto(DaaSet *daaInto, DaaSet *daaFrom) {
  for (int i = 0; i < daaFrom->count; i++) {
    if (!daaSetContains(daaInto, daaFrom->names[i])) {
      daaSetAdd(daaInto, daaFrom->names[i]);
    }
  }
}

static void daaCheckAssignmentExpr(DaaSet *daa, AstNode *node);

static bool daaCheckAssignmentBlock(DaaSet *daa, BlockNode *block) {
  for (int i = 0; i < block->count; i++) {
    if (daaCheckAssignmentStmt(daa, block->stmts[i])) {
      return true; // rest of the block is unreachable
    }
  }

  if (block->value != NULL) {
    daaCheckAssignmentExpr(daa, block->value);
  }

  return false;
}

static void daaCheckAssignmentExpr(DaaSet *daa, AstNode *node) {
  if (node == NULL)
    return;

  switch (node->kind) {
  case NODE_VARIABLE: {
    Token *name = &node->as.variable.name;

    if (daaSetContains(daa, *name)) {
      typchkErrorAtTokenFmt(name, "'%.*s' might not be assigned yet.",
                            name->length, name->start);
    }

    break;
  }
  case NODE_ASSIGN: {
    AssignNode *assign = &node->as.assign;

    daaCheckAssignmentExpr(daa, assign->value);
    daaSetRemove(daa, assign->name);

    break;
  }
  case NODE_UNARY:
    daaCheckAssignmentExpr(daa, node->as.unary.operand);

    break;
  case NODE_BINARY:
    daaCheckAssignmentExpr(daa, node->as.binary.left);
    daaCheckAssignmentExpr(daa, node->as.binary.right);

    break;
  case NODE_GROUPING:
    daaCheckAssignmentExpr(daa, node->as.grouping.inner);

    break;
  case NODE_AND:
  case NODE_OR:
  case NODE_NULLISH:
    daaCheckAssignmentExpr(daa, node->as.logical.left);
    daaCheckAssignmentExpr(daa, node->as.logical.right);

    break;
  case NODE_CALL: {
    CallNode *call = &node->as.call;
    daaCheckAssignmentExpr(daa, call->callee);

    for (int i = 0; i < call->argCount; i++)
      daaCheckAssignmentExpr(daa, call->args[i]);

    break;
  }
  case NODE_GET:
    daaCheckAssignmentExpr(daa, node->as.get.object);

    break;
  case NODE_SET: {
    SetNode *set = &node->as.set;

    daaCheckAssignmentExpr(daa, set->object);
    daaCheckAssignmentExpr(daa, set->value);

    break;
  }
  case NODE_INDEX_GET: {
    IndexGetNode *indexGet = &node->as.indexGet;

    daaCheckAssignmentExpr(daa, indexGet->object);
    daaCheckAssignmentExpr(daa, indexGet->index);

    break;
  }
  case NODE_INDEX_SET: {
    IndexSetNode *indexSet = &node->as.indexSet;

    daaCheckAssignmentExpr(daa, indexSet->object);
    daaCheckAssignmentExpr(daa, indexSet->index);
    daaCheckAssignmentExpr(daa, indexSet->value);

    break;
  }
  case NODE_STRUCT_INIT: {
    StructInitNode *structInit = &node->as.structInit;

    for (int i = 0; i < structInit->fieldCount; i++)
      daaCheckAssignmentExpr(daa, structInit->fields[i].value);

    break;
  }
  case NODE_ARRAY: {
    ArrayNode *array = &node->as.array;

    for (int i = 0; i < array->count; i++)
      daaCheckAssignmentExpr(daa, array->items[i]);

    break;
  }
  case NODE_IF:
  case NODE_BLOCK:
    daaCheckAssignmentStmt(daa, node);
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

bool daaCheckAssignmentStmt(DaaSet *daa, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    daaCheckAssignmentExpr(daa, node->as.exprStmt.expr);
    return false;

  case NODE_PRINT:
    daaCheckAssignmentExpr(daa, node->as.print.expr);
    return false;

  case NODE_VAR_DECL: {
    VarDeclNode *varDecl = &node->as.varDecl;

    if (varDecl->initializer != NULL) {
      daaCheckAssignmentExpr(daa, varDecl->initializer);
    } else {
      daaSetAdd(daa, varDecl->name);
    }

    return false;
  }

  case NODE_WHILE: {
    WhileNode *while_ = &node->as.while_;
    daaCheckAssignmentExpr(daa, while_->condition);
    // The body might run zero times -- whatever it assigns isn't
    // definite afterward, so it's checked against a daa set clone
    DaaSet bodyDaa = daaSetClone(daa);
    daaCheckAssignmentStmt(&bodyDaa, while_->body);
    daaSetFree(&bodyDaa);

    return false;
  }

  case NODE_FOR: {
    ForNode *for_ = &node->as.for_;

    if (for_->init != NULL)
      daaCheckAssignmentStmt(daa, for_->init); // always runs once, for real

    if (for_->condition != NULL)
      daaCheckAssignmentExpr(daa, for_->condition); // always evaluated once

    DaaSet bodyDaa = daaSetClone(daa);
    daaCheckAssignmentStmt(&bodyDaa, for_->body);

    if (for_->increment != NULL)
      daaCheckAssignmentExpr(&bodyDaa, for_->increment);

    daaSetFree(&bodyDaa);

    return false;
  }

  case NODE_RETURN: {
    ReturnNode *return_ = &node->as.return_;
    if (return_->value != NULL)
      daaCheckAssignmentExpr(daa, return_->value);
    return true;
  }

  case NODE_BREAK:
  case NODE_CONTINUE:
    return true;

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    daaCheckAssignmentExpr(daa, if_->condition);

    DaaSet thenDaa = daaSetClone(daa);
    bool thenEscapes = daaCheckAssignmentStmt(&thenDaa, if_->thenBranch);

    DaaSet elseDaa = daaSetClone(daa);
    bool elseEscapes = false;
    if (if_->elseBranch != NULL) {
      elseEscapes = daaCheckAssignmentStmt(&elseDaa, if_->elseBranch);
    }

    if (thenEscapes && elseEscapes) {
      daaSetFree(&thenDaa);
      daaSetFree(&elseDaa);
      return true;
    }

    if (thenEscapes) {
      daaSetFree(daa);
      *daa = elseDaa;
      daaSetFree(&thenDaa);
      return false;
    }

    if (elseEscapes) {
      daaSetFree(daa);
      *daa = thenDaa;
      daaSetFree(&elseDaa);
      return false;
    }

    daaSetFree(daa);
    daaSetInit(daa);
    daaSetUnionInto(daa, &thenDaa);
    daaSetUnionInto(daa, &elseDaa);
    daaSetFree(&thenDaa);
    daaSetFree(&elseDaa);

    return false;
  }

  case NODE_BLOCK:
    return daaCheckAssignmentBlock(daa, &node->as.block);

  case NODE_FUNCTION:
    return false;

  default:
    daaCheckAssignmentExpr(daa, node);
    return false;
  }
}

void daaCheckFn(FunctionNode *fn) {
  if (fn->exprBody != NULL)
    return;

  DaaSet daa;
  daaSetInit(&daa);
  daaCheckAssignmentBlock(&daa, &fn->body);
  daaSetFree(&daa);
}
