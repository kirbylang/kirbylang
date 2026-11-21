#include <stdio.h>

#include "stringset.h"
#include "top_level.h"

/* Names a comptime initializer may refer to. */
typedef struct {
  StringSet funs; // every top-level `fun`, wherever it is declared
  StringSet lets; // comptime `let`s declared above the current declaration
} ComptimeNames;

static bool isDeclaration(const AstNode *node) {
  switch (node->kind) {
  case NODE_FUNCTION:
  case NODE_STRUCT:
  case NODE_IMPL:
  case NODE_TRAIT:
  case NODE_TYPE_ALIAS:
  case NODE_VAR_DECL:
    return true;
  default:
    return false;
  }
}

/* True if the expression's value is fixed by the source text alone: it can't
 * call a function, read a `var`, or reach outside the program. This looks at
 * the shape of the tree. Nothing is evaluated. */
static bool isComptime(const AstNode *node, ComptimeNames *names) {
  switch (node->kind) {
  case NODE_LITERAL:
    return true;
  case NODE_UNARY:
    return isComptime(node->as.unary.operand, names);
  case NODE_BINARY:
    return isComptime(node->as.binary.left, names) &&
           isComptime(node->as.binary.right, names);
  case NODE_GROUPING:
    return isComptime(node->as.grouping.inner, names);
  case NODE_AND:
  case NODE_OR:
  case NODE_NULLISH:
    return isComptime(node->as.logical.left, names) &&
           isComptime(node->as.logical.right, names);
  case NODE_ARRAY:
    for (int i = 0; i < node->as.array.count; i++) {
      if (!isComptime(node->as.array.items[i], names))
        return false;
    }
    return true;
  case NODE_STRUCT_INIT:
    for (int i = 0; i < node->as.structInit.fieldCount; i++) {
      if (!isComptime(node->as.structInit.fields[i].value, names))
        return false;
    }
    return true;
  case NODE_FUNCTION:
    // Creating a lambda is all that happens on load. Its body does not run.
    return node->as.function.isLambda;
  case NODE_VARIABLE: {
    const Token *name = &node->as.variable.name;
    return stringSetContains(&names->funs, name->start, name->length) ||
           stringSetContains(&names->lets, name->start, name->length);
  }
  default:
    return false;
  }
}

static void errorAtToken(const Token *token, const char *message) {
  fprintf(stderr, "[line %d] Error at '%.*s': %s\n", token->line, token->length,
          token->start, message);
}

/* Checks one top-level `let` or `var`. Returns false if it was reported. */
static bool checkVarDecl(const VarDeclNode *decl, ComptimeNames *names) {
  const char *keyword = decl->isMutable ? "var" : "let";
  char message[96];

  if (decl->initializer == NULL) {
    snprintf(message, sizeof message,
             "A top-level '%s' must be assigned a comptime value.", keyword);
    errorAtToken(&decl->name, message);
    return false;
  }

  if (!isComptime(decl->initializer, names)) {
    snprintf(message, sizeof message,
             "A top-level '%s' can only be assigned a comptime value.",
             keyword);
    errorAtToken(&decl->name, message);
    return false;
  }

  // Only a `let` can be named by a later initializer. A `var` can change.
  if (!decl->isMutable) {
    stringSetAdd(&names->lets, decl->name.start, decl->name.length);
  }

  return true;
}

/* `fun main(): unit`: no parameters, no generic parameters, returns unit. */
static bool isMainShape(const AstNode *node) {
  if (node->kind != NODE_FUNCTION)
    return false;

  const FunctionNode *fn = &node->as.function;

  if (fn->isLambda || fn->arity != 0 || fn->genericParamCount != 0)
    return false;

  const AstNode *returnType = fn->returnType;

  return returnType != NULL && returnType->kind == NODE_TYPE &&
         returnType->as.type_.genericArgCount == 0 &&
         tokenTextEquals(&returnType->as.type_.name, "unit");
}

/* An entry file needs a `main`, and it has to have the right shape. A `let` or
 * `var` named `main` is the wrong shape, not a missing one. */
static bool checkMain(AstNode **ast, int count) {
  bool found = false;
  bool ok = true;

  for (int i = 0; i < count; i++) {
    const Token *name;

    if (ast[i]->kind == NODE_FUNCTION)
      name = &ast[i]->as.function.name;
    else if (ast[i]->kind == NODE_VAR_DECL)
      name = &ast[i]->as.varDecl.name;
    else
      continue;

    if (!tokenTextEquals(name, "main"))
      continue;

    found = true;

    if (!isMainShape(ast[i])) {
      errorAtToken(name, "'main' must be declared as 'fun main(): unit'.");
      ok = false;
    }
  }

  if (!found) {
    fprintf(stderr, "Error: No 'main' function. Running a file needs 'fun "
                    "main(): unit'.\n");
    ok = false;
  }

  return ok;
}

bool topLevelCheck(AstNode **ast, int count, bool isEntryFile) {
  bool ok = true;

  ComptimeNames names;
  stringSetInit(&names.funs);
  stringSetInit(&names.lets);

  for (int i = 0; i < count; i++) {
    if (ast[i]->kind == NODE_FUNCTION) {
      const Token *name = &ast[i]->as.function.name;
      stringSetAdd(&names.funs, name->start, name->length);
    }
  }

  for (int i = 0; i < count; i++) {
    if (!isDeclaration(ast[i])) {
      // Statement nodes have a line but no token, so there is no `at 'x'`.
      fprintf(stderr,
              "[line %d] Error: Only declarations can appear at the top level "
              "of a file.\n",
              ast[i]->line);
      ok = false;
    } else if (ast[i]->kind == NODE_VAR_DECL) {
      if (!checkVarDecl(&ast[i]->as.varDecl, &names))
        ok = false;
    }
  }

  stringSetFree(&names.funs);
  stringSetFree(&names.lets);

  if (isEntryFile && !checkMain(ast, count))
    ok = false;

  return ok;
}
