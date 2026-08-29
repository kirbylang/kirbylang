#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typecheck.h"

static bool hadError = false;

static void errorAtToken(Token *token, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

static void errorAtTokenFmt(Token *token, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  errorAtToken(token, message);
}

static void errorAtNode(AstNode *node, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error: %s\n", node->line, message);
}

static void errorAtNodeFmt(AstNode *node, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  errorAtNode(node, message);
}

bool typecheckHadError(void) { return hadError; }
void typecheckResetError(void) { hadError = false; }

typedef struct {
  Token name;
  Type *type;
} Binding;

typedef struct {
  Binding *bindings;
  int count;
  int capacity;
} Scope;

struct TypeEnv {
  Scope *scopes;
  int scopeCount;
  int scopeCapacity;

  Binding *structs;
  int structCount;
  int structCapacity;

  Binding *functions;
  int functionCount;
  int functionCapacity;

  // NULL when not currently checking a method body.
  Type *selfType;
  // NULL when not currently checking a function/method body (or when
  // that function's return type couldn't be resolved -- treated the same
  // as "no opinion" everywhere else in this module).
  Type *currentReturnType;
};

static void bindingArrayWrite(Binding **array, int *count, int *capacity,
                              Token name, Type *type) {
  if (*capacity < *count + 1) {
    *capacity = *capacity < 8 ? 8 : *capacity * 2;
    *array = (Binding *)realloc(*array, sizeof(Binding) * (*capacity));
    if (*array == NULL) {
      fprintf(stderr, "realloc failed in bindingArrayWrite\n");
      exit(1);
    }
  }
  (*array)[*count].name = name;
  (*array)[*count].type = type;
  (*count)++;
}

static bool tokensEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static Type *bindingArrayLookup(Binding *array, int count, Token name) {
  // Most recently declared binding wins
  for (int i = count - 1; i >= 0; i--) {
    if (tokensEqual(&array[i].name, &name))
      return array[i].type;
  }
  return NULL;
}

TypeEnv *typeEnvCreate(void) {
  TypeEnv *env = (TypeEnv *)malloc(sizeof(TypeEnv));
  memset(env, 0, sizeof(TypeEnv));
  return env;
}

void typeEnvDestroy(TypeEnv *env) {
  for (int i = 0; i < env->scopeCount; i++) {
    free(env->scopes[i].bindings);
  }
  free(env->scopes);
  free(env->structs);
  free(env->functions);
  free(env);
}

void typeEnvBeginScope(TypeEnv *env) {
  if (env->scopeCapacity < env->scopeCount + 1) {
    env->scopeCapacity = env->scopeCapacity < 8 ? 8 : env->scopeCapacity * 2;
    env->scopes =
        (Scope *)realloc(env->scopes, sizeof(Scope) * env->scopeCapacity);
    if (env->scopes == NULL) {
      fprintf(stderr, "realloc failed in typeEnvBeginScope\n");
      exit(1);
    }
  }
  env->scopes[env->scopeCount].bindings = NULL;
  env->scopes[env->scopeCount].count = 0;
  env->scopes[env->scopeCount].capacity = 0;
  env->scopeCount++;
}

void typeEnvEndScope(TypeEnv *env) {
  env->scopeCount--;
  free(env->scopes[env->scopeCount].bindings);
  env->scopes[env->scopeCount].bindings = NULL;
  env->scopes[env->scopeCount].count = 0;
  env->scopes[env->scopeCount].capacity = 0;
}

void typeEnvDeclare(TypeEnv *env, Token name, Type *type) {
  Scope *scope = &env->scopes[env->scopeCount - 1];
  bindingArrayWrite(&scope->bindings, &scope->count, &scope->capacity, name,
                    type);
}

Type *typeEnvLookup(TypeEnv *env, Token name) {
  for (int i = env->scopeCount - 1; i >= 0; i--) {
    Type *found =
        bindingArrayLookup(env->scopes[i].bindings, env->scopes[i].count, name);
    if (found != NULL)
      return found;
  }
  return NULL;
}

void typeEnvRegisterStruct(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->structs, &env->structCount, &env->structCapacity,
                    name, type);
}

Type *typeEnvLookupStruct(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->structs, env->structCount, name);
}

void typeEnvRegisterFunction(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->functions, &env->functionCount,
                    &env->functionCapacity, name, type);
}

Type *typeEnvLookupFunction(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->functions, env->functionCount, name);
}

void typeEnvSetSelfType(TypeEnv *env, Type *selfType) {
  env->selfType = selfType;
}

Type *typeEnvGetSelfType(TypeEnv *env) { return env->selfType; }

void typeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType) {
  env->currentReturnType = returnType;
}

Type *typeEnvGetCurrentReturnType(TypeEnv *env) {
  return env->currentReturnType;
}

static bool tokenTextEquals(Token *token, const char *text) {
  size_t len = strlen(text);
  if ((size_t)token->length != len)
    return false;
  return memcmp(token->start, text, len) == 0;
}

Type *resolveType(TypeEnv *env, AstNode *typeAnnotation) {
  if (typeAnnotation->kind == NODE_TYPE_FUNCTION) {
    TypeFunctionNode *fn = &typeAnnotation->as.typeFunction;

    Type **paramTypes = NULL;
    if (fn->paramCount > 0) {
      paramTypes = (Type **)typesAllocRaw(fn->paramCount * sizeof(Type *));
      for (int i = 0; i < fn->paramCount; i++) {
        Type *paramType = resolveType(env, fn->paramTypes[i]);
        if (paramType == NULL)
          return NULL; // error already reported below the recursive call
        paramTypes[i] = paramType;
      }
    }

    Type *returnType = resolveType(env, fn->returnType);
    if (returnType == NULL)
      return NULL;

    return typeFunction(paramTypes, fn->paramCount, returnType);
  }

  TypeNode *t = &typeAnnotation->as.type_;

  if (t->genericArgCount > 0) {
    errorAtToken(&t->name, "Generic types aren't supported yet.");
    return NULL;
  }

  if (tokenTextEquals(&t->name, "unit"))
    return typeUnit();
  if (tokenTextEquals(&t->name, "bool"))
    return typeBool();
  if (tokenTextEquals(&t->name, "string"))
    return typeString();
  if (tokenTextEquals(&t->name, "f64"))
    return typeF64();

  Type *structType = typeEnvLookupStruct(env, t->name);
  if (structType != NULL)
    return structType;

  errorAtToken(&t->name, "Unknown type.");
  return NULL;
}

static Type *inferLiteral(AstNode *node);
static Type *inferUnary(TypeEnv *env, AstNode *node);
static Type *inferBinary(TypeEnv *env, AstNode *node);
static Type *inferVariable(TypeEnv *env, AstNode *node);
static Type *inferAssign(TypeEnv *env, AstNode *node);
static Type *inferLogical(TypeEnv *env, AstNode *node);
static Type *inferNullish(TypeEnv *env, AstNode *node);
static Type *inferCall(TypeEnv *env, AstNode *node);
static Type *checkCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                          Type *calleeType);
static Type *inferGet(TypeEnv *env, AstNode *node);
static Type *inferSet(TypeEnv *env, AstNode *node);
static Type *inferSelfNode(TypeEnv *env, AstNode *node);
static Type *inferIndexGet(TypeEnv *env, AstNode *node);
static Type *inferIndexSet(TypeEnv *env, AstNode *node);
static Type *inferStructInit(TypeEnv *env, AstNode *node);
static Type *inferArrayNode(TypeEnv *env, AstNode *node);
static Type *inferIf(TypeEnv *env, AstNode *node);
static Type *inferBlock(TypeEnv *env, AstNode *node);
static Type *checkBlockContents(TypeEnv *env, BlockNode *block,
                                Type *expectedValueType);
static Type *checkOrInferLambda(TypeEnv *env, AstNode *node, Type *expected);
static void checkVarDecl(TypeEnv *env, AstNode *node);
static void checkFunctionDecl(TypeEnv *env, AstNode *node);

Type *infer(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_LITERAL:
    return inferLiteral(node);
  case NODE_UNARY:
    return inferUnary(env, node);
  case NODE_BINARY:
    return inferBinary(env, node);
  case NODE_GROUPING:
    return infer(env, node->as.grouping.inner);
  case NODE_VARIABLE:
    return inferVariable(env, node);
  case NODE_ASSIGN:
    return inferAssign(env, node);
  case NODE_AND:
  case NODE_OR:
    return inferLogical(env, node);
  case NODE_NULLISH:
    return inferNullish(env, node);
  case NODE_CALL:
    return inferCall(env, node);
  case NODE_GET:
    return inferGet(env, node);
  case NODE_SET:
    return inferSet(env, node);
  case NODE_SELF:
    return inferSelfNode(env, node);
  case NODE_INDEX_GET:
    return inferIndexGet(env, node);
  case NODE_INDEX_SET:
    return inferIndexSet(env, node);
  case NODE_STRUCT_INIT:
    return inferStructInit(env, node);
  case NODE_ARRAY:
    return inferArrayNode(env, node);
  case NODE_IF:
    return inferIf(env, node);
  case NODE_BLOCK:
    return inferBlock(env, node);
  case NODE_FUNCTION:
    if (node->as.function.isLambda)
      return checkOrInferLambda(env, node, NULL);
    errorAtNode(node, "Internal: unexpected function declaration in "
                      "expression position.");
    return NULL;
  default:
    errorAtNode(node, "Internal: this isn't a checkable expression.");
    return NULL;
  }
}

bool check(TypeEnv *env, AstNode *node, Type *expected) {
  if (node->kind == NODE_FUNCTION && node->as.function.isLambda) {
    return checkOrInferLambda(env, node, expected) != NULL;
  }

  if (node->kind == NODE_ARRAY && expected != NULL) {
    if (expected->kind != TYPE_ARRAY) {
      errorAtNodeFmt(node, "Expected %s, got an array.",
                     typeToString(expected));
      return false;
    }
    ArrayNode *a = &node->as.array;
    bool ok = true;
    for (int i = 0; i < a->count; i++) {
      if (!check(env, a->items[i], expected->as.array.elementType))
        ok = false;
    }
    return ok;
  }

  Type *actual = infer(env, node);
  if (actual == NULL)
    return true;
  if (expected == NULL)
    return true;
  if (!typesEqual(actual, expected)) {
    errorAtNodeFmt(node, "Expected %s, got %s.", typeToString(expected),
                   typeToString(actual));
    return false;
  }
  return true;
}

// True if this struct's members shouldn't be trusted for a "no such
// field/method" error -- either because it's generic (known before
// resolution was even attempted) or because one or more of its actual
// fields/methods failed to resolve for some other reason (a missing
// annotation, an unknown type name, etc.). Either way, whatever specific
// error already got reported explains the problem; a lookup miss here
// shouldn't also cascade a second, misleading "doesn't exist" on top of
// it.
static bool structMembersUnreliable(Type *type) {
  return typeStructIsGeneric(type) || typeStructHasUnresolvedMembers(type);
}

static Type *inferLiteral(AstNode *node) {
  LiteralNode *lit = &node->as.literal;
  switch (lit->kind) {
  case LITERAL_NIL:
    return typeUnit();
  case LITERAL_BOOL:
    return typeBool();
  case LITERAL_NUMBER:
    return typeF64();
  case LITERAL_STRING:
    return typeString();
  }
  return NULL; // unreachable
}

static Type *inferUnary(TypeEnv *env, AstNode *node) {
  UnaryNode *u = &node->as.unary;

  if (u->op.type == TOKEN_BANG) {
    Type *operandType = infer(env, u->operand);

    if (operandType == NULL)
      return NULL;

    return typeBool();
  }

  if (!check(env, u->operand, typeF64()))
    return NULL;

  return typeF64();
}

static Type *inferBinary(TypeEnv *env, AstNode *node) {
  BinaryNode *b = &node->as.binary;
  Type *leftType = infer(env, b->left);
  Type *rightType = infer(env, b->right);
  if (leftType == NULL || rightType == NULL)
    return NULL;

  switch (b->op.type) {
  case TOKEN_PLUS:
    if (typesEqual(leftType, typeF64()) && typesEqual(rightType, typeF64()))
      return typeF64();
    if (typesEqual(leftType, typeString()) &&
        typesEqual(rightType, typeString()))
      return typeString();
    errorAtTokenFmt(&b->op, "'+' needs two f64s or two strings, got %s and %s.",
                    typeToString(leftType), typeToString(rightType));
    return NULL;

  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MODULO:
    if (!typesEqual(leftType, typeF64())) {
      errorAtNodeFmt(b->left, "Expected f64, got %s.", typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      errorAtNodeFmt(b->right, "Expected f64, got %s.",
                     typeToString(rightType));
      return NULL;
    }
    return typeF64();

  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
    if (!typesEqual(leftType, rightType)) {
      errorAtTokenFmt(&b->op,
                      "Both sides of '%s' must be the same type, got %s "
                      "and %s.",
                      b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=",
                      typeToString(leftType), typeToString(rightType));
      return NULL;
    }
    return typeBool();

  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    if (!typesEqual(leftType, typeF64())) {
      errorAtNodeFmt(b->left, "Expected f64, got %s.", typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      errorAtNodeFmt(b->right, "Expected f64, got %s.",
                     typeToString(rightType));
      return NULL;
    }
    return typeBool();

  default:
    errorAtToken(&b->op, "Internal: unhandled binary operator.");
    return NULL;
  }
}

static Type *inferVariable(TypeEnv *env, AstNode *node) {
  Token *name = &node->as.variable.name;
  Type *type = typeEnvLookup(env, *name);

  if (type == NULL)
    type = typeEnvLookupFunction(env, *name);

  if (type == NULL)
    return NULL;

  return type;
}

static Type *inferAssign(TypeEnv *env, AstNode *node) {
  AssignNode *a = &node->as.assign;
  Type *varType = typeEnvLookup(env, a->name);

  if (varType == NULL)
    varType = typeEnvLookupFunction(env, a->name);

  if (varType == NULL) {
    infer(env, a->value);
    return NULL;
  }

  if (!check(env, a->value, varType))
    return NULL;

  return varType;
}

static Type *inferLogical(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical;
  Type *leftType = infer(env, l->left);
  Type *rightType = infer(env, l->right);
  if (leftType == NULL || rightType == NULL)
    return NULL;
  if (!typesEqual(leftType, rightType)) {
    errorAtNodeFmt(node,
                   "Both sides of '%s' must be the same type, got %s and "
                   "%s -- Kirby's and/or return whichever operand's "
                   "actual value wins, not a computed bool.",
                   node->kind == NODE_AND ? "and" : "or",
                   typeToString(leftType), typeToString(rightType));
    return NULL;
  }
  return leftType;
}

static Type *inferNullish(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical; // NODE_NULLISH reuses this union
                                      // member, same as compiler.c does
  infer(env, l->left); // unconstrained -- nil is a wildcard, no Option[T]
                       // to check the left side's "presence" against yet
  return infer(env, l->right); // result type comes from the fallback
}

static Type *inferCall(TypeEnv *env, AstNode *node) {
  CallNode *c = &node->as.call;

  if (c->callee->kind == NODE_VARIABLE) {
    Token *name = &c->callee->as.variable.name;
    Type *calleeType = typeEnvLookup(env, *name);
    if (calleeType == NULL)
      calleeType = typeEnvLookupFunction(env, *name);
    if (calleeType == NULL) {
      // Presumed native -- unchecked (see inferVariable's comment).
      // Still walk the arguments so any *internal* errors inside them
      // get caught, just don't validate arg count/types against a
      // signature we don't have.
      for (int i = 0; i < c->argCount; i++)
        infer(env, c->args[i]);
      return NULL;
    }
    return checkCallAgainstFunctionType(env, node, calleeType);
  }

  Type *calleeType = infer(env, c->callee);
  if (calleeType == NULL)
    return NULL;
  return checkCallAgainstFunctionType(env, node, calleeType);
}

static Type *checkCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                          Type *calleeType) {
  CallNode *c = &node->as.call;

  if (calleeType->kind != TYPE_FN) {
    errorAtTokenFmt(&c->paren, "%s isn't callable.", typeToString(calleeType));
    return NULL;
  }
  if (c->argCount != calleeType->as.function.paramCount) {
    errorAtTokenFmt(&c->paren, "Expected %d argument(s), got %d.",
                    calleeType->as.function.paramCount, c->argCount);
    return NULL;
  }

  bool ok = true;
  for (int i = 0; i < c->argCount; i++) {
    if (!check(env, c->args[i], calleeType->as.function.paramTypes[i]))
      ok = false;
  }
  if (!ok)
    return NULL;

  return calleeType->as.function.returnType;
}

static Type *inferGet(TypeEnv *env, AstNode *node) {
  GetNode *g = &node->as.get;

  // Point.new(...): a bare reference to a *registered struct name* is
  // static-member access -- but only when nothing in ordinary scope
  // shadows that name first, matching the same shadowing precedence the
  // compiler already applies at the bytecode level (a struct's own name
  // is just an ordinary global; a local variable of the same name wins).
  if (g->object->kind == NODE_VARIABLE) {
    Token *objectName = &g->object->as.variable.name;
    bool shadowed = typeEnvLookup(env, *objectName) != NULL ||
                    typeEnvLookupFunction(env, *objectName) != NULL;
    if (!shadowed) {
      Type *structType = typeEnvLookupStruct(env, *objectName);
      if (structType != NULL) {
        if (structMembersUnreliable(structType))
          return NULL; // already reported once

        Type *methodType = typeStructStaticMethodLookup(structType, g->name);
        if (methodType == NULL) {
          errorAtTokenFmt(&g->name, "%s has no static method '%.*s'.",
                          typeToString(structType), g->name.length,
                          g->name.start);
          return NULL;
        }
        return methodType;
      }
    }
  }

  Type *objectType = infer(env, g->object);
  if (objectType == NULL)
    return NULL;

  if (objectType->kind != TYPE_STRUCT) {
    errorAtTokenFmt(&g->name, "Can't access '.%.*s' on a %s.", g->name.length,
                    g->name.start, typeToString(objectType));
    return NULL;
  }

  if (structMembersUnreliable(objectType))
    return NULL; // already reported once

  // Fields take priority over instance methods on a name collision --
  // correct for the common case. The runtime's actual precedence is more
  // nuanced than a fixed order (a private field inaccessible from the
  // current context falls back to a same-named public method instead),
  // which needs field/method visibility tracked in the type system to
  // replicate correctly -- not modeled here yet.
  Type *fieldType = typeStructFieldLookup(objectType, g->name);
  if (fieldType != NULL)
    return fieldType;

  Type *methodType = typeStructInstanceMethodLookup(objectType, g->name);
  if (methodType != NULL)
    return methodType;

  // A more specific, helpful reason when we have one: the name exists,
  // just not as an instance member.
  Type *staticMethodType = typeStructStaticMethodLookup(objectType, g->name);
  if (staticMethodType != NULL) {
    errorAtTokenFmt(&g->name,
                    "'%.*s' is a static method. Access it on '%.*s' "
                    "instead of an instance.",
                    g->name.length, g->name.start,
                    objectType->as.struct_.name.length,
                    objectType->as.struct_.name.start);
    return NULL;
  }

  errorAtTokenFmt(&g->name, "%s has no field or method '%.*s'.",
                  typeToString(objectType), g->name.length, g->name.start);
  return NULL;
}

static Type *inferSet(TypeEnv *env, AstNode *node) {
  SetNode *s = &node->as.set;
  Type *objectType = infer(env, s->object);
  if (objectType == NULL) {
    infer(env, s->value); // still walk for internal errors
    return NULL;
  }

  if (objectType->kind != TYPE_STRUCT) {
    errorAtTokenFmt(&s->name, "Can't set '.%.*s' on a %s.", s->name.length,
                    s->name.start, typeToString(objectType));
    return NULL;
  }

  if (structMembersUnreliable(objectType)) {
    infer(env, s->value); // still walk for internal errors
    return NULL;          // already reported once
  }

  Type *fieldType = typeStructFieldLookup(objectType, s->name);
  if (fieldType == NULL) {
    errorAtTokenFmt(&s->name, "%s has no field '%.*s'.",
                    typeToString(objectType), s->name.length, s->name.start);
    return NULL;
  }

  if (!check(env, s->value, fieldType))
    return NULL;
  return fieldType; // an assignment expression evaluates to the assigned
                    // value, same as inferAssign()
}

static Type *inferSelfNode(TypeEnv *env, AstNode *node) {
  Type *selfType = typeEnvGetSelfType(env);
  if (selfType == NULL) {
    // The compiler already rejects `self` outside a method at compile
    // time -- if we somehow get here anyway, don't crash, just report
    // plainly rather than assuming.
    errorAtNode(node, "'self' isn't valid here.");
    return NULL;
  }
  return selfType;
}

static Type *inferIndexGet(TypeEnv *env, AstNode *node) {
  IndexGetNode *ig = &node->as.indexGet;
  Type *objectType = infer(env, ig->object);
  if (objectType == NULL) {
    infer(env, ig->index);
    return NULL;
  }
  if (objectType->kind != TYPE_ARRAY) {
    errorAtTokenFmt(&ig->bracket, "Can't index into a %s.",
                    typeToString(objectType));
    return NULL;
  }
  if (!check(env, ig->index, typeF64()))
    return NULL;
  return objectType->as.array.elementType; // may be NULL (empty-array
                                           // case) -- propagates as "no
                                           // opinion," not an error
}

static Type *inferIndexSet(TypeEnv *env, AstNode *node) {
  IndexSetNode *is = &node->as.indexSet;
  Type *objectType = infer(env, is->object);
  if (objectType == NULL) {
    infer(env, is->index);
    infer(env, is->value);
    return NULL;
  }
  if (objectType->kind != TYPE_ARRAY) {
    errorAtTokenFmt(&is->bracket, "Can't index into a %s.",
                    typeToString(objectType));
    return NULL;
  }
  if (!check(env, is->index, typeF64()))
    return NULL;

  Type *elementType = objectType->as.array.elementType;
  if (elementType != NULL) {
    if (!check(env, is->value, elementType))
      return NULL;
  } else {
    infer(env, is->value); // nothing to check against yet (empty-array
                           // case), still walk for internal errors
  }
  return elementType;
}

static Type *inferStructInit(TypeEnv *env, AstNode *node) {
  StructInitNode *si = &node->as.structInit;
  Type *structType = typeEnvLookupStruct(env, si->name);
  if (structType == NULL) {
    errorAtTokenFmt(&si->name, "Unknown struct '%.*s'.", si->name.length,
                    si->name.start);
    return NULL;
  }

  if (structMembersUnreliable(structType)) {
    for (int i = 0; i < si->fieldCount; i++) {
      infer(env, si->fields[i].value); // still walk for internal errors
    }
    return structType; // already reported once
  }

  // Deliberately not checking for *missing* required fields here --
  // that's already a runtime-level check ("Missing required field"),
  // unrelated to types, and stays exactly as it is.
  bool ok = true;
  for (int i = 0; i < si->fieldCount; i++) {
    StructInitFieldNode *field = &si->fields[i];
    Type *fieldType = typeStructFieldLookup(structType, field->name);
    if (fieldType == NULL) {
      errorAtTokenFmt(&field->name, "%s has no field '%.*s'.",
                      typeToString(structType), field->name.length,
                      field->name.start);
      ok = false;
      continue;
    }
    if (!check(env, field->value, fieldType))
      ok = false;
  }
  if (!ok)
    return NULL;
  return structType;
}

static Type *inferArrayNode(TypeEnv *env, AstNode *node) {
  ArrayNode *a = &node->as.array;
  if (a->count == 0)
    return typeArray(NULL); // nothing to learn an element type from yet

  Type *elementType = infer(env, a->items[0]);
  bool ok = true;
  for (int i = 1; i < a->count; i++) {
    if (elementType != NULL) {
      if (!check(env, a->items[i], elementType))
        ok = false;
    } else {
      infer(env, a->items[i]); // still walk for internal errors
    }
  }
  if (!ok)
    return NULL;
  return typeArray(elementType);
}

static Type *inferIf(TypeEnv *env, AstNode *node) {
  IfNode *i = &node->as.if_;
  check(env, i->condition, typeBool()); // reported if wrong; still proceed

  Type *thenType = infer(env, i->thenBranch);
  Type *elseType =
      i->elseBranch != NULL ? infer(env, i->elseBranch) : typeUnit();
  if (thenType == NULL || elseType == NULL)
    return NULL;

  if (!typesEqual(thenType, elseType)) {
    errorAtNodeFmt(node,
                   "if/else branches must produce the same type, got %s "
                   "and %s.",
                   typeToString(thenType), typeToString(elseType));
    return NULL;
  }
  return thenType;
}

static Type *checkBlockContents(TypeEnv *env, BlockNode *block,
                                Type *expectedValueType) {
  typeEnvBeginScope(env);

  for (int i = 0; i < block->count; i++) {
    checkStmt(env, block->stmts[i]);
  }

  Type *result;
  if (block->value != NULL) {
    if (expectedValueType != NULL) {
      result = check(env, block->value, expectedValueType) ? expectedValueType
                                                           : NULL;
    } else {
      result = infer(env, block->value);
    }
  } else {
    // No trailing value -- always unit, regardless of whether a return
    // type was expected. A function/method body can equally well satisfy
    // its declared return type through explicit `return` statements
    // elsewhere in the block; verifying that every path actually does so
    // is real control-flow analysis, not attempted here.
    result = typeUnit();
  }

  typeEnvEndScope(env);
  return result;
}

static Type *inferBlock(TypeEnv *env, AstNode *node) {
  return checkBlockContents(env, &node->as.block, NULL);
}

// expected may be NULL (infer() context -- every param must already have
// an explicit type in that case) or a TYPE_FN (check() context --
// any untyped param takes its type from the matching position in
// expected). Shared by infer()'s NODE_FUNCTION case and check()'s lambda
// special case, since the two only differ in where param types come from
// when they're missing.
static Type *checkOrInferLambda(TypeEnv *env, AstNode *node, Type *expected) {
  FunctionNode *fn = &node->as.function;

  if (expected != NULL && expected->kind != TYPE_FN) {
    errorAtNodeFmt(node, "Expected %s here, not a function.",
                   typeToString(expected));
    return NULL;
  }
  if (expected != NULL && expected->as.function.paramCount != fn->arity) {
    errorAtNodeFmt(node,
                   "Expected a function taking %d argument(s), this one "
                   "takes %d.",
                   expected->as.function.paramCount, fn->arity);
    return NULL;
  }

  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  bool ok = true;
  for (int i = 0; i < fn->arity; i++) {
    if (fn->paramTypes != NULL && fn->paramTypes[i] != NULL) {
      paramTypes[i] = resolveType(env, fn->paramTypes[i]);
      if (paramTypes[i] == NULL)
        ok = false;
    } else if (expected != NULL) {
      paramTypes[i] = expected->as.function.paramTypes[i];
    } else {
      errorAtNode(node,
                  "Can't infer this lambda's parameter types without more "
                  "context -- add explicit types, or use it somewhere "
                  "its type is already known.");
      paramTypes[i] = NULL;
      ok = false;
    }
  }
  if (!ok)
    return NULL;

  Type *declaredReturnType =
      fn->returnType != NULL ? resolveType(env, fn->returnType) : NULL;
  Type *targetReturnType =
      declaredReturnType != NULL
          ? declaredReturnType
          : (expected != NULL ? expected->as.function.returnType : NULL);

  typeEnvBeginScope(env);
  for (int i = 0; i < fn->arity; i++) {
    typeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousReturnType = typeEnvGetCurrentReturnType(env);
  typeEnvSetCurrentReturnType(env, targetReturnType);

  Type *bodyResultType;
  if (fn->exprBody != NULL) {
    bodyResultType =
        targetReturnType != NULL
            ? (check(env, fn->exprBody, targetReturnType) ? targetReturnType
                                                          : NULL)
            : infer(env, fn->exprBody);
  } else {
    bodyResultType = checkBlockContents(env, &fn->body, targetReturnType);
  }

  typeEnvSetCurrentReturnType(env, previousReturnType);
  typeEnvEndScope(env);

  if (bodyResultType == NULL)
    return NULL;

  Type *actualReturnType =
      declaredReturnType != NULL ? declaredReturnType : bodyResultType;
  return typeFunction(paramTypes, fn->arity, actualReturnType);
}

// --- statement-level checking ---

static void checkVarDecl(TypeEnv *env, AstNode *node) {
  VarDeclNode *vd = &node->as.varDecl;
  Type *declaredType =
      vd->declaredType != NULL ? resolveType(env, vd->declaredType) : NULL;

  if (vd->initializer != NULL) {
    if (declaredType != NULL) {
      check(env, vd->initializer, declaredType);
      typeEnvDeclare(env, vd->name, declaredType);
    } else {
      Type *inferred = infer(env, vd->initializer);
      typeEnvDeclare(env, vd->name, inferred);
    }
  } else {
    if (declaredType == NULL) {
      errorAtTokenFmt(&vd->name,
                      "'%.*s' needs a type -- it has no initializer to "
                      "infer one from.",
                      vd->name.length, vd->name.start);
    }
    typeEnvDeclare(env, vd->name, declaredType);
  }
}

// --- definite-assignment analysis ---
//
// A separate, self-contained pass over the same AST checkFunctionBody
// already walks for types -- tracks which uninitialized var/let bindings
// haven't been assigned yet at each point, flagging a read that isn't
// guaranteed to be preceded by an assignment on every path. Intentionally
// intraprocedural: an assignment that happens inside a *called* function
// (rather than directly in the body being analyzed) is invisible here,
// same as every mainstream language's version of this analysis.

typedef struct {
  Token *names;
  int count;
  int capacity;
} PendingSet;

static void pendingSetInit(PendingSet *set) {
  set->names = NULL;
  set->count = 0;
  set->capacity = 0;
}

static void pendingSetFree(PendingSet *set) {
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
static bool checkAssignmentStmt(PendingSet *pending, AstNode *node);

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
    checkAssignmentExpr(pending, a->value); // RHS evaluated before the
                                            // assignment takes effect, so
                                            // `x = x + 1` still catches a
                                            // read of a not-yet-assigned x
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
    // A lambda literal -- analyzed independently, with its own fresh
    // pending set, whenever/if it's actually invoked as a function body.
    // Its own local bindings aren't part of the enclosing scope.
    break;
  case NODE_LITERAL:
  case NODE_SELF:
    break; // no sub-expressions
  default:
    break;
  }
}

static bool checkAssignmentStmt(PendingSet *pending, AstNode *node) {
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

// Runs definite-assignment analysis over one function/method's block body.
static void checkDefiniteAssignment(FunctionNode *fn) {
  // Expression-bodied functions (`fun f() = expr;`) can't declare a var/let at
  // all, so there's nothing to check there.
  if (fn->exprBody != NULL)
    return;

  PendingSet pending;
  pendingSetInit(&pending);
  checkAssignmentBlock(&pending, &fn->body);
  pendingSetFree(&pending);
}

// Checks a function/method BODY against an already-resolved signature --
// doesn't re-resolve param/return annotations itself, since callers need
// different registration/enforcement semantics around that (a local
// function just declares into the current scope; a top-level function or
// impl method needs the whole-program driver's hoisting and mandatory-
// annotation enforcement). selfType is NULL for a non-method function;
// non-NULL for a method, become the type `self` resolves to inside.
static void checkFunctionBody(TypeEnv *env, FunctionNode *fn, Type **paramTypes,
                              Type *returnType, Type *selfType) {
  typeEnvBeginScope(env);
  for (int i = 0; i < fn->arity; i++) {
    typeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousSelfType = typeEnvGetSelfType(env);
  typeEnvSetSelfType(env, selfType);
  Type *previousReturnType = typeEnvGetCurrentReturnType(env);
  typeEnvSetCurrentReturnType(env, returnType);

  if (fn->exprBody != NULL) {
    if (returnType != NULL)
      check(env, fn->exprBody, returnType);
    else
      infer(env, fn->exprBody);
  } else {
    checkBlockContents(env, &fn->body, returnType);
  }

  checkDefiniteAssignment(fn);

  typeEnvSetCurrentReturnType(env, previousReturnType);
  typeEnvSetSelfType(env, previousSelfType);
  typeEnvEndScope(env);
}

static void checkFunctionDecl(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;

  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = (fn->paramTypes != NULL && fn->paramTypes[i] != NULL)
                        ? resolveType(env, fn->paramTypes[i])
                        : NULL;
  }
  Type *returnType =
      fn->returnType != NULL ? resolveType(env, fn->returnType) : NULL;

  // Declared in the *enclosing* scope, before its own body is checked, so
  // a local/nested function can call itself recursively -- matches how
  // this already works at runtime.
  Type *fnType = typeFunction(paramTypes, fn->arity, returnType);
  typeEnvDeclare(env, fn->name, fnType);

  // Inherits the ambient self-type unchanged (rather than resetting it to
  // NULL) -- a nested function declared inside a method is a closure over
  // self too, same as it closes over any other local variable, not just
  // a lambda is.
  checkFunctionBody(env, fn, paramTypes, returnType, typeEnvGetSelfType(env));
}

void checkStmt(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    infer(env, node->as.exprStmt.expr);
    break;
  case NODE_PRINT:
    infer(env, node->as.print.expr); // unconstrained -- print accepts
                                     // anything, matches runtime reality
    break;
  case NODE_VAR_DECL:
    checkVarDecl(env, node);
    break;
  case NODE_WHILE: {
    WhileNode *w = &node->as.while_;
    check(env, w->condition, typeBool());
    // checkStmt(), not infer() -- the body might be a bare statement
    // (while (cond) return;, no braces), which infer() has no case for.
    // checkStmt()'s own default falls through to infer() for a block or
    // if-as-expression body, so this still handles those correctly too.
    checkStmt(env, w->body);
    break;
  }
  case NODE_FOR: {
    ForNode *f = &node->as.for_;
    typeEnvBeginScope(env); // scopes the loop variable to the loop itself
    if (f->init != NULL)
      checkStmt(env, f->init);
    if (f->condition != NULL)
      check(env, f->condition, typeBool());
    checkStmt(env, f->body); // same reasoning as NODE_WHILE above
    if (f->increment != NULL)
      infer(env, f->increment);
    typeEnvEndScope(env);
    break;
  }
  case NODE_IF: {
    // Statement-position if -- e.g. `if (n < 2) return n;`, whose branch
    // is a bare NODE_RETURN with no braces, not an expression. Routed
    // through checkStmt() (not inferIf()/infer(), which assume genuine
    // expression branches and are what expression-position if -- `var x
    // = if (a) "yes" else "no";` -- still correctly uses, unchanged).
    IfNode *i = &node->as.if_;
    check(env, i->condition, typeBool());
    checkStmt(env, i->thenBranch);
    if (i->elseBranch != NULL)
      checkStmt(env, i->elseBranch);
    break;
  }
  case NODE_RETURN: {
    ReturnNode *r = &node->as.return_;
    Type *expectedReturn = typeEnvGetCurrentReturnType(env);
    if (r->value != NULL) {
      if (expectedReturn != NULL)
        check(env, r->value, expectedReturn);
      else
        infer(env, r->value);
    } else if (expectedReturn != NULL &&
               !typesEqual(expectedReturn, typeUnit())) {
      errorAtNodeFmt(node, "Expected a return value of type %s.",
                     typeToString(expectedReturn));
    }
    break;
  }
  case NODE_BREAK:
  case NODE_CONTINUE:
    break;
  case NODE_FUNCTION:
    checkFunctionDecl(env, node);
    break;
  case NODE_STRUCT:
  case NODE_IMPL:
  case NODE_TYPE_ALIAS:
    // Registering a struct's full type (fields + every impl block's
    // methods, which can appear anywhere in the file, including *before*
    // the struct itself) needs a whole-program pass -- typecheckProgram()
    // handles these directly rather than through checkStmt(). No-op here
    // rather than falling through to infer()'s default case, which has
    // no idea what to do with these and would report a bogus "unchecked
    // expression" error.
    break;
  default:
    // Anything expression-shaped reached directly (shouldn't normally
    // happen -- NODE_EXPR_STMT wraps these -- but infer() rather than
    // silently doing nothing).
    infer(env, node);
    break;
  }
}

// --- whole-program driver ---
//
// Resolves a function or method's full signature, enforcing that every
// parameter and the return type actually have an annotation. self is
// deliberately excluded: it never has, or needs, its own annotation,
// since its type is always just "this struct," bound separately
// (checkFunctionBody's selfType parameter) when a method's body is
// actually checked, not here.
static Type *resolveFunctionSignature(TypeEnv *env, FunctionNode *fn) {
  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  bool ok = true;
  for (int i = 0; i < fn->arity; i++) {
    if (fn->paramTypes == NULL || fn->paramTypes[i] == NULL) {
      errorAtTokenFmt(&fn->params[i], "Parameter '%.*s' needs a type.",
                      fn->params[i].length, fn->params[i].start);
      ok = false;
      continue;
    }
    Type *paramType = resolveType(env, fn->paramTypes[i]);
    if (paramType == NULL)
      ok = false;
    paramTypes[i] = paramType;
  }

  if (fn->returnType == NULL) {
    errorAtTokenFmt(&fn->name, "'%.*s' needs a return type.", fn->name.length,
                    fn->name.start);
    ok = false;
  }
  Type *returnType =
      fn->returnType != NULL ? resolveType(env, fn->returnType) : NULL;

  if (!ok)
    return NULL;
  return typeFunction(paramTypes, fn->arity, returnType);
}

// Pass B: resolves one struct's fields onto its already-registered (Pass
// A) placeholder Type*, enforcing every field has an explicit type.
static void resolveStructFields(TypeEnv *env, AstNode *node) {
  StructNode *sn = &node->as.struct_;
  Type *structType = typeEnvLookupStruct(env, sn->name);
  if (structType == NULL)
    return; // shouldn't happen -- Pass A registers every NODE_STRUCT
  if (typeStructIsGeneric(structType))
    return; // already reported once at declaration; don't cascade

  TypeMember *fields =
      sn->fieldCount > 0
          ? (TypeMember *)typesAllocRaw(sn->fieldCount * sizeof(TypeMember))
          : NULL;
  bool ok = true;
  for (int i = 0; i < sn->fieldCount; i++) {
    VarDeclNode *field = &sn->fields[i];
    if (field->declaredType == NULL) {
      errorAtTokenFmt(&field->name, "Field '%.*s' needs a type.",
                      field->name.length, field->name.start);
      ok = false;
      continue;
    }
    Type *fieldType = resolveType(env, field->declaredType);
    if (fieldType == NULL) {
      ok = false;
      continue;
    }
    fields[i].name = field->name;
    fields[i].type = fieldType;
  }

  // Only attach fields if every single one resolved -- leaving the
  // struct at its Pass-A empty-fields state on any failure, rather than
  // risking a partially-filled array with uninitialized gaps from the
  // entries that hit `continue` above.
  if (ok) {
    typeStructSetFields(structType, fields, sn->fieldCount);
  } else {
    typeStructMarkUnresolvedMembers(structType);
  }
}

// Pass C: resolves one impl block's methods onto their struct, appending
// (not replacing) since a struct can have multiple impl blocks anywhere
// in the file.
static void registerImplMethods(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  Type *structType = typeEnvLookupStruct(env, impl->name);
  if (structType == NULL) {
    // impl of a struct that was never declared -- already a separate
    // runtime error unrelated to types, nothing to attach methods to.
    return;
  }
  if (typeStructIsGeneric(structType))
    return; // already reported once at the struct's declaration

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = resolveFunctionSignature(env, method);
    if (methodType == NULL) {
      typeStructMarkUnresolvedMembers(structType); // error already reported
      continue;
    }
    if (method->hasSelf) {
      typeStructAddInstanceMethod(structType, method->name, methodType);
    } else {
      typeStructAddStaticMethod(structType, method->name, methodType);
    }
  }
}

// Pass D: registers one top-level function's signature into the hoisted
// registry -- visible anywhere in the compiled unit regardless of
// declaration order, including mutual recursion between two top-level
// functions.
static void registerTopLevelFunctionSignature(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = resolveFunctionSignature(env, fn);
  if (fnType != NULL) {
    typeEnvRegisterFunction(env, fn->name, fnType);
  }
}

// Pass E: checks one top-level function's body against its already-
// registered (Pass D) signature.
static void checkTopLevelFunctionBody(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = typeEnvLookupFunction(env, fn->name);
  if (fnType == NULL)
    return; // signature failed to resolve in Pass D; already reported
  checkFunctionBody(env, fn, fnType->as.function.paramTypes,
                    fnType->as.function.returnType, NULL);
}

// Pass E: checks every method body in one impl block against its
// already-registered (Pass C) signature, with self bound to the struct.
static void checkImplMethodBodies(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  Type *structType = typeEnvLookupStruct(env, impl->name);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType =
        structType == NULL
            ? NULL
            : (method->hasSelf
                   ? typeStructInstanceMethodLookup(structType, method->name)
                   : typeStructStaticMethodLookup(structType, method->name));
    if (methodType == NULL)
      continue; // signature failed to resolve, or structType itself is
                // missing -- already reported (or a separate runtime
                // error, for the missing-struct case)

    Type *selfType = method->hasSelf ? structType : NULL;
    checkFunctionBody(env, method, methodType->as.function.paramTypes,
                      methodType->as.function.returnType, selfType);
  }
}

bool typecheckProgram(AstNode **program, int count) {
  TypeEnv *env = typeEnvCreate();
  typeEnvBeginScope(env); // one long-lived top-level scope for the program

  // Pass A: every struct's *name* first, so self-referential fields and
  // forward references to a struct declared later both resolve.
  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      StructNode *sn = &program[i]->as.struct_;
      Type *placeholder = typeStruct(sn->name, NULL, 0, NULL, 0, NULL, 0);
      if (sn->genericParamCount > 0) {
        typeStructMarkGeneric(placeholder);
        errorAtToken(&sn->name, "Generic structs aren't supported yet.");
      }
      typeEnvRegisterStruct(env, sn->name, placeholder);
    }
  }

  // Pass B: every struct's fields.
  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      resolveStructFields(env, program[i]);
    }
  }

  // Pass C: every impl block's methods.
  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      registerImplMethods(env, program[i]);
    }
  }

  // Pass D: every top-level function's signature (hoisted).
  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_FUNCTION) {
      registerTopLevelFunctionSignature(env, program[i]);
    }
  }

  // Pass E: check every body, and every other top-level statement, in
  // program order. Also tracks definite-assignment across the top-level
  // statements themselves (not just inside each function body) -- a
  // top-level uninitialized var is exactly as real a case as a local
  // one. checkAssignmentStmt() already no-ops on NODE_FUNCTION/NODE_IMPL
  // without recursing into them, since those get their own independent
  // analysis via checkFunctionBody() below.
  PendingSet topLevelPending;
  pendingSetInit(&topLevelPending);
  for (int i = 0; i < count; i++) {
    AstNode *node = program[i];
    if (node->kind == NODE_FUNCTION) {
      checkTopLevelFunctionBody(env, node);
    } else if (node->kind == NODE_IMPL) {
      checkImplMethodBodies(env, node);
    } else {
      checkStmt(env, node);
    }
    checkAssignmentStmt(&topLevelPending, node);
  }
  pendingSetFree(&topLevelPending);

  typeEnvEndScope(env);
  bool ok = !typecheckHadError();
  typeEnvDestroy(env);
  return ok;
}
