#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definite_assignment.h"
#include "native_signatures.h"
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

static bool tokenIsPrimitiveTypeName(Token *token) {
  return tokenTextEquals(token, "unit") || tokenTextEquals(token, "bool") ||
         tokenTextEquals(token, "string") || tokenTextEquals(token, "f64");
}

static Token makeTokenFromCString(const char *text) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0;
  return token;
}

static void typchkTypeEnvDefineBuiltinTraits(TypeEnv *env);

void typchkErrorAtToken(Token *token, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type != TOKEN_ERROR) {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
}

void typchkErrorAtTokenFmt(Token *token, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  typchkErrorAtToken(token, message);
}

static void typchkErrorAtNode(AstNode *node, const char *message) {
  hadError = true;
  fprintf(stderr, "[line %d] Error: %s\n", node->line, message);
}

static void typchkErrorAtNodeFmt(AstNode *node, const char *fmt, ...) {
  char message[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  typchkErrorAtNode(node, message);
}

bool typchkHadError(void) { return hadError; }
void typchkResetError(void) { hadError = false; }

typedef struct {
  InternedName name;
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
  (*array)[*count].name = internTokenName(name);
  (*array)[*count].type = type;
  (*count)++;
}

static Type *bindingArrayLookup(TypeEnvBinding *array, int count, Token name) {
  // Most recently declared binding wins
  for (int i = count - 1; i >= 0; i--) {
    if (internedNameEqualsToken(array[i].name, name))
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

  TypeEnvBinding *aliases;
  int aliasCount;
  int aliasCapacity;

  TypeEnvBinding *traits;
  int traitCount;
  int traitCapacity;

  // WIP State

  Type *_selfType;          // NULL when not currently checking a method body.
  Type *_currentReturnType; // NULL when not checking a function/method body, or
                            // its return type didn't resolve.
  Type *_currentImplTargetType; // NULL when not checking an impl block
};

// Type environment that persists across compilation units
static TypeEnv *sessionEnv = NULL;

void typchkSessionBegin(void) {
  if (sessionEnv != NULL)
    return;
  sessionEnv = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(sessionEnv);
}

void typchkSessionEnd(void) {
  if (sessionEnv == NULL)
    return;

  typchkTypeEnvEndScope(sessionEnv);
  typchkTypeEnvDestroy(sessionEnv);

  sessionEnv = NULL;

  typesFreeAll();
}

TypeEnv *typchkTypeEnvCreate(void) {
  TypeEnv *env = (TypeEnv *)malloc(sizeof(TypeEnv));
  memset(env, 0, sizeof(TypeEnv));
  defineAllNativeSignatures(env);
  typchkTypeEnvDefineBuiltinTraits(env);
  return env;
}

void typchkTypeEnvDestroy(TypeEnv *env) {
  for (int i = 0; i < env->scopeCount; i++) {
    free(env->scopes[i].bindings);
  }
  free(env->scopes);
  free(env->structs);
  free(env->functions);
  free(env->aliases);
  free(env->traits);
  free(env);
}

void typchkTypeEnvBeginScope(TypeEnv *env) {
  if (env->scopeCapacity < env->scopeCount + 1) {
    env->scopeCapacity = env->scopeCapacity < 8 ? 8 : env->scopeCapacity * 2;
    env->scopes = (TypeEnvScope *)realloc(env->scopes, sizeof(TypeEnvScope) *
                                                           env->scopeCapacity);
    if (env->scopes == NULL) {
      fprintf(stderr, "realloc failed in typchkTypeEnvBeginScope\n");
      exit(1);
    }
  }
  env->scopes[env->scopeCount].bindings = NULL;
  env->scopes[env->scopeCount].count = 0;
  env->scopes[env->scopeCount].capacity = 0;
  env->scopeCount++;
}

void typchkTypeEnvEndScope(TypeEnv *env) {
  env->scopeCount--;
  free(env->scopes[env->scopeCount].bindings);
  env->scopes[env->scopeCount].bindings = NULL;
  env->scopes[env->scopeCount].count = 0;
  env->scopes[env->scopeCount].capacity = 0;
}

void typchkTypeEnvDeclare(TypeEnv *env, Token name, Type *type) {
  TypeEnvScope *scope = &env->scopes[env->scopeCount - 1];

  bindingArrayWrite(&scope->bindings, &scope->count, &scope->capacity, name,
                    type);
}

Type *typchkTypeEnvLookup(TypeEnv *env, Token name) {
  for (int i = env->scopeCount - 1; i >= 0; i--) {
    Type *found =
        bindingArrayLookup(env->scopes[i].bindings, env->scopes[i].count, name);
    if (found != NULL)
      return found;
  }
  return NULL;
}

void typchkTypeEnvRegisterStruct(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->structs, &env->structCount, &env->structCapacity,
                    name, type);
}

Type *typchkTypeEnvLookupStruct(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->structs, env->structCount, name);
}

void typchkTypeEnvRegisterFunction(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->functions, &env->functionCount,
                    &env->functionCapacity, name, type);
}

Type *typchkTypeEnvLookupFunction(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->functions, env->functionCount, name);
}

void typchkTypeEnvRegisterAlias(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->aliases, &env->aliasCount, &env->aliasCapacity, name,
                    type);
}

Type *typchkTypeEnvLookupAlias(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->aliases, env->aliasCount, name);
}

void typchkTypeEnvRegisterTrait(TypeEnv *env, Token name, Type *type) {
  bindingArrayWrite(&env->traits, &env->traitCount, &env->traitCapacity, name,
                    type);
}

Type *typchkTypeEnvLookupTrait(TypeEnv *env, Token name) {
  return bindingArrayLookup(env->traits, env->traitCount, name);
}

/**
 * Define the builtin traits
 *
 * Display, Eq, Ord (a supertrait of Eq), and Default
 */
static void typchkTypeEnvDefineBuiltinTraits(TypeEnv *env) {
  // Display

  UninternedTypeMember displayInstance[] = {
      {makeTokenFromCString("toString"), typeFunction(NULL, 0, typeString())},
  };
  Type *display =
      typeTrait(makeTokenFromCString("Display"), NULL, 0, displayInstance, 1);
  typchkTypeEnvRegisterTrait(env, makeTokenFromCString("Display"), display);

  // Eq

  Type **equalsParams = (Type **)typesAllocRaw(sizeof(Type *));
  equalsParams[0] = typeSelfPlaceholder();
  UninternedTypeMember eqInstance[] = {
      {makeTokenFromCString("equals"),
       typeFunction(equalsParams, 1, typeBool())},
  };
  Type *eq = typeTrait(makeTokenFromCString("Eq"), NULL, 0, eqInstance, 1);
  typchkTypeEnvRegisterTrait(env, makeTokenFromCString("Eq"), eq);
  Type **cmpParams = (Type **)typesAllocRaw(sizeof(Type *));
  cmpParams[0] = typeSelfPlaceholder();
  UninternedTypeMember ordInstance[] = {
      {makeTokenFromCString("cmp"), typeFunction(cmpParams, 1, typeF64())},
  };

  // Ord

  Type *ord = typeTrait(makeTokenFromCString("Ord"), NULL, 0, ordInstance, 1);
  typeTraitSetSupertrait(ord, eq->as.trait_.name);
  typchkTypeEnvRegisterTrait(env, makeTokenFromCString("Ord"), ord);

  // Default

  UninternedTypeMember defaultStatic[] = {
      {makeTokenFromCString("default"),
       typeFunction(NULL, 0, typeSelfPlaceholder())},
  };
  Type *default_ =
      typeTrait(makeTokenFromCString("Default"), defaultStatic, 1, NULL, 0);
  typchkTypeEnvRegisterTrait(env, makeTokenFromCString("Default"), default_);
}

void typchkTypeEnvSetSelfType(TypeEnv *env, Type *selfType) {
  env->_selfType = selfType;
}

Type *typchkTypeEnvGetSelfType(TypeEnv *env) { return env->_selfType; }

void typchkTypeEnvSetCurrentReturnType(TypeEnv *env, Type *returnType) {
  env->_currentReturnType = returnType;
}

Type *typchkTypeEnvGetCurrentReturnType(TypeEnv *env) {
  return env->_currentReturnType;
}

void typchkTypeEnvSetImplTargetType(TypeEnv *env, Type *implTargetType) {
  env->_currentImplTargetType = implTargetType;
}

Type *typchkTypeEnvGetImplTargetType(TypeEnv *env) {
  return env->_currentImplTargetType;
}

Type *typchkResolveType(TypeEnv *env, AstNode *typeAnnotation) {
  if (typeAnnotation->kind == NODE_TYPE_FUNCTION) {
    TypeFunctionNode *fn = &typeAnnotation->as.typeFunction;

    Type **paramTypes = NULL;
    if (fn->paramCount > 0) {
      paramTypes = (Type **)typesAllocRaw(fn->paramCount * sizeof(Type *));
      for (int i = 0; i < fn->paramCount; i++) {
        Type *paramType = typchkResolveType(env, fn->paramTypes[i]);
        if (paramType == NULL)
          return NULL; // error already reported below the recursive call
        paramTypes[i] = paramType;
      }
    }

    Type *returnType = typchkResolveType(env, fn->returnType);
    if (returnType == NULL)
      return NULL;

    return typeFunction(paramTypes, fn->paramCount, returnType);
  }

  TypeNode *t = &typeAnnotation->as.type_;

  if (t->genericArgCount > 0) {
    typchkErrorAtToken(&t->name, "Generic types aren't supported yet.");
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
  if (tokenTextEquals(&t->name, "Self")) {
    return env->_currentImplTargetType != NULL ? env->_currentImplTargetType
                                               : typeSelfPlaceholder();
  }

  Type *structType = typchkTypeEnvLookupStruct(env, t->name);
  if (structType != NULL)
    return structType;

  Type *aliasType = typchkTypeEnvLookupAlias(env, t->name);
  if (aliasType != NULL)
    return aliasType;

  typchkErrorAtToken(&t->name, "Unknown type.");
  return NULL;
}

static Type *typchkInferLiteral(AstNode *node);
static Type *typchkInferUnary(TypeEnv *env, AstNode *node);
static Type *typchkInferBinary(TypeEnv *env, AstNode *node);
static Type *typchkInferVariable(TypeEnv *env, AstNode *node);
static Type *typchkInferAssign(TypeEnv *env, AstNode *node);
static Type *typchkInferLogical(TypeEnv *env, AstNode *node);
static Type *typchkInferNullish(TypeEnv *env, AstNode *node);
static Type *typchkInferCall(TypeEnv *env, AstNode *node);
static Type *typchkCheckCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                                Type *calleeType);
static Type *typchkInferGet(TypeEnv *env, AstNode *node);
static Type *typchkInferSet(TypeEnv *env, AstNode *node);
static Type *typchkInferSelf(TypeEnv *env, AstNode *node);
static Type *typchkInferIndexGet(TypeEnv *env, AstNode *node);
static Type *typchkInferIndexSet(TypeEnv *env, AstNode *node);
static Type *typchkInferStructInit(TypeEnv *env, AstNode *node);
static Type *typchkInferArray(TypeEnv *env, AstNode *node);
static Type *typchkInferIf(TypeEnv *env, AstNode *node);
static Type *typchkInferBlock(TypeEnv *env, AstNode *node);
static Type *typchkCheckBlockContents(TypeEnv *env, BlockNode *block,
                                      Type *expectedValueType);
static Type *typchkCheckOrInferLambda(TypeEnv *env, AstNode *node,
                                      Type *expected);
static void typchkCheckVarDecl(TypeEnv *env, AstNode *node);
static void typchkCheckFunctionDecl(TypeEnv *env, AstNode *node);

// Infer the type of an expression
Type *typchkInfer(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_LITERAL:
    return typchkInferLiteral(node);
  case NODE_UNARY:
    return typchkInferUnary(env, node);
  case NODE_BINARY:
    return typchkInferBinary(env, node);
  case NODE_GROUPING:
    return typchkInfer(env, node->as.grouping.inner);
  case NODE_VARIABLE:
    return typchkInferVariable(env, node);
  case NODE_ASSIGN:
    return typchkInferAssign(env, node);
  case NODE_AND:
  case NODE_OR:
    return typchkInferLogical(env, node);
  case NODE_NULLISH:
    return typchkInferNullish(env, node);
  case NODE_CALL:
    return typchkInferCall(env, node);
  case NODE_GET:
    return typchkInferGet(env, node);
  case NODE_SET:
    return typchkInferSet(env, node);
  case NODE_SELF:
    return typchkInferSelf(env, node);
  case NODE_INDEX_GET:
    return typchkInferIndexGet(env, node);
  case NODE_INDEX_SET:
    return typchkInferIndexSet(env, node);
  case NODE_STRUCT_INIT:
    return typchkInferStructInit(env, node);
  case NODE_ARRAY:
    return typchkInferArray(env, node);
  case NODE_IF:
    return typchkInferIf(env, node);
  case NODE_BLOCK:
    return typchkInferBlock(env, node);
  case NODE_FUNCTION:
    if (node->as.function.isLambda)
      return typchkCheckOrInferLambda(env, node, NULL);
    typchkErrorAtNode(node, "Internal: unexpected function declaration in "
                            "expression position.");
    return NULL;
  default:
    typchkErrorAtNode(node, "Internal: this isn't a checkable expression.");
    return NULL;
  }
}

bool typchkCheck(TypeEnv *env, AstNode *node, Type *expected) {
  if (node->kind == NODE_FUNCTION && node->as.function.isLambda)
    return typchkCheckOrInferLambda(env, node, expected) != NULL;

  if (node->kind == NODE_ARRAY && expected != NULL) {
    if (expected->kind != TYPE_ARRAY) {
      typchkErrorAtNodeFmt(node, "Expected %s, got an array.",
                           typeToString(expected));
      return false;
    }

    ArrayNode *a = &node->as.array;

    bool ok = true;

    for (int i = 0; i < a->count; i++) {
      AstNode *item = a->items[i];

      if (!typchkCheck(env, item, expected->as.array.elementType))
        ok = false;
    }

    return ok;
  }

  Type *actual = typchkInfer(env, node);

  if (actual == NULL)
    return true;

  if (expected == NULL)
    return true;

  if (!typesEqual(actual, expected)) {
    typchkErrorAtNodeFmt(node, "Expected %s, got %s.", typeToString(expected),
                         typeToString(actual));
    return false;
  }

  return true;
}

// True if either: struct is generic (unsupported currently) or if struct
// members couldn't be type checked
static bool typchkStructMembersUnreliable(Type *type) {
  return typeStructIsGeneric(type) || typeStructHasUnresolvedMembers(type);
}

// Infer the type of a literal value expression
static Type *typchkInferLiteral(AstNode *node) {
  LiteralNode *lit = &node->as.literal;
  switch (lit->kind) {
  case LITERAL_NIL:
  case LITERAL_UNIT:
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
static Type *typchkInferUnary(TypeEnv *env, AstNode *node) {
  UnaryNode *u = &node->as.unary;

  if (u->op.type == TOKEN_BANG) {
    Type *operandType = typchkInfer(env, u->operand);

    if (operandType == NULL)
      return NULL;

    return typeBool();
  }

  if (!typchkCheck(env, u->operand, typeF64()))
    return NULL;

  return typeF64();
}

// Infer the type of a binary expression
static Type *typchkInferBinary(TypeEnv *env, AstNode *node) {
  BinaryNode *b = &node->as.binary;
  Type *leftType = typchkInfer(env, b->left);
  Type *rightType = typchkInfer(env, b->right);

  if (leftType == NULL || rightType == NULL)
    return NULL;

  switch (b->op.type) {
  case TOKEN_PLUS:
    if (typesEqual(leftType, typeF64()) && typesEqual(rightType, typeF64()))
      return typeF64();

    if (typesEqual(leftType, typeString()) &&
        typesEqual(rightType, typeString()))
      return typeString();

    typchkErrorAtTokenFmt(&b->op,
                          "'+' needs two f64s or two strings, got %s and %s.",
                          typeToString(leftType), typeToString(rightType));
    return NULL;

  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MODULO:
    if (!typesEqual(leftType, typeF64())) {
      typchkErrorAtNodeFmt(b->left, "Expected f64, got %s.",
                           typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      typchkErrorAtNodeFmt(b->right, "Expected f64, got %s.",
                           typeToString(rightType));
      return NULL;
    }
    return typeF64();

  case TOKEN_EQUAL_EQUAL:
  case TOKEN_BANG_EQUAL:
    if (!typesEqual(leftType, rightType)) {
      typchkErrorAtTokenFmt(&b->op,
                            "Both sides of '%s' must be the same type, got %s "
                            "and %s.",
                            b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=",
                            typeToString(leftType), typeToString(rightType));
      return NULL;
    }
    if (leftType->kind == TYPE_STRUCT &&
        !typeStructImplementsTrait(
            leftType, internTokenName(makeTokenFromCString("Eq")))) {
      typchkErrorAtTokenFmt(&b->op,
                            "%s needs 'impl Eq for %s' to support '%s'.",
                            typeToString(leftType), typeToString(leftType),
                            b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=");
      return NULL;
    }
    return typeBool();

  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    if (!typesEqual(leftType, typeF64())) {
      typchkErrorAtNodeFmt(b->left, "Expected f64, got %s.",
                           typeToString(leftType));
      return NULL;
    }
    if (!typesEqual(rightType, typeF64())) {
      typchkErrorAtNodeFmt(b->right, "Expected f64, got %s.",
                           typeToString(rightType));
      return NULL;
    }
    return typeBool();

  default:
    typchkErrorAtToken(&b->op, "Internal: unhandled binary operator.");
    return NULL;
  }
}

// Infer the type of an variable/identifier expression
static Type *typchkInferVariable(TypeEnv *env, AstNode *node) {
  Token *name = &node->as.variable.name;
  Type *type = typchkTypeEnvLookup(env, *name);

  if (type == NULL)
    type = typchkTypeEnvLookupFunction(env, *name);

  if (type == NULL)
    return NULL;

  return type;
}

// Infer the type of an assignment expression
static Type *typchkInferAssign(TypeEnv *env, AstNode *node) {
  AssignNode *a = &node->as.assign;
  Type *varType = typchkTypeEnvLookup(env, a->name);

  if (varType == NULL)
    varType = typchkTypeEnvLookupFunction(env, a->name);

  if (varType == NULL) {
    typchkInfer(env, a->value);
    return NULL;
  }

  if (!typchkCheck(env, a->value, varType))
    return NULL;

  return varType;
}

// Infer the type of a logic operator e.g. and, or
static Type *typchkInferLogical(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical;

  // Both operands are conditions, so both are bool and so is the result.
  // The VM still short circuits, it just can't yield a non-bool operand.
  bool ok = typchkCheck(env, l->left, typeBool());

  if (!typchkCheck(env, l->right, typeBool()))
    ok = false;

  return ok ? typeBool() : NULL;
}

// Infer the type of a nullish expression
static Type *typchkInferNullish(TypeEnv *env, AstNode *node) {
  LogicalNode *l = &node->as.logical; // reuses LogicalNode, like compiler.c
  typchkInfer(env,
              l->left); // unconstrained -- no Option[T] to check against yet
  return typchkInfer(env, l->right); // result type comes from the fallback
}

// Infer the type of a call expression
static Type *typchkInferCall(TypeEnv *env, AstNode *node) {
  CallNode *c = &node->as.call;

  if (c->callee->kind == NODE_VARIABLE) {
    Token *name = &c->callee->as.variable.name;
    Type *calleeType = typchkTypeEnvLookup(env, *name);

    if (calleeType == NULL)
      calleeType = typchkTypeEnvLookupFunction(env, *name);

    if (calleeType == NULL) {
      // Unable to resolve called function (possible a native function)
      // Run typchkInfer over args to report any type errors they might contain
      for (int i = 0; i < c->argCount; i++)
        typchkInfer(env, c->args[i]);

      return NULL;
    }

    return typchkCheckCallAgainstFunctionType(env, node, calleeType);
  }

  Type *calleeType = typchkInfer(env, c->callee);

  if (calleeType == NULL)
    return NULL;

  return typchkCheckCallAgainstFunctionType(env, node, calleeType);
}

static Type *typchkCheckCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                                Type *calleeType) {
  CallNode *c = &node->as.call;

  if (calleeType->kind != TYPE_FN) {
    typchkErrorAtTokenFmt(&c->paren, "%s isn't callable.",
                          typeToString(calleeType));
    return NULL;
  }
  if (c->argCount != calleeType->as.function.paramCount) {
    typchkErrorAtTokenFmt(&c->paren, "Expected %d argument(s), got %d.",
                          calleeType->as.function.paramCount, c->argCount);
    return NULL;
  }

  bool ok = true;
  for (int i = 0; i < c->argCount; i++) {
    if (!typchkCheck(env, c->args[i], calleeType->as.function.paramTypes[i]))
      ok = false;
  }
  if (!ok)
    return NULL;

  return calleeType->as.function.returnType;
}

// Infer the type of a get expression
static Type *typchkInferGet(TypeEnv *env, AstNode *node) {
  GetNode *get = &node->as.get;

  if (get->object->kind == NODE_VARIABLE) {
    Token *objIdentifier = &get->object->as.variable.name;

    bool shadowed = typchkTypeEnvLookup(env, *objIdentifier) != NULL ||
                    typchkTypeEnvLookupFunction(env, *objIdentifier) != NULL;

    if (!shadowed) {
      bool isSelf = tokenTextEquals(objIdentifier, "Self");
      Type *structType = isSelf
                             ? typchkTypeEnvGetImplTargetType(env)
                             : typchkTypeEnvLookupStruct(env, *objIdentifier);

      if (structType == NULL && isSelf) {
        typchkErrorAtTokenFmt(objIdentifier,
                              "'Self' can only be used inside an impl block.");
        return NULL;
      }

      if (structType != NULL) {
        if (typchkStructMembersUnreliable(structType))
          return NULL;

        Type *methodType = typeStructStaticMethodLookup(structType, get->name);

        if (methodType == NULL) {
          methodType = typeStructTraitStaticMethodLookup(structType, get->name);
        }

        if (methodType == NULL) {
          typchkErrorAtTokenFmt(&get->name, "%s has no static method '%.*s'.",
                                typeToString(structType), get->name.length,
                                get->name.start);
          return NULL;
        }

        return methodType;
      }
    }
  }

  // Attempt to infer the object's type
  Type *objectType = typchkInfer(env, get->object);

  // No type found, bail
  if (objectType == NULL)
    return NULL;

  // The object's type isn't a struct
  if (objectType->kind != TYPE_STRUCT) {
    typchkErrorAtTokenFmt(&get->name, "Can't access '.%.*s' on a %s.",
                          get->name.length, get->name.start,
                          typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (typchkStructMembersUnreliable(objectType))
    return NULL; // already reported once

  // Look up struct fields first
  Type *fieldType = typeStructFieldLookup(objectType, get->name);
  if (fieldType != NULL)
    return fieldType;

  // Then look up struct methods
  Type *methodType = typeStructInstanceMethodLookup(objectType, get->name);
  if (methodType != NULL)
    return methodType;

  // Then look up methods that came from an `impl Trait for X` block
  Type *traitMethodType =
      typeStructTraitInstanceMethodLookup(objectType, get->name);
  if (traitMethodType != NULL)
    return traitMethodType;

  // Then check for accidental static method access
  Type *staticMethodType = typeStructStaticMethodLookup(objectType, get->name);
  if (staticMethodType == NULL) {
    staticMethodType = typeStructTraitStaticMethodLookup(objectType, get->name);
  }
  if (staticMethodType != NULL) {
    typchkErrorAtTokenFmt(&get->name,
                          "'%.*s' is a static method. Access it on '%.*s' "
                          "instead of an instance.",
                          get->name.length, get->name.start,
                          objectType->as.struct_.name.length,
                          internedNameChars(objectType->as.struct_.name));
    return NULL;
  }

  // Report error if nothing was found
  typchkErrorAtTokenFmt(&get->name, "%s has no field or method '%.*s'.",
                        typeToString(objectType), get->name.length,
                        get->name.start);
  return NULL;
}

// Infer the type of a set expression
static Type *typchkInferSet(TypeEnv *env, AstNode *node) {
  SetNode *s = &node->as.set;
  Type *objectType = typchkInfer(env, s->object);

  if (objectType == NULL) {
    typchkInfer(env, s->value); // still walk for internal errors
    return NULL;
  }

  // Check attempts to set fields on a none struct
  if (objectType->kind != TYPE_STRUCT) {
    typchkErrorAtTokenFmt(&s->name, "Can't set '.%.*s' on a %s.",
                          s->name.length, s->name.start,
                          typeToString(objectType));
    return NULL;
  }

  // There was an error performing typechecking on object
  if (typchkStructMembersUnreliable(objectType)) {
    typchkInfer(env, s->value); // still walk for internal errors
    return NULL;
  }

  // Check struct for field
  Type *fieldType = typeStructFieldLookup(objectType, s->name);

  // Struct has no field by name
  if (fieldType == NULL) {
    typchkErrorAtTokenFmt(&s->name, "%s has no field '%.*s'.",
                          typeToString(objectType), s->name.length,
                          s->name.start);
    return NULL;
  }

  // Type check value being set matches struct field's type
  if (!typchkCheck(env, s->value, fieldType))
    return NULL;

  return fieldType;
}

// Infer the type of the self parameter of a method
static Type *typchkInferSelf(TypeEnv *env, AstNode *node) {
  // Returns null if not inside a method
  Type *selfType = typchkTypeEnvGetSelfType(env);

  if (selfType == NULL) {
    typchkErrorAtNode(node, "'self' isn't valid here.");
    return NULL;
  }

  return selfType;
}

// Infer the type of an index access get expression
static Type *typchkInferIndexGet(TypeEnv *env, AstNode *node) {
  IndexGetNode *ig = &node->as.indexGet;
  Type *objectType = typchkInfer(env, ig->object);

  if (objectType == NULL) {
    typchkInfer(env, ig->index);
    return NULL;
  }

  if (objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&ig->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!typchkCheck(env, ig->index, typeF64()))
    return NULL;

  return objectType->as.array.elementType;
}

// Infer the type of an index access set expression
static Type *typchkInferIndexSet(TypeEnv *env, AstNode *node) {
  IndexSetNode *is = &node->as.indexSet;
  Type *objectType = typchkInfer(env, is->object);

  if (objectType == NULL) {
    typchkInfer(env, is->index);
    typchkInfer(env, is->value);
    return NULL;
  }

  if (objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&is->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!typchkCheck(env, is->index, typeF64()))
    return NULL;

  Type *elementType = objectType->as.array.elementType;
  if (elementType != NULL) {
    if (!typchkCheck(env, is->value, elementType))
      return NULL;
  } else {
    typchkInfer(env, is->value); // nothing to check against yet (empty-array
                                 // case), still walk for internal errors
  }

  return elementType;
}

// Infer the type of a struct initialization expression
static Type *typchkInferStructInit(TypeEnv *env, AstNode *node) {
  StructInitNode *si = &node->as.structInit;

  Type *structType;
  if (tokenTextEquals(&si->name, "Self")) {
    structType = typchkTypeEnvGetImplTargetType(env);
    if (structType == NULL) {
      typchkErrorAtTokenFmt(&si->name,
                            "'Self' can only be used inside an impl block.");
      return NULL;
    }
  } else {
    structType = typchkTypeEnvLookupStruct(env, si->name);
  }

  if (structType == NULL) {
    typchkErrorAtTokenFmt(&si->name, "Unknown struct '%.*s'.", si->name.length,
                          si->name.start);
    return NULL;
  }

  if (typchkStructMembersUnreliable(structType)) {
    for (int i = 0; i < si->fieldCount; i++) {
      typchkInfer(env, si->fields[i].value); // still walk for internal errors
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
      typchkErrorAtTokenFmt(&field->name, "%s has no field '%.*s'.",
                            typeToString(structType), field->name.length,
                            field->name.start);
      ok = false;
      continue;
    }
    if (!typchkCheck(env, field->value, fieldType))
      ok = false;
  }

  if (!ok)
    return NULL;

  return structType;
}

static Type *typchkInferArray(TypeEnv *env, AstNode *node) {
  ArrayNode *a = &node->as.array;
  if (a->count == 0)
    return typeArray(NULL); // nothing to learn an element type from yet

  Type *elementType = typchkInfer(env, a->items[0]);
  bool ok = true;
  for (int i = 1; i < a->count; i++) {
    if (elementType != NULL) {
      if (!typchkCheck(env, a->items[i], elementType))
        ok = false;
    } else {
      typchkInfer(env, a->items[i]); // still walk for internal errors
    }
  }
  if (!ok)
    return NULL;
  return typeArray(elementType);
}

// Infer the type of an if expression
static Type *typchkInferIf(TypeEnv *env, AstNode *node) {
  IfNode *i = &node->as.if_;
  typchkCheck(env, i->condition,
              typeBool()); // reported if wrong; still proceed

  Type *thenType = typchkInfer(env, i->thenBranch);
  Type *elseType =
      i->elseBranch != NULL ? typchkInfer(env, i->elseBranch) : typeUnit();

  if (thenType == NULL || elseType == NULL)
    return NULL;

  if (!typesEqual(thenType, elseType)) {
    typchkErrorAtNodeFmt(node,
                         "if/else branches must produce the same type, got %s "
                         "and %s.",
                         typeToString(thenType), typeToString(elseType));
    return NULL;
  }

  return thenType;
}

static Type *typchkCheckBlockContents(TypeEnv *env, BlockNode *block,
                                      Type *expectedValueType) {
  typchkTypeEnvBeginScope(env);

  for (int i = 0; i < block->count; i++) {
    typchkCheckStmt(env, block->stmts[i]);
  }

  Type *result;
  if (block->value != NULL) {
    if (expectedValueType != NULL) {
      result = typchkCheck(env, block->value, expectedValueType)
                   ? expectedValueType
                   : NULL;
    } else {
      result = typchkInfer(env, block->value);
    }
  } else {
    // No trailing value -- always unit. A declared return type can
    // still be satisfied via explicit `return`s; verifying every path
    // does so isn't attempted here.
    result = typeUnit();
  }

  typchkTypeEnvEndScope(env);
  return result;
}

// Infer the type of a block expression
static Type *typchkInferBlock(TypeEnv *env, AstNode *node) {
  return typchkCheckBlockContents(env, &node->as.block,
                                  /*expectedValueType=*/NULL);
}

static bool typchkCheckIfBlockAlwaysReturns(BlockNode *block);

// Check if the statement (AstNode) exits it's enclosing function
static bool typchkCheckIfAlwaysReturns(AstNode *node) {
  if (node == NULL)
    return false;

  switch (node->kind) {
  case NODE_RETURN:
    return true;

  case NODE_BLOCK:
    return typchkCheckIfBlockAlwaysReturns(&node->as.block);

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    return if_->elseBranch != NULL &&
           typchkCheckIfAlwaysReturns(if_->thenBranch) &&
           typchkCheckIfAlwaysReturns(if_->elseBranch);
  }

  default:
    return false;
  }
}

static bool typchkCheckIfBlockAlwaysReturns(BlockNode *block) {
  // Implicit return
  if (block->value != NULL)
    return true;

  for (int i = 0; i < block->count; i++) {
    if (typchkCheckIfAlwaysReturns(block->stmts[i]))
      return true;
  }

  return false;
}

static bool typchkCheckIfBodyProducesDeclaredValue(FunctionNode *fn,
                                                   Type *returnType) {
  if (returnType == NULL || typesEqual(returnType, typeUnit()))
    return true;

  if (fn->exprBody != NULL)
    return true;

  return typchkCheckIfBlockAlwaysReturns(&fn->body);
}

static Type *typchkCheckOrInferLambda(
    TypeEnv *env, AstNode *node,
    Type *expected // expected is NULL in typchkInfer() context (every param
                   // needs an explicit type) or a TYPE_FN in typchkCheck()
                   // context (untyped params take their type from the matching
                   // position). Shared by typchkInfer()'s NODE_FUNCTION case
                   // and typchkCheck()'s lambda special case.
) {
  FunctionNode *fn = &node->as.function;

  if (expected != NULL && expected->kind != TYPE_FN) {
    typchkErrorAtNodeFmt(node, "Expected %s here, not a function.",
                         typeToString(expected));
    return NULL;
  }

  if (expected != NULL && expected->as.function.paramCount != fn->arity) {
    typchkErrorAtNodeFmt(node,
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
      paramTypes[i] = typchkResolveType(env, fn->paramTypes[i]);
      if (paramTypes[i] == NULL)
        ok = false;
    } else if (expected != NULL) {
      paramTypes[i] = expected->as.function.paramTypes[i];
    } else {
      typchkErrorAtNode(
          node, "Can't infer this lambda's parameter types without more "
                "context -- add explicit types, or use it somewhere "
                "its type is already known.");
      paramTypes[i] = NULL;
      ok = false;
    }
  }

  if (!ok)
    return NULL;

  Type *declaredReturnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;
  Type *targetReturnType =
      declaredReturnType != NULL
          ? declaredReturnType
          : (expected != NULL ? expected->as.function.returnType : NULL);

  typchkTypeEnvBeginScope(env);

  for (int i = 0; i < fn->arity; i++) {
    typchkTypeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousReturnType = typchkTypeEnvGetCurrentReturnType(env);
  typchkTypeEnvSetCurrentReturnType(env, targetReturnType);

  Type *bodyResultType;
  if (fn->exprBody != NULL) {
    bodyResultType = targetReturnType != NULL
                         ? (typchkCheck(env, fn->exprBody, targetReturnType)
                                ? targetReturnType
                                : NULL)
                         : typchkInfer(env, fn->exprBody);
  } else {
    bodyResultType = typchkCheckBlockContents(env, &fn->body, targetReturnType);
  }

  typchkTypeEnvSetCurrentReturnType(env, previousReturnType);
  typchkTypeEnvEndScope(env);

  if (!typchkCheckIfBodyProducesDeclaredValue(fn, targetReturnType)) {
    typchkErrorAtNodeFmt(node, "This lambda must return %s on every path.",
                         typeToString(targetReturnType));
    return NULL;
  }

  if (bodyResultType == NULL)
    return NULL;

  Type *actualReturnType =
      declaredReturnType != NULL ? declaredReturnType : bodyResultType;

  return typeFunction(paramTypes, fn->arity, actualReturnType);
}

static void typchkCheckVarDecl(TypeEnv *env, AstNode *node) {
  VarDeclNode *varDecl = &node->as.varDecl;
  bool hasExpectedType = varDecl->declaredType != NULL;
  Type *declaredType =
      hasExpectedType ? typchkResolveType(env, varDecl->declaredType) : NULL;

  if (varDecl->initializer != NULL) {
    if (declaredType != NULL) {
      typchkCheck(env, varDecl->initializer, declaredType);
      typchkTypeEnvDeclare(env, varDecl->name, declaredType);
    } else {
      Type *inferred = typchkInfer(env, varDecl->initializer);
      typchkTypeEnvDeclare(env, varDecl->name, inferred);
    }
  } else {
    if (declaredType == NULL) {
      typchkErrorAtTokenFmt(&varDecl->name,
                            "'%.*s' needs a type -- it has no initializer to "
                            "infer one from.",
                            varDecl->name.length, varDecl->name.start);
    }

    typchkTypeEnvDeclare(env, varDecl->name, declaredType);
  }
}

// Checks a function/method body against an already-resolved signature.
// Callers handle registration differently (local fn vs. hoisted
// top-level/impl method), so that's not redone here.
static void typchkCheckFunctionBody(TypeEnv *env, FunctionNode *fn,
                                    Type **paramTypes, Type *returnType,
                                    Type *selfType) {
  typchkTypeEnvBeginScope(env);
  for (int i = 0; i < fn->arity; i++) {
    typchkTypeEnvDeclare(env, fn->params[i], paramTypes[i]);
  }

  Type *previousSelfType = typchkTypeEnvGetSelfType(env);
  typchkTypeEnvSetSelfType(env, selfType);

  Type *previousReturnType = typchkTypeEnvGetCurrentReturnType(env);
  typchkTypeEnvSetCurrentReturnType(env, returnType);

  if (fn->exprBody != NULL) {
    if (returnType != NULL)
      typchkCheck(env, fn->exprBody, returnType);
    else
      typchkInfer(env, fn->exprBody);
  } else {
    typchkCheckBlockContents(env, &fn->body, returnType);
  }

  daaCheckFn(fn);

  if (!typchkCheckIfBodyProducesDeclaredValue(fn, returnType)) {
    typchkErrorAtTokenFmt(&fn->name, "'%.*s' must return %s on every path.",
                          fn->name.length, fn->name.start,
                          typeToString(returnType));
  }

  typchkTypeEnvSetCurrentReturnType(env, previousReturnType);
  typchkTypeEnvSetSelfType(env, previousSelfType);
  typchkTypeEnvEndScope(env);
}

static void typchkCheckFunctionDecl(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  bool hasArity = fn->arity > 0;

  Type **paramTypes =
      hasArity ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;

  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = (fn->paramTypes != NULL && fn->paramTypes[i] != NULL)
                        ? typchkResolveType(env, fn->paramTypes[i])
                        : NULL;
  }

  Type *returnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;

  Type *fnType = typeFunction(paramTypes, fn->arity, returnType);
  typchkTypeEnvDeclare(env, fn->name, fnType);

  Type *selfType = typchkTypeEnvGetSelfType(env);

  typchkCheckFunctionBody(env, fn, paramTypes, returnType, selfType);
}

void typchkCheckStmt(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    typchkInfer(env, node->as.exprStmt.expr);
    break;
  case NODE_PRINT:
    typchkInfer(env, node->as.print.expr);
    break;
  case NODE_VAR_DECL:
    typchkCheckVarDecl(env, node);
    break;
  case NODE_WHILE: {
    WhileNode *w = &node->as.while_;
    typchkCheck(env, w->condition, typeBool());
    typchkCheckStmt(env, w->body);
    break;
  }
  case NODE_FOR: {
    ForNode *f = &node->as.for_;
    typchkTypeEnvBeginScope(env);

    if (f->init != NULL)
      typchkCheckStmt(env, f->init);

    if (f->condition != NULL)
      typchkCheck(env, f->condition, typeBool());

    typchkCheckStmt(env, f->body);

    if (f->increment != NULL)
      typchkInfer(env, f->increment);

    typchkTypeEnvEndScope(env);
    break;
  }
  case NODE_IF: {
    IfNode *if_ = &node->as.if_;

    typchkCheck(env, if_->condition, typeBool());
    typchkCheckStmt(env, if_->thenBranch);

    if (if_->elseBranch != NULL)
      typchkCheckStmt(env, if_->elseBranch);

    break;
  }
  case NODE_RETURN: {
    ReturnNode *r = &node->as.return_;
    Type *expectedReturn = typchkTypeEnvGetCurrentReturnType(env);

    if (r->value != NULL) {
      if (expectedReturn != NULL)
        typchkCheck(env, r->value, expectedReturn);
      else
        typchkInfer(env, r->value);
    } else if (expectedReturn != NULL &&
               !typesEqual(expectedReturn, typeUnit())) {
      typchkErrorAtNodeFmt(node, "Expected a return value of type %s.",
                           typeToString(expectedReturn));
    }

    break;
  }
  case NODE_BREAK:
  case NODE_CONTINUE:
    break;
  case NODE_FUNCTION:
    typchkCheckFunctionDecl(env, node);
    break;
  case NODE_STRUCT:
  case NODE_IMPL:
  case NODE_TRAIT:
  case NODE_TYPE_ALIAS:
    break;
  default:
    typchkInfer(env, node);
    break;
  }
}

// Resolves a function/method signature, requiring every param + the
// return type to have an annotation. self is excluded -- its type is
// always just "this struct," bound separately via selfType.
static Type *typchkResolveFunctionSignature(TypeEnv *env, FunctionNode *fn) {
  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  bool ok = true;
  for (int i = 0; i < fn->arity; i++) {
    if (fn->paramTypes == NULL || fn->paramTypes[i] == NULL) {
      typchkErrorAtTokenFmt(&fn->params[i], "Parameter '%.*s' needs a type.",
                            fn->params[i].length, fn->params[i].start);
      ok = false;
      continue;
    }
    Type *paramType = typchkResolveType(env, fn->paramTypes[i]);
    if (paramType == NULL)
      ok = false;
    paramTypes[i] = paramType;
  }

  if (fn->returnType == NULL) {
    typchkErrorAtTokenFmt(&fn->name, "'%.*s' needs a return type.",
                          fn->name.length, fn->name.start);
    ok = false;
  }
  Type *returnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;

  if (!ok)
    return NULL;
  return typeFunction(paramTypes, fn->arity, returnType);
}

// A type alias declaration waiting to be resolved. Aliases may reference
// each other in any order, so they're collected first and resolved
// on demand, depth first.
typedef struct {
  AstNode *node;
  bool resolving;
  bool resolved;
} UnresolvedTypeAlias;

static void typchkResolvePendingAlias(TypeEnv *env,
                                      UnresolvedTypeAlias *unresolvedAlias,
                                      int count, int index);

// Resolves any alias `typeAnnotation` names before it is itself resolved,
// so an alias declared later in the file still works.
static void typchkResolveAliasDependencies(TypeEnv *env,
                                           UnresolvedTypeAlias *unresolvedAlias,
                                           int count, AstNode *typeAnnotation) {
  if (typeAnnotation == NULL)
    return;

  if (typeAnnotation->kind == NODE_TYPE_FUNCTION) {
    TypeFunctionNode *fn = &typeAnnotation->as.typeFunction;
    for (int i = 0; i < fn->paramCount; i++) {
      typchkResolveAliasDependencies(env, unresolvedAlias, count,
                                     fn->paramTypes[i]);
    }
    typchkResolveAliasDependencies(env, unresolvedAlias, count, fn->returnType);
    return;
  }

  TypeNode *t = &typeAnnotation->as.type_;

  for (int i = 0; i < t->genericArgCount; i++) {
    typchkResolveAliasDependencies(env, unresolvedAlias, count,
                                   t->genericArgs[i]);
  }

  for (int i = 0; i < count; i++) {
    if (tokensEqual(&unresolvedAlias[i].node->as.typeAlias.name, &t->name)) {
      typchkResolvePendingAlias(env, unresolvedAlias, count, i);
      return;
    }
  }
}

static void typchkResolvePendingAlias(TypeEnv *env,
                                      UnresolvedTypeAlias *unresolvedAlias,
                                      int count, int index) {
  UnresolvedTypeAlias *alias = &unresolvedAlias[index];
  TypeAliasNode *decl = &alias->node->as.typeAlias;

  if (alias->resolved)
    return;

  if (alias->resolving) {
    typchkErrorAtTokenFmt(&decl->name, "Type alias '%.*s' is circular.",
                          decl->name.length, decl->name.start);
    alias->resolved = true;
    return;
  }

  alias->resolving = true;
  typchkResolveAliasDependencies(env, unresolvedAlias, count, decl->target);
  alias->resolving = false;
  alias->resolved = true;

  Type *target = typchkResolveType(env, decl->target);

  if (target != NULL)
    typchkTypeEnvRegisterAlias(env, decl->name, target);
}

static void typchkResolveStructFields(TypeEnv *env, AstNode *node) {
  StructNode *struct_ = &node->as.struct_;
  Type *structType = typchkTypeEnvLookupStruct(env, struct_->name);

  if (structType == NULL)
    return;

  if (typeStructIsGeneric(structType))
    return; // already reported once at declaration; don't cascade

  UninternedTypeMember *fields =
      struct_->fieldCount > 0
          ? (UninternedTypeMember *)typesAllocRaw(struct_->fieldCount *
                                                  sizeof(UninternedTypeMember))
          : NULL;

  bool ok = true;
  for (int i = 0; i < struct_->fieldCount; i++) {
    VarDeclNode *field = &struct_->fields[i];

    if (field->declaredType == NULL) {
      typchkErrorAtTokenFmt(&field->name, "Field '%.*s' needs a type.",
                            field->name.length, field->name.start);
      ok = false;
      continue;
    }

    Type *fieldType = typchkResolveType(env, field->declaredType);

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

static void typchkResolveTraitMethods(TypeEnv *env, AstNode *node) {
  TraitNode *trait_ = &node->as.trait_;
  Type *traitType = typchkTypeEnvLookupTrait(env, trait_->name);

  if (traitType == NULL)
    return;

  bool ok = true;

  if (trait_->hasSupertrait) {
    Type *supertraitType = typchkTypeEnvLookupTrait(env, trait_->supertrait);

    if (supertraitType == NULL) {
      typchkErrorAtTokenFmt(&trait_->supertrait, "Unknown trait '%.*s'.",
                            trait_->supertrait.length,
                            trait_->supertrait.start);
      ok = false;
    } else {
      typeTraitSetSupertrait(traitType, supertraitType->as.trait_.name);
    }
  }

  int staticCount = 0, instanceCount = 0;
  for (int i = 0; i < trait_->methodCount; i++) {
    if (trait_->methods[i]->hasSelf)
      instanceCount++;
    else
      staticCount++;
  }

  UninternedTypeMember *staticMethods =
      staticCount > 0 ? (UninternedTypeMember *)typesAllocRaw(
                            staticCount * sizeof(UninternedTypeMember))
                      : NULL;
  UninternedTypeMember *instanceMethods =
      instanceCount > 0 ? (UninternedTypeMember *)typesAllocRaw(
                              instanceCount * sizeof(UninternedTypeMember))
                        : NULL;

  int staticIndex = 0, instanceIndex = 0;
  for (int i = 0; i < trait_->methodCount; i++) {
    FunctionNode *method = trait_->methods[i];
    Type *methodType = typchkResolveFunctionSignature(env, method);

    if (methodType == NULL) {
      ok = false;
      continue;
    }

    if (method->hasSelf) {
      instanceMethods[instanceIndex].name = method->name;
      instanceMethods[instanceIndex].type = methodType;
      instanceIndex++;
    } else {
      staticMethods[staticIndex].name = method->name;
      staticMethods[staticIndex].type = methodType;
      staticIndex++;
    }
  }

  if (ok) {
    typeTraitSetMethods(traitType, staticMethods, staticIndex, instanceMethods,
                        instanceIndex);
  } else {
    typeTraitMarkUnresolvedMembers(traitType);
  }
}

static void typchkRegisterTraitImpl(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  Type *traitType = typchkTypeEnvLookupTrait(env, impl->traitName);

  if (traitType == NULL) {
    typchkErrorAtTokenFmt(&impl->traitName, "Unknown trait '%.*s'.",
                          impl->traitName.length, impl->traitName.start);
    return;
  }

  if (typeTraitHasUnresolvedMembers(traitType))
    return; // already reported once, at the trait's own declaration

  Type *targetType = typchkTypeEnvLookupStruct(env, impl->targetName);

  if (targetType == NULL) {
    if (tokenIsPrimitiveTypeName(&impl->targetName)) {
      typchkErrorAtTokenFmt(&impl->targetName,
                            "Primitive trait implementations aren't supported "
                            "yet.");
    }
    return;
  }

  if (typeStructIsGeneric(targetType))
    return; // already reported once at the struct's declaration

  InternedName traitName = internTokenName(impl->traitName);

  if (typeStructImplementsTrait(targetType, traitName)) {
    typchkErrorAtTokenFmt(&impl->traitName, "'%.*s' already implements '%.*s'.",
                          impl->targetName.length, impl->targetName.start,
                          impl->traitName.length, impl->traitName.start);
    return;
  }

  bool ok = true;

  // Register the impl's target type e.g. struct for `Self`
  typchkTypeEnvSetImplTargetType(env, targetType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];

    Type *requiredType =
        method->hasSelf ? typeTraitInstanceMethodLookup(traitType, method->name)
                        : typeTraitStaticMethodLookup(traitType, method->name);

    if (requiredType == NULL) {
      Type *otherCategory =
          method->hasSelf
              ? typeTraitStaticMethodLookup(traitType, method->name)
              : typeTraitInstanceMethodLookup(traitType, method->name);

      if (otherCategory != NULL) {
        typchkErrorAtTokenFmt(
            &method->name,
            method->hasSelf
                ? "'%.*s' is a static method on trait '%.*s' -- drop 'self'."
                : "'%.*s' is an instance method on trait '%.*s' -- add "
                  "'self'.",
            method->name.length, method->name.start, impl->traitName.length,
            impl->traitName.start);
      } else {
        typchkErrorAtTokenFmt(&method->name,
                              "'%.*s' isn't a method defined by trait '%.*s'.",
                              method->name.length, method->name.start,
                              impl->traitName.length, impl->traitName.start);
      }
      ok = false;
      continue;
    }

    Type *methodType = typchkResolveFunctionSignature(env, method);
    if (methodType == NULL) {
      ok = false;
      continue;
    }

    Type *concreteMethodType = typeSubstituteSelf(methodType, targetType);
    Type *concreteRequiredType = typeSubstituteSelf(requiredType, targetType);

    if (!typesEqual(concreteMethodType, concreteRequiredType)) {
      typchkErrorAtTokenFmt(
          &method->name,
          "'%.*s' doesn't match trait '%.*s': expected %s, got %s.",
          method->name.length, method->name.start, impl->traitName.length,
          impl->traitName.start, typeToString(concreteRequiredType),
          typeToString(concreteMethodType));
      ok = false;
      continue;
    }

    typeStructAddTraitMethod(targetType, method->name, concreteMethodType,
                             method->hasSelf);
  }

  typchkTypeEnvSetImplTargetType(env, NULL);

  for (int i = 0; i < typeTraitInstanceMethodCount(traitType); i++) {
    TypeMember required = typeTraitInstanceMethodAt(traitType, i);
    bool found = false;
    for (int j = 0; j < impl->methodCount && !found; j++) {
      found = impl->methods[j]->hasSelf &&
              internedNameEqualsToken(required.name, impl->methods[j]->name);
    }
    if (!found) {
      typchkErrorAtTokenFmt(&impl->traitName,
                            "'%.*s' is missing '%.*s', required by trait "
                            "'%.*s'.",
                            impl->targetName.length, impl->targetName.start,
                            required.name.length,
                            internedNameChars(required.name),
                            impl->traitName.length, impl->traitName.start);
      ok = false;
    }
  }
  for (int i = 0; i < typeTraitStaticMethodCount(traitType); i++) {
    TypeMember required = typeTraitStaticMethodAt(traitType, i);
    bool found = false;
    for (int j = 0; j < impl->methodCount && !found; j++) {
      found = !impl->methods[j]->hasSelf &&
              internedNameEqualsToken(required.name, impl->methods[j]->name);
    }
    if (!found) {
      typchkErrorAtTokenFmt(&impl->traitName,
                            "'%.*s' is missing '%.*s', required by trait "
                            "'%.*s'.",
                            impl->targetName.length, impl->targetName.start,
                            required.name.length,
                            internedNameChars(required.name),
                            impl->traitName.length, impl->traitName.start);
      ok = false;
    }
  }

  if (!ok)
    return;

  typeStructMarkTraitImplemented(targetType, traitName);
}

static void typchkCheckTraitSupertraitSatisfied(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  if (!impl->hasTraitName)
    return;

  Type *traitType = typchkTypeEnvLookupTrait(env, impl->traitName);
  if (traitType == NULL || !traitType->as.trait_.hasSupertrait)
    return;

  Type *targetType = typchkTypeEnvLookupStruct(env, impl->targetName);
  if (targetType == NULL || targetType->kind != TYPE_STRUCT)
    return; // already reported, or a (currently unsupported) primitive

  InternedName traitName = internTokenName(impl->traitName);

  if (!typeStructImplementsTrait(targetType, traitName))
    return;

  if (!typeStructImplementsTrait(targetType,
                                 traitType->as.trait_.supertraitName)) {
    typchkErrorAtTokenFmt(
        &impl->traitName,
        "'%.*s' also needs 'impl %.*s for %.*s' -- '%.*s' requires it.",
        impl->targetName.length, impl->targetName.start,
        traitType->as.trait_.supertraitName.length,
        internedNameChars(traitType->as.trait_.supertraitName),
        impl->targetName.length, impl->targetName.start, impl->traitName.length,
        impl->traitName.start);
  }
}

static void typchkRegisterImplMethods(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  if (impl->hasTraitName) {
    typchkRegisterTraitImpl(env, node);
    return;
  }

  Type *structType = typchkTypeEnvLookupStruct(env, impl->targetName);

  if (structType == NULL) {
    if (tokenIsPrimitiveTypeName(&impl->targetName)) {
      typchkErrorAtTokenFmt(
          &impl->targetName,
          "Only trait implementations are allowed on primitive types.");
    }
    return;
  }

  if (typeStructIsGeneric(structType))
    return; // already reported once at the struct's declaration

  typchkTypeEnvSetImplTargetType(env, structType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = typchkResolveFunctionSignature(env, method);

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

  typchkTypeEnvSetImplTargetType(env, NULL);
}

static void typchkRegisterTopLevelFunctionSignature(TypeEnv *env,
                                                    AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = typchkResolveFunctionSignature(env, fn);

  if (fnType != NULL) {
    typchkTypeEnvRegisterFunction(env, fn->name, fnType);
  }
}

static void typchkCheckTopLevelFunctionBody(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  Type *fnType = typchkTypeEnvLookupFunction(env, fn->name);
  if (fnType == NULL)
    return; // signature failed to resolve in Pass D; already reported
  typchkCheckFunctionBody(env, fn, fnType->as.function.paramTypes,
                          fnType->as.function.returnType, NULL);
}

static void checkImplMethodBodies(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;
  Type *structType = typchkTypeEnvLookupStruct(env, impl->targetName);

  typchkTypeEnvSetImplTargetType(env, structType);

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = NULL;

    if (structType != NULL) {
      if (impl->hasTraitName) {
        methodType =
            method->hasSelf
                ? typeStructTraitInstanceMethodLookup(structType, method->name)
                : typeStructTraitStaticMethodLookup(structType, method->name);
      } else if (method->hasSelf) {
        methodType = typeStructInstanceMethodLookup(structType, method->name);
      } else {
        methodType = typeStructStaticMethodLookup(structType, method->name);
      }
    }

    if (methodType == NULL)
      continue; // signature/struct/trait validation failed; already reported

    Type *selfType = method->hasSelf ? structType : NULL;
    typchkCheckFunctionBody(env, method, methodType->as.function.paramTypes,
                            methodType->as.function.returnType, selfType);
  }

  typchkTypeEnvSetImplTargetType(env, NULL);
}

bool typchkCheckProgram(AstNode **program, int count) {
  // Diagnostics are per-unit/program
  typchkResetError();

  bool ownsEnv = sessionEnv == NULL;
  TypeEnv *env = ownsEnv ? typchkTypeEnvCreate() : sessionEnv;

  if (ownsEnv)
    typchkTypeEnvBeginScope(env);

  // Structs

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      StructNode *sn = &program[i]->as.struct_;
      Type *placeholder = typeStruct(sn->name, NULL, 0, NULL, 0, NULL, 0);

      if (sn->genericParamCount > 0) {
        typeStructMarkGeneric(placeholder);
        typchkErrorAtToken(&sn->name, "Generic structs aren't supported yet.");
      }

      typchkTypeEnvRegisterStruct(env, sn->name, placeholder);
    }
  }

  // Trait placeholders
  //
  // Registered before any trait's own methods/supertrait are resolved, so
  // `trait Ord: Eq` works regardless of which trait is declared first.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      TraitNode *tn = &program[i]->as.trait_;
      Type *placeholder = typeTrait(tn->name, NULL, 0, NULL, 0);
      typchkTypeEnvRegisterTrait(env, tn->name, placeholder);
    }
  }

  // Type aliases
  //
  // After struct placeholders so an alias can name a struct, before struct
  // fields so a field can be annotated with an alias.

  // TODO: Refactor this

  UnresolvedTypeAlias *unresolvedAliasAliases = NULL;
  int pendingAliasCount = 0;

  for (int i = 0; i < count; i++) {
    // Generic aliases are still parse-only, same as generic types.
    if (program[i]->kind == NODE_TYPE_ALIAS &&
        program[i]->as.typeAlias.genericParamCount == 0) {
      pendingAliasCount++;
    }
  }

  if (pendingAliasCount > 0) {
    unresolvedAliasAliases = (UnresolvedTypeAlias *)calloc(
        (size_t)pendingAliasCount, sizeof(UnresolvedTypeAlias));

    int next = 0;
    for (int i = 0; i < count; i++) {
      if (program[i]->kind == NODE_TYPE_ALIAS &&
          program[i]->as.typeAlias.genericParamCount == 0) {
        unresolvedAliasAliases[next++].node = program[i];
      }
    }

    for (int i = 0; i < pendingAliasCount; i++) {
      typchkResolvePendingAlias(env, unresolvedAliasAliases, pendingAliasCount,
                                i);
    }

    free(unresolvedAliasAliases);
  }

  // Trait method signatures + supertrait resolution
  //
  // After aliases, so a trait method can be annotated with one; after
  // struct placeholders, so a trait method can reference a struct by name.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      typchkResolveTraitMethods(env, program[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      typchkResolveStructFields(env, program[i]);
    }
  }

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      typchkRegisterImplMethods(env, program[i]);
    }
  }

  // Supertrait satisfaction
  //
  // After every impl in the program has been registered, so `impl Ord for
  // X` is checked against `impl Eq for X` regardless of which appears
  // first in the file.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_IMPL) {
      typchkCheckTraitSupertraitSatisfied(env, program[i]);
    }
  }

  // Hoisted Functions

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_FUNCTION) {
      typchkRegisterTopLevelFunctionSignature(env, program[i]);
    }
  }

  // Definite Assignment Analysis

  DaaSet topLevelDaa;
  daaSetInit(&topLevelDaa);

  for (int i = 0; i < count; i++) {
    AstNode *node = program[i];

    switch (node->kind) {
    case NODE_FUNCTION:
      typchkCheckTopLevelFunctionBody(env, node);
      break;

    case NODE_IMPL:
      checkImplMethodBodies(env, node);
      break;

    default:
      typchkCheckStmt(env, node);
      break;
    }

    daaCheckAssignmentStmt(&topLevelDaa, node);
  }

  daaSetFree(&topLevelDaa);

  // Clean Up

  bool ok = !typchkHadError();

  if (ownsEnv) {
    typchkTypeEnvEndScope(env);
    typchkTypeEnvDestroy(env);
  }

  return ok;
}
