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
    AssignNode *a = &node->as.assign;
    // RHS evaluated before the assignment takes effect, so `x = x + 1`
    // still catches a read of a not-yet-assigned x.
    checkAssignmentExpr(pending, a->value);
    pendingSetRemove(pending, a->name);
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
    CallNode *c = &node->as.call;
    checkAssignmentExpr(pending, c->callee);
    for (int i = 0; i < c->argCount; i++)
      checkAssignmentExpr(pending, c->args[i]);
    break;
  }
  case NODE_GET:
    checkAssignmentExpr(pending, node->as.get.object);
    break;
  case NODE_SET: {
    SetNode *s = &node->as.set;
    checkAssignmentExpr(pending, s->object);
    checkAssignmentExpr(pending, s->value);
    break;
  }
  case NODE_INDEX_GET: {
    IndexGetNode *ig = &node->as.indexGet;
    checkAssignmentExpr(pending, ig->object);
    checkAssignmentExpr(pending, ig->index);
    break;
  }
  case NODE_INDEX_SET: {
    IndexSetNode *is = &node->as.indexSet;
    checkAssignmentExpr(pending, is->object);
    checkAssignmentExpr(pending, is->index);
    checkAssignmentExpr(pending, is->value);
    break;
  }
  case NODE_STRUCT_INIT: {
    StructInitNode *si = &node->as.structInit;
    for (int i = 0; i < si->fieldCount; i++)
      checkAssignmentExpr(pending, si->fields[i].value);
    break;
  }
  case NODE_ARRAY: {
    ArrayNode *a = &node->as.array;
    for (int i = 0; i < a->count; i++)
      checkAssignmentExpr(pending, a->items[i]);
    break;
  }
  case NODE_IF:
  case NODE_BLOCK:
    checkAssignmentStmt(pending, node); // same merge/block logic either way
    break;
  case NODE_FUNCTION:
    // A lambda literal -- analyzed independently with its own fresh
    // pending set, if/when it's checked as a function body.
    break;
  case NODE_LITERAL:
  case NODE_SELF:
    break; // no sub-expressions
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
    VarDeclNode *vd = &node->as.varDecl;
    if (vd->initializer != NULL) {
      checkAssignmentExpr(pending, vd->initializer);
      // Has an initializer -- never pending, nothing to track.
    } else {
      pendingSetAdd(pending, vd->name);
    }
    return false;
  }

  case NODE_WHILE: {
    WhileNode *w = &node->as.while_;
    checkAssignmentExpr(pending, w->condition);
    // The body might run zero times -- whatever it assigns isn't
    // definite afterward, so it's checked (for unsafe reads inside it)
    // against a throwaway clone, never `pending` itself.
    PendingSet bodyPending = pendingSetClone(pending);
    checkAssignmentStmt(&bodyPending, w->body);
    pendingSetFree(&bodyPending);
    return false;
  }

  case NODE_FOR: {
    ForNode *f = &node->as.for_;
    if (f->init != NULL)
      checkAssignmentStmt(pending, f->init); // always runs once, for real
    if (f->condition != NULL)
      checkAssignmentExpr(pending, f->condition); // always evaluated once
    PendingSet bodyPending = pendingSetClone(pending);
    checkAssignmentStmt(&bodyPending, f->body);
    if (f->increment != NULL)
      checkAssignmentExpr(&bodyPending, f->increment);
    pendingSetFree(&bodyPending);
    return false;
  }

  case NODE_RETURN: {
    ReturnNode *r = &node->as.return_;
    if (r->value != NULL)
      checkAssignmentExpr(pending, r->value);
    return true;
  }

  case NODE_BREAK:
  case NODE_CONTINUE:
    return true;

  case NODE_IF: {
    IfNode *i = &node->as.if_;
    checkAssignmentExpr(pending, i->condition);

    PendingSet thenPending = pendingSetClone(pending);
    bool thenEscapes = checkAssignmentStmt(&thenPending, i->thenBranch);

    PendingSet elsePending = pendingSetClone(pending);
    bool elseEscapes = false;
    if (i->elseBranch != NULL) {
      elseEscapes = checkAssignmentStmt(&elsePending, i->elseBranch);
    }
    // No else -- elsePending stays a clone of pre-if `pending` (nothing
    // happens when the condition is false), elseEscapes stays false.

    if (thenEscapes && elseEscapes) {
      // Nothing after this if is reachable through either branch.
      pendingSetFree(&thenPending);
      pendingSetFree(&elsePending);
      return true;
    }
    if (thenEscapes) {
      // Only reachable via else.
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
    // Neither branch escapes -- merged result is the union: a binding is
    // still pending after the if unless it was assigned on *both* paths.
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
    // A nested function declaration -- analyzed independently, with its
    // own fresh pending set, when checkFunctionBody() checks it.
    return false;

  default:
    // Anything expression-shaped reached directly as a statement.
    checkAssignmentExpr(pending, node);
    return false;
  }
}

void checkDefiniteAssignment(FunctionNode *fn) {
  // Expression-bodied functions (`fun f() = expr;`) can't declare a
  // var/let at all, so there's nothing to check there.
  if (fn->exprBody != NULL)
    return;

  PendingSet pending;
  pendingSetInit(&pending);
  checkAssignmentBlock(&pending, &fn->body);
  pendingSetFree(&pending);
}
