#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definite_assignment.h"
#include "typecheck.h"

static bool hadError = false;

static bool tokensEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static bool tokenTextEquals(Token *token, const char *text) {
  size_t len = strlen(text);
  if ((size_t)token->length != len)
    return false;
  return memcmp(token->start, text, len) == 0;
}

void errorAtToken(Token *token, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

void errorAtTokenFmt(Token *token, const char *fmt, ...) {
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
} TypeEnvBinding;

static void bindingArrayWrite(TypeEnvBinding **array, int *count, int *capacity,
                              Token name, Type *type) {
  if (*capacity < *count + 1) {
    *capacity = *capacity < 8 ? 8 : *capacity * 2;
    *array =
        (TypeEnvBinding *)realloc(*array, sizeof(TypeEnvBinding) * (*capacity));
    if (*array == NULL) {
      fprintf(stderr, "realloc failed in bindingArrayWrite\n");
      exit(1);
    }
  }
  (*array)[*count].name = name;
  (*array)[*count].type = type;
  (*count)++;
}

static Type *bindingArrayLookup(TypeEnvBinding *array, int count, Token name) {
  // Most recently declared binding wins
  for (int i = count - 1; i >= 0; i--) {
    if (tokensEqual(&array[i].name, &name))
      return array[i].type;
  }
  return NULL;
}

typedef struct {
  TypeEnvBinding *bindings;
  int count;
  int capacity;
} TypeEnvScope;

struct TypeEnv {
  // State

  TypeEnvScope *scopes;
  int scopeCount;
  int scopeCapacity;

  TypeEnvBinding *structs;
  int structCount;
  int structCapacity;

  TypeEnvBinding *functions;
  int functionCount;
  int functionCapacity;

  // WIP State

  Type *_selfType;          // NULL when not currently checking a method body.
  Type *_currentReturnType; // NULL when not checking a function/method body, or
                            // its return type didn't resolve.
};

// --------

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
    env->scopes = (TypeEnvScope *)realloc(env->scopes, sizeof(TypeEnvScope) *
                                                           env->scopeCapacity);
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
  TypeEnvScope *scope = &env->scopes[env->scopeCount - 1];
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
  env->_selfType = selfType;
}

Type *typeEnvGetSelfType(TypeEnv *env) { return env->_selfType; }

void typeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType) {
  env->_currentReturnType = returnType;
}

Type *typeEnvGetCurrentReturnType(TypeEnv *env) {
  return env->_currentReturnType;
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

// Infer the type of an expression
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
  if (node->kind == NODE_FUNCTION && node->as.function.isLambda)
    return checkOrInferLambda(env, node, expected) != NULL;

  if (node->kind == NODE_ARRAY && expected != NULL) {
    if (expected->kind != TYPE_ARRAY) {
      errorAtNodeFmt(node, "Expected %s, got an array.",
                     typeToString(expected));
      return false;
    }

    ArrayNode *a = &node->as.array;

    bool ok = true;

    for (int i = 0; i < a->count; i++) {
      AstNode *item = a->items[i];

      if (!check(env, item, expected->as.array.elementType))
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

// True if either: struct is generic (unsupported currently) or if struct
// members couldn't be type checked
static bool structMembersUnreliable(Type *type) {
  return typeStructIsGeneric(type) || typeStructHasUnresolvedMembers(type);
}

// Infer the type of a literal value expression
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

// Infer the type of a unary expressoin
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

// Infer the type of a binary expression
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

// Infer the type of an variable/identifier expression
static Type *inferVariable(TypeEnv *env, AstNode *node) {
  Token *name = &node->as.variable.name;
  Type *type = typeEnvLookup(env, *name);

  if (type == NULL)
    type = typeEnvLookupFunction(env, *name);

  if (type == NULL)
    return NULL;

  return type;
}

// Infer the type of an assignment expression
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

// Infer the type of a logic operator e.g. and, or
static Type *inferLogical(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical;
  Type *leftType = infer(env, l->left);
  Type *rightType = infer(env, l->right);

  if (leftType == NULL || rightType == NULL)
    return NULL;

  if (!typesEqual(leftType, rightType)) {
    errorAtNodeFmt(node,
                   "Both sides of '%s' must be the same type, got %s and %s",
                   node->kind == NODE_AND ? "and" : "or",
                   typeToString(leftType), typeToString(rightType));
    return NULL;
  }

  return leftType;
}

// Infer the type of a nullish expression
static Type *inferNullish(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical; // reuses LogicalNode, like compiler.c
  infer(env, l->left); // unconstrained -- no Option[T] to check against yet
  return infer(env, l->right); // result type comes from the fallback
}

// Infer the type of a call expression
static Type *inferCall(TypeEnv *env, AstNode *node) {
  CallNode *c = &node->as.call;

  if (c->callee->kind == NODE_VARIABLE) {
    Token *name = &c->callee->as.variable.name;
    Type *calleeType = typeEnvLookup(env, *name);

    if (calleeType == NULL)
      calleeType = typeEnvLookupFunction(env, *name);

    if (calleeType == NULL) {
      // Unable to resolve called function (possible a native function)
      // Run infer over args to report any type errors they might contain
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

// Infer the type of a get expression
static Type *inferGet(TypeEnv *env, AstNode *node) {
  GetNode *get = &node->as.get;

  if (get->object->kind == NODE_VARIABLE) {
    Token *objIdentifier = &get->object->as.variable.name;

    bool shadowed = typeEnvLookup(env, *objIdentifier) != NULL ||
                    typeEnvLookupFunction(env, *objIdentifier) != NULL;

    if (!shadowed) {
      Type *structType = typeEnvLookupStruct(env, *objIdentifier);

      if (structType != NULL) {
        if (structMembersUnreliable(structType))
          return NULL;

        Type *methodType = typeStructStaticMethodLookup(structType, get->name);

        if (methodType == NULL) {
          errorAtTokenFmt(&get->name, "%s has no static method '%.*s'.",
                          typeToString(structType), get->name.length,
                          get->name.start);
          return NULL;
        }

        return methodType;
      }
    }
  }

  // Attempt to infer the object's type
  Type *objectType = infer(env, get->object);

  // No type found, bail
  if (objectType == NULL)
    return NULL;

  // The object's type isn't a struct
  if (objectType->kind != TYPE_STRUCT) {
    errorAtTokenFmt(&get->name, "Can't access '.%.*s' on a %s.",
                    get->name.length, get->name.start,
                    typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (structMembersUnreliable(objectType))
    return NULL; // already reported once

  // Look up struct fields first
  Type *fieldType = typeStructFieldLookup(objectType, get->name);
  if (fieldType != NULL)
    return fieldType;

  // Then look up struct methods
  Type *methodType = typeStructInstanceMethodLookup(objectType, get->name);
  if (methodType != NULL)
    return methodType;

  // Then check for accidental static method access
  Type *staticMethodType = typeStructStaticMethodLookup(objectType, get->name);
  if (staticMethodType != NULL) {
    errorAtTokenFmt(&get->name,
                    "'%.*s' is a static method. Access it on '%.*s' "
                    "instead of an instance.",
                    get->name.length, get->name.start,
                    objectType->as.struct_.name.length,
                    objectType->as.struct_.name.start);
    return NULL;
  }

  // Report error if nothing was found
  errorAtTokenFmt(&get->name, "%s has no field or method '%.*s'.",
                  typeToString(objectType), get->name.length, get->name.start);
  return NULL;
}

// Infer the type of a set expression
static Type *inferSet(TypeEnv *env, AstNode *node) {
  SetNode *s = &node->as.set;
  Type *objectType = infer(env, s->object);

  if (objectType == NULL) {
    infer(env, s->value); // still walk for internal errors
    return NULL;
  }

  // Check attempts to set fields on a none struct
  if (objectType->kind != TYPE_STRUCT) {
    errorAtTokenFmt(&s->name, "Can't set '.%.*s' on a %s.", s->name.length,
                    s->name.start, typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (structMembersUnreliable(objectType)) {
    infer(env, s->value); // still walk for internal errors
    return NULL;
  }

  // Check struct for field
  Type *fieldType = typeStructFieldLookup(objectType, s->name);

  // Struct has no field by name
  if (fieldType == NULL) {
    errorAtTokenFmt(&s->name, "%s has no field '%.*s'.",
                    typeToString(objectType), s->name.length, s->name.start);
    return NULL;
  }

  // Type check value being set matches struct field's type
  if (!check(env, s->value, fieldType))
    return NULL;

  return fieldType;
}

// Infer the type of the self parameter of a method
static Type *inferSelfNode(TypeEnv *env, AstNode *node) {
  // Returns null if not inside a method
  Type *selfType = typeEnvGetSelfType(env);

  if (selfType == NULL) {
    errorAtNode(node, "'self' isn't valid here.");
    return NULL;
  }

  return selfType;
}

// Infer the type of an index access get expression
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

  return objectType->as.array.elementType;
}

// Infer the type of an index access set expression
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

// Infer the type of a struct initialization expression
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

  // Missing required fields are a separate runtime-level check, not
  // this pass's concern.
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

// Infer the type of an if expression
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
    // No trailing value -- always unit. A declared return type can
    // still be satisfied via explicit `return`s; verifying every path
    // does so isn't attempted here.
    result = typeUnit();
  }

  typeEnvEndScope(env);
  return result;
}

// Infer the type of a block expression
static Type *inferBlock(TypeEnv *env, AstNode *node) {
  return checkBlockContents(env, &node->as.block, /*expectedValueType=*/NULL);
}

static Type *checkOrInferLambda(
    TypeEnv *env, AstNode *node,
    Type *expected // expected is NULL in infer() context (every param needs an
                   // explicit type) or a TYPE_FN in check() context (untyped
                   // params take their type from the matching position). Shared
                   // by infer()'s NODE_FUNCTION case and check()'s lambda
                   // special case.
) {
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
  VarDeclNode *varDecl = &node->as.varDecl;
  bool hasExpectedType = varDecl->declaredType != NULL;
  Type *declaredType =
      hasExpectedType ? resolveType(env, varDecl->declaredType) : NULL;

  if (varDecl->initializer != NULL) {
    if (declaredType != NULL) {
      check(env, varDecl->initializer, declaredType);
      typeEnvDeclare(env, varDecl->name, declaredType);
    } else {
      Type *inferred = infer(env, varDecl->initializer);
      typeEnvDeclare(env, varDecl->name, inferred);
    }
  } else {
    if (declaredType == NULL) {
      errorAtTokenFmt(&varDecl->name,
                      "'%.*s' needs a type -- it has no initializer to "
                      "infer one from.",
                      varDecl->name.length, varDecl->name.start);
    }

    typeEnvDeclare(env, varDecl->name, declaredType);
  }
}

// Checks a function/method body against an already-resolved signature.
// Callers handle registration differently (local fn vs. hoisted
// top-level/impl method), so that's not redone here.
static void checkFunctionBody(TypeEnv *env, FunctionNode *fn, Type **paramTypes,
                              Type *returnType,
                              Type *selfType // NULL for a non-method function
) {
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
  bool hasArity = fn->arity > 0;

  Type **paramTypes =
      hasArity ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;

  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = (fn->paramTypes != NULL && fn->paramTypes[i] != NULL)
                        ? resolveType(env, fn->paramTypes[i])
                        : NULL;
  }

  Type *returnType =
      fn->returnType != NULL ? resolveType(env, fn->returnType) : NULL;

  Type *fnType = typeFunction(paramTypes, fn->arity, returnType);
  typeEnvDeclare(env, fn->name, fnType);

  Type *selfType = typeEnvGetSelfType(env);

  checkFunctionBody(env, fn, paramTypes, returnType, selfType);
}

void checkStmt(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    infer(env, node->as.exprStmt.expr);
    break;
  case NODE_PRINT:
    infer(env, node->as.print.expr);
    break;
  case NODE_VAR_DECL:
    checkVarDecl(env, node);
    break;
  case NODE_WHILE: {
    WhileNode *w = &node->as.while_;
    check(env, w->condition, typeBool());
    checkStmt(env, w->body);
    break;
  }
  case NODE_FOR: {
    ForNode *f = &node->as.for_;
    typeEnvBeginScope(env);

    if (f->init != NULL)
      checkStmt(env, f->init);

    if (f->condition != NULL)
      check(env, f->condition, typeBool());

    checkStmt(env, f->body);

    if (f->increment != NULL)
      infer(env, f->increment);

    typeEnvEndScope(env);
    break;
  }
  case NODE_IF: {
    IfNode *if_ = &node->as.if_;

    check(env, if_->condition, typeBool());
    checkStmt(env, if_->thenBranch);

    if (if_->elseBranch != NULL)
      checkStmt(env, if_->elseBranch);

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
    break;
  default:
    infer(env, node);
    break;
  }
}

// --- whole-program driver ---

// Resolves a function/method signature, requiring every param + the
// return type to have an annotation. self is excluded -- its type is
// always just "this struct," bound separately via selfType.
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

static void resolveStructFields(TypeEnv *env, AstNode *node) {
  StructNode *struct_ = &node->as.struct_;
  Type *structType = typeEnvLookupStruct(env, struct_->name);

  if (structType == NULL)
    return;

  if (typeStructIsGeneric(structType))
    return; // already reported once at declaration; don't cascade

  TypeMember *fields = struct_->fieldCount > 0
                           ? (TypeMember *)typesAllocRaw(struct_->fieldCount *
                                                         sizeof(TypeMember))
                           : NULL;

  bool ok = true;
  for (int i = 0; i < struct_->fieldCount; i++) {
    VarDeclNode *field = &struct_->fields[i];

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

  if (ok) {
    typeStructSetFields(structType, fields, struct_->fieldCount);
  } else {
    typeStructMarkUnresolvedMembers(structType);
  }
}

static void registerImplMethods(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  Type *structType = typeEnvLookupStruct(env, impl->name);

  if (structType == NULL) {
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

static void registerTopLevelFunctionSignature(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = resolveFunctionSignature(env, fn);

  if (fnType != NULL) {
    typeEnvRegisterFunction(env, fn->name, fnType);
  }
}

static void checkTopLevelFunctionBody(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = typeEnvLookupFunction(env, fn->name);
  if (fnType == NULL)
    return; // signature failed to resolve in Pass D; already reported
  checkFunctionBody(env, fn, fnType->as.function.paramTypes,
                    fnType->as.function.returnType, NULL);
}

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
      continue; // signature or struct failed to resolve; already reported

    Type *selfType = method->hasSelf ? structType : NULL;
    checkFunctionBody(env, method, methodType->as.function.paramTypes,
                      methodType->as.function.returnType, selfType);
  }
}

bool typecheckProgram(AstNode **program, int count) {
  TypeEnv *env = typeEnvCreate();
  typeEnvBeginScope(env);

  // Structs

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

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      resolveStructFields(env, program[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      registerImplMethods(env, program[i]);
    }
  }

  // Hoisted Functions

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_FUNCTION) {
      registerTopLevelFunctionSignature(env, program[i]);
    }
  }

  // Definite Assignment Analysis

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

  // Clean Up

  typeEnvEndScope(env);
  bool ok = !typecheckHadError();
  typeEnvDestroy(env);

  return ok;
}
