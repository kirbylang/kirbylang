#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_signatures.h"
#include "typenv.h"

typedef struct {
  InternedName name;
  Type *type;
} TypeEnvBinding;

static void _bindingArrayWrite(TypeEnvBinding **array, int *count,
                               int *capacity, Token name, Type *type) {
  if (*capacity < *count + 1) {
    *capacity = *capacity < 8 ? 8 : *capacity * 2;
    *array =
        (TypeEnvBinding *)realloc(*array, sizeof(TypeEnvBinding) * (*capacity));
    if (*array == NULL) {
      fprintf(stderr, "realloc failed in _bindingArrayWrite\n");
      exit(1);
    }
  }
  (*array)[*count].name = internTokenName(name);
  (*array)[*count].type = type;
  (*count)++;
}

static Type *_bindingArrayLookup(TypeEnvBinding *array, int count, Token name) {
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

static Token _makeTokenFromCString(const char *text) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = text;
  token.length = (int)strlen(text);
  token.line = 0;
  return token;
}

static void _typeEnvDefineBuiltinTraits(TypeEnv *env);

TypeEnv *typeEnvInit(void) {
  TypeEnv *env = (TypeEnv *)malloc(sizeof(TypeEnv));
  memset(env, 0, sizeof(TypeEnv));
  defineAllNativeSignatures(env);
  _typeEnvDefineBuiltinTraits(env);
  return env;
}

void typeEnvFree(TypeEnv *env) {
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

  _bindingArrayWrite(&scope->bindings, &scope->count, &scope->capacity, name,
                     type);
}

Type *typeEnvLookupName(TypeEnv *env, Token name) {
  for (int i = env->scopeCount - 1; i >= 0; i--) {
    Type *found = _bindingArrayLookup(env->scopes[i].bindings,
                                      env->scopes[i].count, name);
    if (found != NULL)
      return found;
  }
  return NULL;
}

void typeEnvRegisterStruct(TypeEnv *env, Token name, Type *type) {
  _bindingArrayWrite(&env->structs, &env->structCount, &env->structCapacity,
                     name, type);
}

Type *typeEnvLookupStruct(TypeEnv *env, Token name) {
  return _bindingArrayLookup(env->structs, env->structCount, name);
}

void typeEnvRegisterFunction(TypeEnv *env, Token name, Type *type) {
  _bindingArrayWrite(&env->functions, &env->functionCount,
                     &env->functionCapacity, name, type);
}

Type *typeEnvLookupFunction(TypeEnv *env, Token name) {
  return _bindingArrayLookup(env->functions, env->functionCount, name);
}

void typeEnvRegisterAlias(TypeEnv *env, Token name, Type *type) {
  _bindingArrayWrite(&env->aliases, &env->aliasCount, &env->aliasCapacity, name,
                     type);
}

Type *typeEnvLookupAlias(TypeEnv *env, Token name) {
  return _bindingArrayLookup(env->aliases, env->aliasCount, name);
}

void typeEnvRegisterTrait(TypeEnv *env, Token name, Type *type) {
  _bindingArrayWrite(&env->traits, &env->traitCount, &env->traitCapacity, name,
                     type);
}

Type *typeEnvLookupTrait(TypeEnv *env, Token name) {
  return _bindingArrayLookup(env->traits, env->traitCount, name);
}

int typeEnvTraitCount(TypeEnv *env) { return env->traitCount; }

/**
 * Define the builtin traits
 *
 * Display, Eq, Ord (a supertrait of Eq), and Default
 */
static void _typeEnvDefineBuiltinTraits(TypeEnv *env) {
  // Display

  UninternedTypeMember displayInstance[] = {
      {_makeTokenFromCString("toString"), typeFunction(NULL, 0, typeString())},
  };
  Type *display =
      typeTrait(_makeTokenFromCString("Display"), NULL, 0, displayInstance, 1);
  typeTraitMarkBuiltin(display);
  typeEnvRegisterTrait(env, _makeTokenFromCString("Display"), display);

  // Eq

  Type **equalsParams = (Type **)typesAllocRaw(sizeof(Type *));
  equalsParams[0] = typeSelfPlaceholder();
  UninternedTypeMember eqInstance[] = {
      {_makeTokenFromCString("equals"),
       typeFunction(equalsParams, 1, typeBool())},
  };
  Type *eq = typeTrait(_makeTokenFromCString("Eq"), NULL, 0, eqInstance, 1);
  typeTraitMarkBuiltin(eq);
  typeEnvRegisterTrait(env, _makeTokenFromCString("Eq"), eq);
  Type **cmpParams = (Type **)typesAllocRaw(sizeof(Type *));
  cmpParams[0] = typeSelfPlaceholder();
  UninternedTypeMember ordInstance[] = {
      {_makeTokenFromCString("cmp"), typeFunction(cmpParams, 1, typeF64())},
  };

  // Ord

  Type *ord = typeTrait(_makeTokenFromCString("Ord"), NULL, 0, ordInstance, 1);
  typeTraitSetSupertrait(ord, eq->as.trait_.name);
  typeTraitMarkBuiltin(ord);
  typeEnvRegisterTrait(env, _makeTokenFromCString("Ord"), ord);

  // Default

  UninternedTypeMember defaultStatic[] = {
      {_makeTokenFromCString("default"),
       typeFunction(NULL, 0, typeSelfPlaceholder())},
  };
  Type *default_ =
      typeTrait(_makeTokenFromCString("Default"), defaultStatic, 1, NULL, 0);
  typeTraitMarkBuiltin(default_);
  typeEnvRegisterTrait(env, _makeTokenFromCString("Default"), default_);
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

void typeEnvSetImplTargetType(TypeEnv *env, Type *implTargetType) {
  env->_currentImplTargetType = implTargetType;
}

Type *typeEnvGetImplTargetType(TypeEnv *env) {
  return env->_currentImplTargetType;
}
