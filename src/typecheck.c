#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "definite_assignment.h"
#include "native_signatures.h"
#include "resolved_impl_targets.h"
#include "typecheck.h"

static bool hadError = false;

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

// A generic type alias. `target` holds the alias's parameters as unfilled
// generic parameters, and is NULL if it didn't resolve. `params` are those
// parameters, which hold the alias's bounds.
typedef struct {
  InternedName name;
  int paramCount;
  Type **params;
  Type *target;
} GenericAlias;

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

// A method declared with `impl string { ... }` in the standard library.
// `owner` is the type's name, as the compiler spells it in the method's global
// name.
// A method that came from `impl Trait for string` has `isTrait` set and names
// its trait in `trait`.
typedef struct {
  const char *owner;
  InternedName name;
  Type *type;
  bool isPublic;
  bool hasSelf;
  bool isTrait;
  InternedName trait;
  // For a list or array impl: the receiver as the impl wrote it, such as
  // `[T]`, and how many type parameters the impl declared. The method's own
  // type parameters are numbered after them. NULL for other owners.
  Type *receiver;
  int implParamCount;
} PrimitiveMethod;

// Records that a primitive type implements a trait
typedef struct {
  const char *owner;
  InternedName trait;
} PrimitiveTraitImpl;

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

  // Methods of primitive types. Only the standard library declares them
  PrimitiveMethod *primitiveMethods;
  int primitiveMethodCount;
  int primitiveMethodCapacity;
  PrimitiveTraitImpl *primitiveTraitImpls;
  int primitiveTraitImplCount;
  int primitiveTraitImplCapacity;

  // Generic type aliases such as `type Wrapper[T] = Box[T];`
  GenericAlias *genericAliases;
  int genericAliasCount;
  int genericAliasCapacity;

  // WIP State

  Type *_selfType;          // NULL when not currently checking a method body.
  Type *_currentReturnType; // NULL when not checking a function/method body, or
                            // its return type didn't resolve.
  Type *_currentImplTargetType; // NULL when not checking an impl block
  bool _resolvingSignature;     // Allows the names only native signatures use
  bool _isStdlib; // The unit being checked is the standard library
  // Owner of the primitive impl block being checked, NULL outside one
  const char *_currentPrimitiveOwner;
  // The get expression being called, so `a.b` alone can be told from `a.b()`
  AstNode *_calleeNode;

  // Generic parameters of the declarations being checked, innermost last
  TypeEnvBinding *_genericParams;
  int _genericParamCount;
  int _genericParamCapacity;

  // Inside the impl of a generic struct, `impl Box[T]`: the impl's own
  // parameters, and `Self` as a type, which is `Box[T]`. A method's own
  // generic parameters are numbered after the impl's.
  Type **_implGenericTypes;
  int _implGenericCount;
  Type *_currentImplSelfType;
  // Inside an impl for a list or array, its receiver type, such as `[T]`
  Type *_currentImplReceiver;
  // The type the call being checked is expected to return, passed along to the
  // call check. Read once and cleared.
  Type *_callExpected;
};

static GenericAlias *genericAliasLookup(TypeEnv *env, Token name) {
  for (int i = env->genericAliasCount - 1; i >= 0; i--) {
    if (internedNameEqualsToken(env->genericAliases[i].name, name))
      return &env->genericAliases[i];
  }

  return NULL;
}

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

TypeEnv *typchkSessionEnv(void) { return sessionEnv; }

TypeEnv *typchkTypeEnvCreate(void) {
  TypeEnv *env = (TypeEnv *)malloc(sizeof(TypeEnv));
  memset(env, 0, sizeof(TypeEnv));
  defineAllNativeSignatures(env);
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
  free(env->genericAliases);
  free(env->primitiveMethods);
  free(env->primitiveTraitImpls);
  free(env->_genericParams);
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

// Puts a declaration's generic parameters in scope and returns how many were
// in scope before, to give to typchkLeaveGenericParams. Each parameter's type
// is also written to `types`, and numbered from `firstIndex`. A parameter
// callers fill in is made when
// `isOpaque` is false, and the fixed type the declaration's own body sees when
// it is true. `bounds` has the traits each parameter must implement, or is
// NULL where a declaration has no bounds. An unknown trait is reported once,
// for the form callers fill in.
static int typchkEnterGenericParams(TypeEnv *env, Token *names,
                                    GenericBounds *bounds, int count,
                                    int firstIndex, bool isOpaque,
                                    Type **types) {
  int previousCount = env->_genericParamCount;

  for (int i = 0; i < count; i++) {
    for (int j = 0; j < i; j++) {
      if (names[i].length == names[j].length &&
          memcmp(names[i].start, names[j].start, names[i].length) == 0) {
        if (!isOpaque) {
          typchkErrorAtTokenFmt(&names[i],
                                "Generic parameter '%.*s' is already declared.",
                                names[i].length, names[i].start);
        }
        break;
      }
    }

    types[i] = typeDeclaredGenericParam(names[i], firstIndex + i, isOpaque);

    for (int j = 0; bounds != NULL && j < bounds[i].count; j++) {
      Token *traitName = &bounds[i].traits[j];
      Type *boundTrait = typchkTypeEnvLookupTrait(env, *traitName);

      if (boundTrait != NULL) {
        typeGenericParamAddBound(types[i], boundTrait->as.trait_.name);
      } else if (!isOpaque) {
        typchkErrorAtTokenFmt(traitName, "Unknown trait '%.*s'.",
                              traitName->length, traitName->start);
      }
    }

    bindingArrayWrite(&env->_genericParams, &env->_genericParamCount,
                      &env->_genericParamCapacity, names[i], types[i]);
  }

  return previousCount;
}

static void typchkLeaveGenericParams(TypeEnv *env, int previousCount) {
  env->_genericParamCount = previousCount;
}

// Gives an impl's parameters the bounds of the struct's parameters, which they
// stand for in order.
static void typchkInheritStructBounds(Type *structType, Type **implParams,
                                      int count) {
  Type **structParams = structType->as.struct_.genericParams;

  if (structParams == NULL)
    return;

  for (int i = 0; i < count; i++) {
    for (int j = 0; j < structParams[i]->as.genericParam.boundCount; j++) {
      typeGenericParamAddBound(implParams[i],
                               structParams[i]->as.genericParam.boundTraits[j]);
    }
  }
}

// Whether `trait` is `traitName` or has it as a supertrait.
static bool typchkTraitChainHas(TypeEnv *env, InternedName trait,
                                InternedName traitName) {
  InternedName current = trait;

  // A supertrait cycle is reported elsewhere, so the walk must still end
  for (int step = 0; step <= env->traitCount; step++) {
    if (internedNamesEqual(current, traitName))
      return true;

    Type *found = typchkTypeEnvLookupTrait(env, internedNameToToken(current));

    if (found == NULL || !found->as.trait_.hasSupertrait)
      return false;

    current = found->as.trait_.supertraitName;
  }

  return false;
}

// Numbers have the operator traits built in, until primitives can implement
// traits themselves. Only a signed number can be negated.
static bool typchkNumberImplementsTrait(Type *number, InternedName traitName) {
  int count = getOperatorTraitCount();

  for (int i = 0; i < count; i++) {
    const OperatorTrait *entry = &table[i];

    if (!internedNameEqualsToken(traitName, tokenFromCString(entry->trait)))
      continue;

    return !entry->isUnary || numberKindInfo(number->as.number.kind)->isSigned;
  }

  return false;
}

// Whether values of `type` have the methods of a trait. A struct needs an
// impl. A generic parameter has them through a bound, or through a supertrait
// of a bound, so `T: Ord` also gives `Eq`.
static bool typchkTypeImplementsTrait(TypeEnv *env, Type *type,
                                      InternedName traitName) {
  if (type != NULL && typeIsNumber(type))
    return typchkNumberImplementsTrait(type, traitName);

  if (type == NULL || type->kind != TYPE_GENERIC_PARAM)
    return typeStructImplementsTrait(type, traitName);

  for (int i = 0; i < type->as.genericParam.boundCount; i++) {
    if (typchkTraitChainHas(env, type->as.genericParam.boundTraits[i],
                            traitName))
      return true;
  }

  return false;
}

typedef enum {
  BOUND_METHOD_FOUND,
  BOUND_METHOD_MISSING,
  BOUND_METHOD_AMBIGUOUS
} BoundMethodResult;

// The type of a method a generic parameter's bounds give it, with `Self` read
// as the parameter. The method is looked up on each bound trait and then on
// its supertraits. A method that two different traits both have is ambiguous,
// and the two trait names are written to `first` and `second`.
static BoundMethodResult typchkBoundMethodLookup(TypeEnv *env, Type *param,
                                                 Token name, Type **found,
                                                 InternedName *first,
                                                 InternedName *second) {
  *found = NULL;

  for (int i = 0; i < param->as.genericParam.boundCount; i++) {
    InternedName current = param->as.genericParam.boundTraits[i];

    for (int step = 0; step <= env->traitCount; step++) {
      Type *trait = typchkTypeEnvLookupTrait(env, internedNameToToken(current));

      if (trait == NULL)
        break;

      Type *methodType = typeTraitInstanceMethodLookup(trait, name);

      if (methodType != NULL) {
        if (*found != NULL && !internedNamesEqual(*first, current)) {
          *second = current;
          return BOUND_METHOD_AMBIGUOUS;
        }

        if (*found == NULL) {
          *found = typeSubstituteSelf(methodType, param);
          *first = current;
        }
        break;
      }

      if (!trait->as.trait_.hasSupertrait)
        break;

      current = trait->as.trait_.supertraitName;
    }
  }

  return *found != NULL ? BOUND_METHOD_FOUND : BOUND_METHOD_MISSING;
}

// Names that only native signatures can use: generic parameters T, U, V and
// W, and `number` and `integer`, which accept any number or integer kind.
static Type *typchkResolveSignatureName(Token *name) {
  static const char genericNames[] = "TUVW";

  if (name->length == 1) {
    const char *found = strchr(genericNames, name->start[0]);
    if (found != NULL)
      return typeGenericParam((int)(found - genericNames));
  }

  if (tokenTextEquals(name, "number"))
    return typeAnyNumber();
  if (tokenTextEquals(name, "integer"))
    return typeAnyInteger();

  return NULL;
}

Type *typchkResolveSignatureType(TypeEnv *env, AstNode *typeAnnotation) {
  env->_resolvingSignature = true;
  Type *type = typchkResolveType(env, typeAnnotation);
  env->_resolvingSignature = false;
  return type;
}

static bool typchkTypeArgsMeetBounds(TypeEnv *env, InternedName owner,
                                     Type **params, Type **args, int count,
                                     Token *token, AstNode *node);

// Resolves a type written with type arguments, such as `Box[f64]`, or
// `Wrapper[i64]`: the alias's target with its parameters replaced by the
// arguments.
static Type *typchkInstantiateGenericAlias(TypeEnv *env, TypeNode *t,
                                           GenericAlias *alias) {
  if (t->genericArgCount != alias->paramCount) {
    typchkErrorAtTokenFmt(&t->name, "'%.*s' takes %d type argument(s), got %d.",
                          t->name.length, t->name.start, alias->paramCount,
                          t->genericArgCount);
    return NULL;
  }

  Type **typeArgs =
      (Type **)typesAllocRaw((size_t)t->genericArgCount * sizeof(Type *));

  for (int i = 0; i < t->genericArgCount; i++) {
    typeArgs[i] = typchkResolveType(env, t->genericArgs[i]);

    if (typeArgs[i] == NULL)
      return NULL;
  }

  // The alias's own error was already reported
  if (alias->target == NULL)
    return NULL;

  if (!typchkTypeArgsMeetBounds(env, alias->name, alias->params, typeArgs,
                                t->genericArgCount, &t->name, NULL))
    return NULL;

  return typeSubstituteGenericParams(alias->target, typeArgs,
                                     t->genericArgCount);
}

// `Self` was used where a struct is needed. Inside a primitive's impl block
// `Self` is the primitive, which has no fields or static methods.
static void typchkErrorSelfIsNotAStruct(TypeEnv *env, Token *token) {
  if (env->_currentPrimitiveOwner != NULL) {
    typchkErrorAtTokenFmt(token, "'Self' is '%s' in this impl, not a struct.",
                          env->_currentPrimitiveOwner);
    return;
  }

  typchkErrorAtToken(token, "'Self' can only be used inside an impl block.");
}

// `Self[U]` inside the impl of a generic struct.
static Type *typchkResolveGenericInstance(TypeEnv *env, TypeNode *t) {
  Type *declaration;

  if (tokenTextEquals(&t->name, "Self")) {
    declaration = env->_currentImplTargetType;

    if (declaration == NULL) {
      typchkErrorSelfIsNotAStruct(env, &t->name);
      return NULL;
    }
  } else {
    declaration = typchkTypeEnvLookupStruct(env, t->name);
  }

  if (declaration == NULL) {
    GenericAlias *alias = genericAliasLookup(env, t->name);

    if (alias == NULL) {
      if (typchkTypeEnvLookupAlias(env, t->name) != NULL)
        typchkErrorAtTokenFmt(&t->name, "'%.*s' doesn't take type arguments.",
                              t->name.length, t->name.start);
      else
        typchkErrorAtToken(&t->name, "Unknown type.");
      return NULL;
    }

    return typchkInstantiateGenericAlias(env, t, alias);
  }

  if (!typeStructIsGeneric(declaration)) {
    typchkErrorAtTokenFmt(&t->name, "'%.*s' doesn't take type arguments.",
                          t->name.length, t->name.start);
    return NULL;
  }

  int expected = declaration->as.struct_.genericParamCount;

  if (t->genericArgCount != expected) {
    typchkErrorAtTokenFmt(&t->name, "'%.*s' takes %d type argument(s), got %d.",
                          t->name.length, t->name.start, expected,
                          t->genericArgCount);
    return NULL;
  }

  Type **typeArgs =
      (Type **)typesAllocRaw((size_t)t->genericArgCount * sizeof(Type *));

  for (int i = 0; i < t->genericArgCount; i++) {
    typeArgs[i] = typchkResolveType(env, t->genericArgs[i]);

    if (typeArgs[i] == NULL)
      return NULL; // already reported below the recursive call
  }

  if (!typchkTypeArgsMeetBounds(env, declaration->as.struct_.name,
                                declaration->as.struct_.genericParams, typeArgs,
                                t->genericArgCount, &t->name, NULL))
    return NULL;

  return typeStructInstance(declaration, typeArgs, t->genericArgCount);
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

  if (typeAnnotation->kind == NODE_TYPE_LIST) {
    Type *elementType =
        typchkResolveType(env, typeAnnotation->as.typeList.elementType);
    return elementType == NULL ? NULL : typeList(elementType);
  }

  if (typeAnnotation->kind == NODE_TYPE_ARRAY) {
    Type *elementType =
        typchkResolveType(env, typeAnnotation->as.typeArray.elementType);
    return elementType == NULL
               ? NULL
               : typeArray(elementType, typeAnnotation->as.typeArray.length);
  }

  TypeNode *t = &typeAnnotation->as.type_;

  if (env->_resolvingSignature && t->genericArgCount == 0) {
    Type *signatureType = typchkResolveSignatureName(&t->name);
    if (signatureType != NULL)
      return signatureType;
  }

  if (t->genericArgCount > 0)
    return typchkResolveGenericInstance(env, t);

  if (tokenTextEquals(&t->name, "unit"))
    return typeUnit();
  if (tokenTextEquals(&t->name, "bool"))
    return typeBool();
  if (tokenTextEquals(&t->name, "string"))
    return typeString();
  NumberKind numberKind;
  if (numberKindFromName(t->name.start, t->name.length, &numberKind))
    return typeNumber(numberKind);
  if (tokenTextEquals(&t->name, "Self")) {
    if (env->_currentImplSelfType != NULL)
      return env->_currentImplSelfType;

    return env->_currentImplTargetType != NULL ? env->_currentImplTargetType
                                               : typeSelfPlaceholder();
  }

  // The innermost name wins, so a parameter hides a struct or alias of the
  // same name inside its declaration
  Type *genericParam =
      bindingArrayLookup(env->_genericParams, env->_genericParamCount, t->name);
  if (genericParam != NULL)
    return genericParam;

  Type *structType = typchkTypeEnvLookupStruct(env, t->name);
  if (structType != NULL) {
    if (typeStructIsGeneric(structType)) {
      typchkErrorAtTokenFmt(&t->name, "'%.*s' needs %d type argument(s).",
                            t->name.length, t->name.start,
                            structType->as.struct_.genericParamCount);
      return NULL;
    }
    return structType;
  }

  Type *aliasType = typchkTypeEnvLookupAlias(env, t->name);
  if (aliasType != NULL)
    return aliasType;

  GenericAlias *genericAlias = genericAliasLookup(env, t->name);
  if (genericAlias != NULL) {
    typchkErrorAtTokenFmt(&t->name, "'%.*s' needs %d type argument(s).",
                          t->name.length, t->name.start,
                          genericAlias->paramCount);
    return NULL;
  }

  typchkErrorAtToken(&t->name, "Unknown type.");
  return NULL;
}

static Type *typchkInferLiteral(AstNode *node);
static Type *typchkInferUnary(TypeEnv *env, AstNode *node);
static Type *typchkInferBinary(TypeEnv *env, AstNode *node);
static Type *typchkInferCast(TypeEnv *env, AstNode *node);
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
static Type *typchkInferInterpString(TypeEnv *env, AstNode *node);
static Type *typchkInferIf(TypeEnv *env, AstNode *node);
static bool typchkAlwaysYields(AstNode *node);
static Type *typchkInferBlock(TypeEnv *env, AstNode *node);
static bool typchkCheckInto(TypeEnv *env, AstNode *node, Type *expected,
                            Type **actualOut);
static Type *typchkCheckBlockContents(TypeEnv *env, BlockNode *block,
                                      Type *expectedValueType);
static Type *typchkCheckOrInferLambda(TypeEnv *env, AstNode *node,
                                      Type *expected);
static void typchkCheckVarDecl(TypeEnv *env, AstNode *node);
static void typchkCheckFunctionDecl(TypeEnv *env, AstNode *node);
static bool typchkStructMembersUnreliable(Type *type);
static Type *typchkLookupVariableType(TypeEnv *env, Token *name);
static bool typchkHasDeclaredGenericParams(Type *type);
static bool typchkCheckGenericFunctionValue(TypeEnv *env, AstNode *node,
                                            Type *genericType, Type *expected);
static Type *typchkFillGenericFunctionValue(TypeEnv *env, AstNode *node,
                                            Type *genericType, Type *expected);
static Type *typchkStructInitDeclaration(TypeEnv *env, StructInitNode *si);
static bool typchkCheckGenericStructInit(TypeEnv *env, AstNode *node,
                                         Type *expected);

// The largest integer a literal can be while still being exact in the kind.
static uint64_t typchkLargestExactInteger(NumberKind kind) {
  if (kind == NUMBER_KIND_F32)
    return (uint64_t)1 << 24;

  if (kind == NUMBER_KIND_F64)
    return (uint64_t)1 << 53;

  return numberKindMax(kind);
}

static bool typchkIsPlainIntegerLiteral(AstNode *node) {
  return node->kind == NODE_LITERAL &&
         node->as.literal.kind == LITERAL_NUMBER &&
         node->as.literal.as.number.literalIsPlainInteger;
}

// Whether a plain integer literal, such as `65`, can be written exactly in
// `kind`. Suffixed and decimal literals are an explicit choice and never fit.
static bool typchkPlainIntegerFits(AstNode *literal, NumberKind kind) {
  if (!typchkIsPlainIntegerLiteral(literal))
    return false;

  uint64_t value = literal->as.literal.as.number.literalValue;

  return literal->as.literal.as.number.literalValueFits &&
         value <= typchkLargestExactInteger(kind);
}

// Whether `-literal` can be written exactly in `kind`, which includes the
// smallest value of a signed kind, such as `-128` for `i8`.
static bool typchkNegatedPlainIntegerFits(AstNode *literal, NumberKind kind) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (!info->isSigned || !typchkIsPlainIntegerLiteral(literal) ||
      !literal->as.literal.as.number.literalValueFits)
    return false;

  uint64_t largest = info->isFloat ? typchkLargestExactInteger(kind)
                                   : 0 - (uint64_t)numberKindMin(kind);

  return literal->as.literal.as.number.literalValue <= largest;
}

// Rewrites a plain integer literal as a literal of `kind`. The compiler reads
// the literal's kind, so this happens before compilation.
static void typchkSetPlainIntegerKind(AstNode *literal, NumberKind kind) {
  LiteralNode *lit = &literal->as.literal;

  lit->as.number.kind = kind;
  lit->as.number.literalIsPlainInteger = false;

  if (numberKindInfo(kind)->isFloat)
    lit->as.number.as.f64 = (double)lit->as.number.literalValue;
  else
    lit->as.number.as.u64 = lit->as.number.literalValue;
}

// Rewrites `-literal` as one negative literal of `kind`.
static void typchkFoldNegatedPlainInteger(AstNode *node, NumberKind kind) {
  uint64_t value = node->as.unary.operand->as.literal.as.number.literalValue;

  node->kind = NODE_LITERAL;
  node->as.literal.kind = LITERAL_NUMBER;
  node->as.literal.as.number.kind = kind;
  node->as.literal.as.number.literalIsPlainInteger = false;

  if (numberKindInfo(kind)->isFloat)
    node->as.literal.as.number.as.f64 = -(double)value;
  else
    node->as.literal.as.number.as.i64 = (int64_t)(0 - value);
}

static bool typchkIsArithmeticOperator(TokenType type) {
  return type == TOKEN_PLUS || type == TOKEN_MINUS || type == TOKEN_STAR ||
         type == TOKEN_SLASH || type == TOKEN_MODULO;
}

// Called on each leaf of literal arithmetic: a number literal or `-literal`.
// Returning false stops the walk.
typedef bool (*LiteralLeafVisitor)(AstNode *leaf, NumberKind kind,
                                   void *context);

// Walks a value made only of number literals, brackets, `-` and arithmetic,
// such as `(1 + 2) * -3`, calling `visit` on each leaf. Returns false if the
// value is anything else, if `visit` returns false, or if `-` is applied to
// more than a literal while `kind` is unsigned.
static bool typchkVisitLiteralArithmetic(AstNode *node, NumberKind kind,
                                         LiteralLeafVisitor visit,
                                         void *context) {
  switch (node->kind) {
  case NODE_LITERAL:
    return node->as.literal.kind == LITERAL_NUMBER &&
           visit(node, kind, context);
  case NODE_GROUPING:
    return typchkVisitLiteralArithmetic(node->as.grouping.inner, kind, visit,
                                        context);
  case NODE_UNARY:
    if (node->as.unary.op.type != TOKEN_MINUS)
      return false;
    if (typchkIsPlainIntegerLiteral(node->as.unary.operand))
      return visit(node, kind, context);
    return numberKindInfo(kind)->isSigned &&
           typchkVisitLiteralArithmetic(node->as.unary.operand, kind, visit,
                                        context);
  case NODE_BINARY:
    return typchkIsArithmeticOperator(node->as.binary.op.type) &&
           typchkVisitLiteralArithmetic(node->as.binary.left, kind, visit,
                                        context) &&
           typchkVisitLiteralArithmetic(node->as.binary.right, kind, visit,
                                        context);
  default:
    return false;
  }
}

// A leaf fits if it can be written exactly in `kind` or already is `kind`.
static bool typchkLiteralLeafFits(AstNode *leaf, NumberKind kind,
                                  void *context) {
  (void)context;

  if (leaf->kind == NODE_UNARY)
    return typchkNegatedPlainIntegerFits(leaf->as.unary.operand, kind);

  if (typchkIsPlainIntegerLiteral(leaf))
    return typchkPlainIntegerFits(leaf, kind);

  return leaf->as.literal.as.number.kind == kind;
}

static bool typchkSetLiteralLeafKind(AstNode *leaf, NumberKind kind,
                                     void *context) {
  (void)context;

  if (leaf->kind == NODE_UNARY)
    typchkFoldNegatedPlainInteger(leaf, kind);
  else if (typchkIsPlainIntegerLiteral(leaf))
    typchkSetPlainIntegerKind(leaf, kind);

  return true;
}

static bool typchkLiteralLeafIsPlainInteger(AstNode *leaf, NumberKind kind,
                                            void *context) {
  (void)kind;
  (void)context;
  return leaf->kind == NODE_UNARY || typchkIsPlainIntegerLiteral(leaf);
}

// Whether every number in literal arithmetic can be of `kind`.
static bool typchkLiteralArithmeticFits(AstNode *node, NumberKind kind) {
  return typchkVisitLiteralArithmetic(node, kind, typchkLiteralLeafFits, NULL);
}

// Whether a value is literal arithmetic made only of plain integers, such as
// `2 * 3`, so it has no number type of its own until something gives it one.
static bool typchkIsPlainIntegerArithmetic(AstNode *node) {
  return typchkVisitLiteralArithmetic(node, NUMBER_KIND_I64,
                                      typchkLiteralLeafIsPlainInteger, NULL);
}

// Records the first leaf that doesn't fit `kind` in `context`.
static bool typchkFindMisfitLeaf(AstNode *leaf, NumberKind kind,
                                 void *context) {
  if (typchkLiteralLeafFits(leaf, kind, NULL))
    return true;

  *(AstNode **)context = leaf;
  return false;
}

// Reports why plain integer arithmetic can't be `kind`, naming the literal
// and what `kind` holds, e.g. "Integer literal 300 doesn't fit in u8, which
// holds 0 to 255."
static void typchkReportLiteralMisfit(AstNode *node, NumberKind kind) {
  const NumberKindInfo *info = numberKindInfo(kind);
  AstNode *leaf = NULL;

  typchkVisitLiteralArithmetic(node, kind, typchkFindMisfitLeaf, &leaf);

  if (leaf == NULL) {
    typchkErrorAtNodeFmt(node, "%s can't be negative.", info->name);
    return;
  }

  bool negative = leaf->kind == NODE_UNARY;
  AstNode *literal = negative ? leaf->as.unary.operand : leaf;

  if (!literal->as.literal.as.number.literalValueFits) {
    typchkErrorAtNode(leaf,
                      "Integer literal is too large for any number type.");
    return;
  }

  char value[32];
  snprintf(value, sizeof(value), "%s%llu ", negative ? "-" : "",
           (unsigned long long)literal->as.literal.as.number.literalValue);

  if (info->isFloat) {
    unsigned long long largest =
        (unsigned long long)typchkLargestExactInteger(kind);
    typchkErrorAtNodeFmt(leaf,
                         "Integer literal %scan't be stored exactly in %s, "
                         "which holds whole numbers from -%llu to %llu "
                         "exactly.",
                         value, info->name, largest, largest);
    return;
  }

  typchkErrorAtNodeFmt(leaf,
                       "Integer literal %sdoesn't fit in %s, which holds "
                       "%lld to %llu.",
                       value, info->name, (long long)numberKindMin(kind),
                       (unsigned long long)numberKindMax(kind));
}

// Called on a value, or a value it comes from. Returns true when it handled
// the value, which stops the walk going inside it.
typedef bool (*ValueSourceVisitor)(AstNode *value, NumberKind kind);

// Walks the places a value comes from: the value itself, or the value inside
// brackets, both if/else branches and a block's last value. Returns whether
// `visit` handled any of them.
static bool typchkVisitValueSources(AstNode *node, NumberKind kind,
                                    ValueSourceVisitor visit) {
  if (node == NULL)
    return false;

  if (visit(node, kind))
    return true;

  switch (node->kind) {
  case NODE_GROUPING:
    return typchkVisitValueSources(node->as.grouping.inner, kind, visit);
  case NODE_IF: {
    bool thenHandled =
        typchkVisitValueSources(node->as.if_.thenBranch, kind, visit);
    bool elseHandled =
        typchkVisitValueSources(node->as.if_.elseBranch, kind, visit);
    return thenHandled || elseHandled;
  }
  case NODE_BLOCK:
    return typchkVisitValueSources(node->as.block.value, kind, visit);
  default:
    return false;
  }
}

static bool typchkSetLiteralArithmeticKind(AstNode *value, NumberKind kind) {
  if (!typchkLiteralArithmeticFits(value, kind))
    return false;

  typchkVisitLiteralArithmetic(value, kind, typchkSetLiteralLeafKind, NULL);
  return true;
}

// Plain integer arithmetic that is still plain didn't fit the expected kind.
static bool typchkReportPlainIntegerArithmetic(AstNode *value,
                                               NumberKind kind) {
  if (!typchkIsPlainIntegerArithmetic(value))
    return false;

  typchkReportLiteralMisfit(value, kind);
  return true;
}

static bool typchkIsPlainIntegerSource(AstNode *value, NumberKind kind) {
  (void)kind;
  return typchkIsPlainIntegerArithmetic(value);
}

// Whether a value, or a value it comes from, is plain integer arithmetic, as
// in `if (flag) { 1 } else 2`.
static bool typchkHasPlainIntegerSource(AstNode *node) {
  return typchkVisitValueSources(node, NUMBER_KIND_I64,
                                 typchkIsPlainIntegerSource);
}

static bool typchkIsConcreteNumberType(Type *type) {
  return type != NULL && typeIsNumber(type) &&
         type->as.number.kind != NUMBER_KIND_COUNT;
}

// Gives the number literals in a value the number type expected of the value,
// so `let byte: u8 = 65;` needs no suffix and `let half: f64 = 7 / 2;` is 3.5.
// Arithmetic changes only when every number in it is a literal that fits, so
// a variable always keeps its own type. Plain integer arithmetic that can't
// be the expected type is reported. Returns false after reporting.
static bool typchkApplyExpectedNumberType(AstNode *node, Type *expected) {
  if (node == NULL || !typchkIsConcreteNumberType(expected))
    return true;

  NumberKind kind = expected->as.number.kind;

  typchkVisitValueSources(node, kind, typchkSetLiteralArithmeticKind);

  return !typchkVisitValueSources(node, kind,
                                  typchkReportPlainIntegerArithmetic);
}

// An array index is a number of any kind.
static bool typchkCheckIndex(TypeEnv *env, AstNode *index) {
  Type *type = typchkInfer(env, index);

  if (type == NULL)
    return false;

  if (!typeIsNumber(type)) {
    typchkErrorAtNodeFmt(index, "Array index must be an integer, got %s.",
                         typeToString(type));
    return false;
  }

  if (numberKindInfo(type->as.number.kind)->isFloat) {
    typchkErrorAtNodeFmt(
        index, "Array index must be an integer, got %s. Cast it with `as`.",
        typeToString(type));
    return false;
  }

  return true;
}

// Infer the type of an expression
Type *typchkInfer(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_LITERAL:
    return typchkInferLiteral(node);
  case NODE_UNARY:
    return typchkInferUnary(env, node);
  case NODE_BINARY:
    return typchkInferBinary(env, node);
  case NODE_CAST:
    return typchkInferCast(env, node);
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
  case NODE_INTERP_STRING:
    return typchkInferInterpString(env, node);
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

// Like typchkCheck. When the value is checked by working out its own type,
// that type is stored in `actualOut`, otherwise `actualOut` is left alone.
static bool typchkCheckInto(TypeEnv *env, AstNode *node, Type *expected,
                            Type **actualOut) {
  if (!typchkApplyExpectedNumberType(node, expected))
    return false;

  if (node->kind == NODE_FUNCTION && node->as.function.isLambda)
    return typchkCheckOrInferLambda(env, node, expected) != NULL;

  if (node->kind == NODE_ARRAY && expected != NULL) {
    if (expected->kind != TYPE_LIST && expected->kind != TYPE_ARRAY) {
      typchkErrorAtNodeFmt(node, "Expected %s, got an array.",
                           typeToString(expected));
      return false;
    }

    ArrayNode *a = &node->as.array;
    bool isArray = expected->kind == TYPE_ARRAY;
    Type *elementType = isArray ? expected->as.array.elementType
                                : expected->as.list.elementType;

    a->isFixedSize = isArray;

    bool ok = true;

    if (isArray && a->count != expected->as.array.length) {
      typchkErrorAtNodeFmt(
          node, "Expected %s, got an array literal with %d %s.",
          typeToString(expected), a->count, a->count == 1 ? "item" : "items");
      ok = false;
    }

    for (int i = 0; i < a->count; i++) {
      AstNode *item = a->items[i];

      if (!typchkCheck(env, item, elementType))
        ok = false;
    }

    return ok;
  }

  if (node->kind == NODE_VARIABLE && expected != NULL &&
      expected->kind == TYPE_FN) {
    Type *named = typchkLookupVariableType(env, &node->as.variable.name);

    if (named != NULL && named->kind == TYPE_FN &&
        typchkHasDeclaredGenericParams(named))
      return typchkCheckGenericFunctionValue(env, node, named, expected);
  }

  if (node->kind == NODE_STRUCT_INIT && expected != NULL &&
      expected->kind == TYPE_STRUCT && expected->as.struct_.typeArgCount > 0 &&
      typeIsFullyKnown(expected)) {
    Type *declaration = typchkStructInitDeclaration(env, &node->as.structInit);

    if (declaration != NULL && declaration == typeStructDeclaration(expected) &&
        !typchkStructMembersUnreliable(declaration))
      return typchkCheckGenericStructInit(env, node, expected);
  }

  if (node->kind == NODE_CALL)
    env->_callExpected = expected;

  Type *actual = typchkInfer(env, node);

  if (actual == NULL)
    return true;

  if (expected == NULL)
    return true;

  if (expected->kind == TYPE_FN && actual->kind == TYPE_FN &&
      typchkHasDeclaredGenericParams(actual))
    return typchkCheckGenericFunctionValue(env, node, actual, expected);

  if (!typeAccepts(expected, actual)) {
    typchkErrorAtNodeFmt(node, "Expected %s, got %s.", typeToString(expected),
                         typeToString(actual));
    return false;
  }

  if (actualOut != NULL)
    *actualOut = actual;

  return true;
}

bool typchkCheck(TypeEnv *env, AstNode *node, Type *expected) {
  return typchkCheckInto(env, node, expected, NULL);
}

// True if the struct's members couldn't be type checked
static bool typchkStructMembersUnreliable(Type *type) {
  return typeStructHasUnresolvedMembers(type);
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
    if (lit->as.number.literalIsPlainInteger) {
      if (!lit->as.number.literalValueFits) {
        typchkErrorAtNode(node,
                          "Integer literal is too large for any number type.");
        return NULL;
      }

      if (lit->as.number.literalValue > numberKindMax(NUMBER_KIND_I64)) {
        typchkErrorAtNode(
            node,
            "Integer literal is too large for i64, add a suffix like u64.");
        return NULL;
      }

      return typeNumber(NUMBER_KIND_I64);
    }

    return typeNumber(lit->as.number.kind);
  case LITERAL_STRING:
    return typeString();
  }
  return NULL; // unreachable
}

// A plain integer value next to a number of another type takes that type, so
// `byte + 1` needs no suffix. Used for both sides of an operator and both
// branches of an if/else. Returns false after reporting a literal that can't
// be the other type.
static bool typchkPairNumberType(TypeEnv *env, AstNode *value, Type **type,
                                 Type *otherType) {
  if (!typeIsNumber(otherType) || !typeIsNumber(*type) ||
      typesEqual(*type, otherType) || !typchkHasPlainIntegerSource(value))
    return true;

  if (!typchkApplyExpectedNumberType(value, otherType))
    return false;

  *type = typchkInfer(env, value);
  return *type != NULL;
}

// Infer the type of a unary expressoin
// Whether an operator can work on `type` through a trait, rather than as a
// built-in: a struct, or a generic parameter.
static bool typchkIsTraitOperand(Type *type) {
  return type->kind == TYPE_STRUCT || type->kind == TYPE_GENERIC_PARAM;
}

// Checks that `type` implements the trait of `entry`, which `-a` or `a + b`
// calls a method of. Reports it at `op` when it doesn't.
static bool typchkRequireOperatorTrait(TypeEnv *env, Type *type,
                                       const OperatorTrait *entry, Token *op) {
  InternedName trait = internTokenName(tokenFromCString(entry->trait));

  if (typchkTypeImplementsTrait(env, type, trait))
    return true;

  typchkErrorAtTokenFmt(op, "%s doesn't implement %s, required by '%.*s'.",
                        typeToString(type), entry->trait, op->length,
                        op->start);
  return false;
}

static Type *typchkInferUnary(TypeEnv *env, AstNode *node) {
  UnaryNode *u = &node->as.unary;

  if (u->op.type == TOKEN_BANG) {
    Type *operandType = typchkInfer(env, u->operand);

    if (operandType == NULL)
      return NULL;

    return typeBool();
  }

  // With no expected type, `-literal` is an i64. It stays plain so it can still
  // take the type of a paired value, as in `byte32 + -1`. Only the smallest
  // i64 is folded into one literal now, since its digits alone are too large
  // for an i64.
  if (typchkIsPlainIntegerLiteral(u->operand)) {
    Type *i64 = typeNumber(NUMBER_KIND_I64);
    AstNode *literal = u->operand;

    if (!typchkNegatedPlainIntegerFits(literal, NUMBER_KIND_I64)) {
      typchkReportLiteralMisfit(node, NUMBER_KIND_I64);
      return NULL;
    }

    if (literal->as.literal.as.number.literalValue >
        numberKindMax(NUMBER_KIND_I64))
      typchkFoldNegatedPlainInteger(node, NUMBER_KIND_I64);

    return i64;
  }

  Type *operandType = typchkInfer(env, u->operand);

  if (operandType == NULL)
    return NULL;

  if (typchkIsTraitOperand(operandType)) {
    u->traitDispatch = operatorTraitFor(u->op.type, /*isUnary=*/true);
    return typchkRequireOperatorTrait(env, operandType, u->traitDispatch,
                                      &u->op)
               ? operandType
               : NULL;
  }

  if (!typeIsNumber(operandType)) {
    typchkErrorAtTokenFmt(&u->op, "Can't negate %s.",
                          typeToString(operandType));
    return NULL;
  }

  if (!numberKindInfo(operandType->as.number.kind)->isSigned) {
    typchkErrorAtTokenFmt(&u->op, "Can't negate %s.",
                          typeToString(operandType));
    return NULL;
  }

  return operandType;
}

// Infer the type of a binary expression
// Does the type have a Display impl, and so a toString() method?
static bool typeImplementsDisplay(TypeEnv *env, Type *type) {
  return typchkTypeImplementsTrait(
      env, type, internTokenName(tokenFromCString("Display")));
}

// The user-facing symbol for a binary operator token, used in error messages.
static const char *binaryOperatorSymbol(TokenType type) {
  switch (type) {
  case TOKEN_PLUS:
    return "+";
  case TOKEN_MINUS:
    return "-";
  case TOKEN_STAR:
    return "*";
  case TOKEN_SLASH:
    return "/";
  case TOKEN_MODULO:
    return "%";
  case TOKEN_EQUAL_EQUAL:
    return "==";
  case TOKEN_BANG_EQUAL:
    return "!=";
  case TOKEN_LESS:
    return "<";
  case TOKEN_GREATER:
    return ">";
  case TOKEN_LESS_EQUAL:
    return "<=";
  case TOKEN_GREATER_EQUAL:
    return ">=";
  default:
    return "?";
  }
}

static Type *typchkInferBinary(TypeEnv *env, AstNode *node) {
  BinaryNode *b = &node->as.binary;
  Type *leftType = typchkInfer(env, b->left);
  Type *rightType = typchkInfer(env, b->right);

  if (leftType == NULL || rightType == NULL)
    return NULL;

  if (!typchkPairNumberType(env, b->left, &leftType, rightType) ||
      !typchkPairNumberType(env, b->right, &rightType, leftType))
    return NULL;

  switch (b->op.type) {
  case TOKEN_PLUS:
    if (typeIsNumber(leftType) && typesEqual(leftType, rightType))
      return leftType;

    if (typesEqual(leftType, typeString()) &&
        typesEqual(rightType, typeString())) {
      b->isStringConcat = true;
      return typeString();
    }

    if (typchkIsTraitOperand(leftType) && typesEqual(leftType, rightType)) {
      b->traitDispatch = operatorTraitFor(b->op.type, /*isUnary=*/false);
      return typchkRequireOperatorTrait(env, leftType, b->traitDispatch, &b->op)
                 ? leftType
                 : NULL;
    }

    typchkErrorAtTokenFmt(
        &b->op,
        "'+' needs two numbers of the same type or two strings, got %s and %s.",
        typeToString(leftType), typeToString(rightType));
    return NULL;

  case TOKEN_MINUS:
  case TOKEN_STAR:
  case TOKEN_SLASH:
  case TOKEN_MODULO:
    if (typeIsNumber(leftType) && typesEqual(leftType, rightType))
      return leftType;

    if (typchkIsTraitOperand(leftType) && typesEqual(leftType, rightType)) {
      b->traitDispatch = operatorTraitFor(b->op.type, /*isUnary=*/false);
      return typchkRequireOperatorTrait(env, leftType, b->traitDispatch, &b->op)
                 ? leftType
                 : NULL;
    }

    typchkErrorAtTokenFmt(
        &b->op, "'%s' needs two numbers of the same type, got %s and %s.",
        binaryOperatorSymbol(b->op.type), typeToString(leftType),
        typeToString(rightType));
    return NULL;

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
        !typeStructImplementsTrait(leftType,
                                   internTokenName(tokenFromCString("Eq")))) {
      typchkErrorAtTokenFmt(&b->op,
                            "%s needs 'impl Eq for %s' to support '%s'.",
                            typeToString(leftType), typeToString(leftType),
                            b->op.type == TOKEN_EQUAL_EQUAL ? "==" : "!=");
      return NULL;
    }

    if (typchkIsTraitOperand(leftType)) {
      b->traitDispatch = operatorTraitFor(b->op.type, /*isUnary=*/false);

      if (!typchkRequireOperatorTrait(env, leftType, b->traitDispatch, &b->op))
        return NULL;
    }

    return typeBool();

  case TOKEN_LESS:
  case TOKEN_GREATER:
  case TOKEN_LESS_EQUAL:
  case TOKEN_GREATER_EQUAL:
    if (typeIsNumber(leftType) && typesEqual(leftType, rightType))
      return typeBool();

    if (typchkIsTraitOperand(leftType) && typesEqual(leftType, rightType)) {
      b->traitDispatch = operatorTraitFor(b->op.type, /*isUnary=*/false);
      return typchkRequireOperatorTrait(env, leftType, b->traitDispatch, &b->op)
                 ? typeBool()
                 : NULL;
    }

    typchkErrorAtTokenFmt(
        &b->op, "'%s' needs two numbers of the same type, got %s and %s.",
        binaryOperatorSymbol(b->op.type), typeToString(leftType),
        typeToString(rightType));
    return NULL;

  default:
    typchkErrorAtToken(&b->op, "Internal: unhandled binary operator.");
    return NULL;
  }
}

/**
 * `expr as Type` -- an explicit conversion between number types, e.g.
 * `value as u8`. Only f64 and u8 may appear on either side, so the legal
 * casts are f64->f64, f64->u8, u8->f64, and u8->u8. The u8->f64 widening is
 * always exact. The f64->u8 narrowing is checked at runtime: the value must
 * be an integer from 0 to 255, else the VM raises an error.
 */
static Type *typchkInferCast(TypeEnv *env, AstNode *node) {
  CastNode *c = &node->as.cast;

  Type *sourceType = typchkInfer(env, c->expr);
  if (sourceType == NULL)
    return NULL;

  Type *targetType = typchkResolveType(env, c->type);
  if (targetType == NULL)
    return NULL;

  if (!typeIsNumber(sourceType) || !typeIsNumber(targetType)) {
    typchkErrorAtTokenFmt(&c->as,
                          "Can't cast %s to %s. Only numbers can be cast.",
                          typeToString(sourceType), typeToString(targetType));
    return NULL;
  }

  // The compiler has no types, so it reads the target's number kind from
  // the node.
  c->target = targetType->as.number.kind;
  return targetType;
}

/**
 * Chooses how a value of this type becomes a string. Returns false when it
 * can't: only strings, numbers, bool, and types that implement Display can.
 */
static bool chooseStringConversion(TypeEnv *env, Type *type,
                                   StringConversion *conversion) {
  if (typesEqual(type, typeString())) {
    *conversion = STRING_CONVERSION_STRING;
  } else if (typeIsNumber(type)) {
    *conversion = STRING_CONVERSION_NUMBER;
  } else if (typesEqual(type, typeBool())) {
    *conversion = STRING_CONVERSION_BOOL;
  } else if (typeImplementsDisplay(env, type)) {
    *conversion = STRING_CONVERSION_DISPLAY;
  } else {
    return false;
  }

  return true;
}

// Infer the type of an interpolated string. Also records how each placeholder
// becomes a string, because the compiler has no types to decide that itself.
static Type *typchkInferInterpString(TypeEnv *env, AstNode *node) {
  InterpStringNode *is = &node->as.interpString;

  for (int i = 0; i < is->count; i++) {
    InterpPart *part = &is->parts[i];

    // A NULL type means either an error was already reported, or the type
    // isn't known. Only the second needs its own error.
    bool hadErrorBefore = hadError;
    hadError = false;

    Type *type = typchkInfer(env, part->expr);

    bool partHadError = hadError;
    hadError = hadErrorBefore || partHadError;

    if (type == NULL) {
      if (!partHadError) {
        typchkErrorAtNode(part->expr,
                          "Can't tell the type of this placeholder. Declare it "
                          "before this line, or give it a type, e.g. 'let n: "
                          "f64 = ...;'.");
      }
      continue;
    }

    if (!chooseStringConversion(env, type, &part->conversion)) {
      typchkErrorAtNodeFmt(part->expr,
                           "Can't interpolate %s. Only string, numbers, bool, "
                           "and types that implement Display can be "
                           "interpolated.",
                           typeToString(type));
    }
  }

  return typeString();
}

// The declared type of a variable or function, which may still be generic
static Type *typchkLookupVariableType(TypeEnv *env, Token *name) {
  Type *type = typchkTypeEnvLookup(env, *name);

  if (type == NULL)
    type = typchkTypeEnvLookupFunction(env, *name);

  return type;
}

// True if the type holds a generic parameter that a program declared, as
// opposed to the placeholders in a native's signature
static bool typchkHasDeclaredGenericParams(Type *type) {
  for (int i = 0; i < typeGenericParamCount(type); i++) {
    Type *param = typeFindGenericParam(type, i);

    if (param != NULL && param->as.genericParam.hasName)
      return true;
  }

  return false;
}

// Whether `arg` fails to implement one of the bounds of `param`. The first
// bound it fails is written to `unmetTrait`. A type that isn't fully known yet
// can't be judged.
static bool typchkFirstUnmetBound(TypeEnv *env, Type *param, Type *arg,
                                  InternedName *unmetTrait) {
  if (param == NULL || param->as.genericParam.boundCount == 0 || arg == NULL ||
      !typeIsFullyKnown(arg))
    return false;

  for (int i = 0; i < param->as.genericParam.boundCount; i++) {
    InternedName bound = param->as.genericParam.boundTraits[i];

    if (!typchkTypeImplementsTrait(env, arg, bound)) {
      *unmetTrait = bound;
      return true;
    }
  }

  return false;
}

static const char *typchkPrimitiveOwner(const Type *type);
static bool typchkPrimitiveImplements(TypeEnv *env, const char *owner,
                                      InternedName trait);

// Writes the start of an unmet-bound message into `out`. A primitive whose
// standard library impl exists is told apart, because the impl isn't the
// problem: bounds can't be filled by a primitive yet.
static void typchkUnmetBoundText(TypeEnv *env, Type *arg, InternedName bound,
                                 char *out, size_t size) {
  const char *owner = typchkPrimitiveOwner(arg);

  if (owner != NULL && typchkPrimitiveImplements(env, owner, bound)) {
    snprintf(out, size,
             "%s implements %.*s, but a primitive can't be used with a bound "
             "yet",
             typeToString(arg), bound.length, internedNameChars(bound));
    return;
  }

  snprintf(out, size, "%s doesn't implement %.*s", typeToString(arg),
           bound.length, internedNameChars(bound));
}

// Checks the type arguments of the generic struct or alias `owner` against the
// bounds of its parameters. An argument that doesn't meet one is reported at
// `token`, or at `node` when `token` is NULL.
static bool typchkTypeArgsMeetBounds(TypeEnv *env, InternedName owner,
                                     Type **params, Type **args, int count,
                                     Token *token, AstNode *node) {
  if (params == NULL)
    return true;

  for (int i = 0; i < count; i++) {
    InternedName bound;

    if (!typchkFirstUnmetBound(env, params[i], args[i], &bound))
      continue;

    char text[256];
    typchkUnmetBoundText(env, args[i], bound, text, sizeof(text));

    const char *format = "%s, required by %.*s.";

    if (token != NULL)
      typchkErrorAtTokenFmt(token, format, text, owner.length,
                            internedNameChars(owner));
    else
      typchkErrorAtNodeFmt(node, format, text, owner.length,
                           internedNameChars(owner));

    return false;
  }

  return true;
}

// The index of the first generic parameter of `genericType` that was filled
// with a type that doesn't implement one of the parameter's bounds, or -1.
// The trait that is missing is written to `unmetTrait`. A parameter that
// wasn't filled, or whose type isn't fully known yet, can't be judged.
static int typchkUnmetBoundIndex(TypeEnv *env, Type *genericType,
                                 Type **bindings, int count,
                                 InternedName *unmetTrait) {
  for (int i = 0; i < count; i++) {
    Type *param = typeFindGenericParam(genericType, i);

    if (typchkFirstUnmetBound(env, param, bindings[i], unmetTrait))
      return i;
  }

  return -1;
}

static void typchkErrorUnmetBound(TypeEnv *env, AstNode *node,
                                  Type *genericType, Type **bindings, int index,
                                  InternedName bound) {
  Type *param = typeFindGenericParam(genericType, index);
  InternedName name = param->as.genericParam.name;

  char text[256];
  typchkUnmetBoundText(env, bindings[index], bound, text, sizeof(text));

  typchkErrorAtNodeFmt(node, "%s, required by %.*s.", text, name.length,
                       internedNameChars(name));
}

// A generic function type with its parameters filled in from `expected`,
// which is a function type that may be partly unknown. A parameter filled
// with a type that doesn't implement its bound is reported at `node`, and the
// result is NULL.
static Type *typchkFillGenericFunctionValue(TypeEnv *env, AstNode *node,
                                            Type *genericType, Type *expected) {
  int count = typeGenericParamCount(genericType);
  Type **bindings = (Type **)typesAllocRaw((size_t)count * sizeof(Type *));
  memset(bindings, 0, (size_t)count * sizeof(Type *));

  if (genericType->as.function.paramCount == expected->as.function.paramCount) {
    for (int i = 0; i < genericType->as.function.paramCount; i++) {
      typeBindGenericParams(genericType->as.function.paramTypes[i],
                            expected->as.function.paramTypes[i], bindings,
                            count);
    }

    typeBindGenericParams(genericType->as.function.returnType,
                          expected->as.function.returnType, bindings, count);
  }

  InternedName unmetTrait;
  int unmet =
      typchkUnmetBoundIndex(env, genericType, bindings, count, &unmetTrait);

  if (unmet >= 0) {
    typchkErrorUnmetBound(env, node, genericType, bindings, unmet, unmetTrait);
    return NULL;
  }

  return typeSubstituteGenericParams(genericType, bindings, count);
}

// A generic function used as a value where a function type is expected, such
// as `let f: fun (i64) => i64 = id;`. The expected type fills in the
// function's generic parameters, and what is left must be accepted by it.
static bool typchkCheckGenericFunctionValue(TypeEnv *env, AstNode *node,
                                            Type *genericType, Type *expected) {
  Type *filled =
      typchkFillGenericFunctionValue(env, node, genericType, expected);

  if (filled == NULL)
    return false;

  if (typeAccepts(expected, typeEraseGenericParams(filled)))
    return true;

  typchkErrorAtNodeFmt(node, "Expected %s, got %s.", typeToString(expected),
                       typeToString(filled));
  return false;
}

// Infer the type of an variable/identifier expression
static Type *typchkInferVariable(TypeEnv *env, AstNode *node) {
  Type *type = typchkLookupVariableType(env, &node->as.variable.name);

  if (type == NULL)
    return NULL;

  // A generic function used as a value, such as `let pop = @listPop;`, has no
  // single type to give it.
  if (typeGenericParamCount(type) > 0)
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

// Reads a constant number out of an expression: a number literal, possibly
// negated with '-' and possibly wrapped in parentheses. Anything else, such as
// a variable or arithmetic, isn't treated as constant.
static bool typchkConstantNumber(AstNode *node, double *value) {
  switch (node->kind) {
  case NODE_LITERAL:
    if (node->as.literal.kind != LITERAL_NUMBER)
      return false;

    *value = numberToDouble(node->as.literal.as.number.kind,
                            node->as.literal.as.number.as);
    return true;
  case NODE_GROUPING:
    return typchkConstantNumber(node->as.grouping.inner, value);
  case NODE_UNARY:
    if (node->as.unary.op.type != TOKEN_MINUS)
      return false;
    if (!typchkConstantNumber(node->as.unary.operand, value))
      return false;
    *value = -*value;
    return true;
  default:
    return false;
  }
}

// Some natives can never accept certain arguments. When the argument is a
// constant, report it now instead of when the call runs. Arguments that aren't
// constant are still checked at runtime by the native itself.
static void typchkCheckNativeConstantArgs(AstNode *node) {
  CallNode *c = &node->as.call;

  for (int i = 0; i < nativeArgConstraintCount; i++) {
    const NativeArgConstraint *constraint = &nativeArgConstraints[i];
    double value;

    if (constraint->paramIndex < c->argCount &&
        tokenTextEquals(&c->callee->as.variable.name, constraint->name) &&
        typchkConstantNumber(c->args[constraint->paramIndex], &value) &&
        !constraint->isValid(value)) {
      typchkErrorAtNodeFmt(
          c->args[constraint->paramIndex],
          "function %s expects argument %d to be %s but got %g.",
          constraint->name, constraint->paramIndex + 1, constraint->expectation,
          value);
    }
  }
}

// Infer the type of a call expression
// Is this @print, @println, @eprint, or @eprintln?
static bool isPrintNative(Token *name) {
  static const char *printNatives[] = {"@print", "@println", "@eprint",
                                       "@eprintln"};

  for (size_t i = 0; i < sizeof(printNatives) / sizeof(*printNatives); i++) {
    if ((int)strlen(printNatives[i]) == name->length &&
        memcmp(printNatives[i], name->start, (size_t)name->length) == 0)
      return true;
  }

  return false;
}

static Type *typchkInferCall(TypeEnv *env, AstNode *node) {
  CallNode *c = &node->as.call;
  Type *expected = env->_callExpected;
  env->_callExpected = NULL;

  if (c->callee->kind == NODE_VARIABLE) {
    Token *name = &c->callee->as.variable.name;
    Type *calleeType = typchkTypeEnvLookup(env, *name);

    if (calleeType == NULL)
      calleeType = typchkTypeEnvLookupFunction(env, *name);

    if (calleeType == NULL) {
      // Unable to resolve called function (possible a native function)
      // Run typchkInfer over args to report any type errors they might contain
      for (int i = 0; i < c->argCount; i++) {
        Type *argType = typchkInfer(env, c->args[i]);

        if (i == 0 && isPrintNative(name) &&
            typeImplementsDisplay(env, argType))
          c->argUsesDisplay = true;
      }

      return NULL;
    }

    env->_callExpected = expected;
    Type *returnType =
        typchkCheckCallAgainstFunctionType(env, node, calleeType);

    if (returnType != NULL)
      // Used to validate args passed native functions like `@sqrt`
      typchkCheckNativeConstantArgs(node);

    return returnType;
  }

  env->_calleeNode = c->callee;
  Type *calleeType = typchkInfer(env, c->callee);
  env->_calleeNode = NULL;

  if (calleeType == NULL) {
    for (int i = 0; i < c->argCount; i++) {
      AstNode *arg = c->args[i];

      typchkInfer(env, arg);
    }

    return NULL;
  }

  env->_callExpected = expected;
  return typchkCheckCallAgainstFunctionType(env, node, calleeType);
}

// Checks one argument, or one field of a struct literal, against a type that
// may still hold generic parameters. A type with none left is checked like any
// other. Otherwise the value's type fills them, as `[i64]` fills T in `[T]`.
// The value is checked once. Sets `sawUnknown` when its type isn't known.
// Returns false after reporting an error.
static bool typchkUnifyValue(TypeEnv *env, AstNode *arg, Type *pattern,
                             Type **bindings, int genericCount,
                             bool *sawUnknown) {
  Type *param = typeSubstituteGenericParams(pattern, bindings, genericCount);

  if (typeGenericParamCount(param) == 0)
    return typchkCheck(env, arg, param);

  Type *known = typeEraseGenericParams(param);
  Type *actual;

  if (arg->kind == NODE_ARRAY && arg->as.array.count == 0 &&
      param->kind == TYPE_LIST) {
    // A list is expected, so `[]` is a list. Its item type is not known
    // yet, and the generic parameter stays unfilled
    actual = typeList(NULL);
  } else if (arg->kind == NODE_FUNCTION && arg->as.function.isLambda) {
    actual = typchkCheckOrInferLambda(env, arg, known);
  } else {
    actual = typchkInfer(env, arg);

    if (actual == NULL && arg->kind == NODE_VARIABLE)
      actual = typchkLookupVariableType(env, &arg->as.variable.name);

    if (actual != NULL && actual->kind == TYPE_FN && known->kind == TYPE_FN &&
        actual->as.function.paramCount == known->as.function.paramCount &&
        typchkHasDeclaredGenericParams(actual)) {
      Type *filled = typchkFillGenericFunctionValue(env, arg, actual, known);

      if (filled == NULL)
        return false;

      actual = typeEraseGenericParams(filled);
    }
  }

  if (actual == NULL) {
    *sawUnknown = true;
    return true;
  }

  // A literal given where a list is expected is a list, as with a
  // parameter that has no generic parameters in it.
  if (arg->kind == NODE_ARRAY && param->kind == TYPE_LIST &&
      actual->kind == TYPE_ARRAY) {
    arg->as.array.isFixedSize = false;
    actual = typeList(actual->as.array.elementType);
  }

  if (!typeAccepts(known, actual)) {
    typchkErrorAtNodeFmt(arg, "Expected %s, got %s.", typeToString(param),
                         typeToString(actual));
    return false;
  }

  typeBindGenericParams(param, actual, bindings, genericCount);
  return true;
}

// The first argument of a call whose parameter type mentions generic parameter
// `index`, to point an error at. The call itself when none does, as when only
// the type the call is expected to return filled it.
static AstNode *typchkArgumentOfParam(AstNode *call, Type *calleeType,
                                      int index) {
  CallNode *c = &call->as.call;

  for (int i = 0; i < c->argCount; i++) {
    if (typeFindGenericParam(calleeType->as.function.paramTypes[i], index) !=
        NULL)
      return c->args[i];
  }

  return call;
}

// Checks a call to a function with generic parameters, such as
// `fun ([T], T) => unit`. Goes through the arguments in order, each checked
// once. Generic parameters that are never filled are unknown for a native.
static Type *typchkCheckGenericCall(TypeEnv *env, AstNode *node,
                                    Type *calleeType, int genericCount,
                                    Type *expected) {
  CallNode *c = &node->as.call;
  Type **bindings =
      (Type **)typesAllocRaw((size_t)genericCount * sizeof(Type *));
  memset(bindings, 0, (size_t)genericCount * sizeof(Type *));
  bool ok = true;
  bool sawUnknownArg = false;

  for (int i = 0; i < c->argCount; i++) {
    if (!typchkUnifyValue(env, c->args[i],
                          calleeType->as.function.paramTypes[i], bindings,
                          genericCount, &sawUnknownArg))
      ok = false;
  }

  if (!ok)
    return NULL;

  // What the call is expected to return fills any parameter the arguments left
  // empty, such as the `T` of `fun new(): Self[T]`
  if (expected != NULL) {
    typeBindGenericParams(calleeType->as.function.returnType, expected,
                          bindings, genericCount);
  }

  // A parameter the program declared has to be filled in by the arguments. A
  // native's stays unknown. An argument that already failed to check explains
  // a missing one, so it isn't reported again.
  for (int i = 0; i < genericCount && !sawUnknownArg; i++) {
    Type *param = typeFindGenericParam(calleeType, i);

    if (bindings[i] == NULL && param != NULL &&
        param->as.genericParam.hasName) {
      typchkErrorAtTokenFmt(&c->paren,
                            "Can't tell what '%.*s' is from the arguments.",
                            param->as.genericParam.name.length,
                            internedNameChars(param->as.genericParam.name));
      return NULL;
    }
  }

  // Each type that filled a parameter has to implement the parameter's bound
  InternedName unmetTrait;
  int unmet = typchkUnmetBoundIndex(env, calleeType, bindings, genericCount,
                                    &unmetTrait);

  if (unmet >= 0) {
    typchkErrorUnmetBound(env, typchkArgumentOfParam(node, calleeType, unmet),
                          calleeType, bindings, unmet, unmetTrait);
    return NULL;
  }

  return typeEraseGenericParams(typeSubstituteGenericParams(
      calleeType->as.function.returnType, bindings, genericCount));
}

static Type *typchkCheckCallAgainstFunctionType(TypeEnv *env, AstNode *node,
                                                Type *calleeType) {
  CallNode *c = &node->as.call;
  Type *expected = env->_callExpected;
  env->_callExpected = NULL;

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

  int genericCount = typeGenericParamCount(calleeType);

  if (genericCount > 0)
    return typchkCheckGenericCall(env, node, calleeType, genericCount,
                                  expected);

  bool ok = true;
  for (int i = 0; i < c->argCount; i++) {
    if (!typchkCheck(env, c->args[i], calleeType->as.function.paramTypes[i]))
      ok = false;
  }
  if (!ok)
    return NULL;

  return calleeType->as.function.returnType;
}

// The primitive type a name refers to, or NULL if it isn't one
static Type *typchkPrimitiveTypeNamed(const Token *name) {
  if (tokenTextEquals(name, "unit"))
    return typeUnit();
  if (tokenTextEquals(name, "bool"))
    return typeBool();
  if (tokenTextEquals(name, "string"))
    return typeString();

  NumberKind kind;
  if (numberKindFromName(name->start, name->length, &kind))
    return typeNumber(kind);

  return NULL;
}

// The name primitive methods are grouped under, or NULL if the type can't have
// methods. The name is the type's own spelling, such as "string" or "u8", and
// lives for the whole program
static const char *typchkPrimitiveOwner(const Type *type) {
  switch (type->kind) {
  case TYPE_STRING:
    return "string";
  case TYPE_BOOL:
    return "bool";
  case TYPE_UNIT:
    return "unit";
  case TYPE_LIST:
    return "list";
  case TYPE_ARRAY:
    return "array";
  case TYPE_NUMBER:
    return numberKindInfo(type->as.number.kind)->name;
  default:
    return NULL;
  }
}

// The method an `impl string { ... }` block declared, not one from a trait
static PrimitiveMethod *
typchkPrimitiveMethodLookup(TypeEnv *env, const char *owner, Token name) {
  InternedName interned = internTokenName(name);

  for (int i = 0; i < env->primitiveMethodCount; i++) {
    PrimitiveMethod *method = &env->primitiveMethods[i];

    if (!method->isTrait && strcmp(method->owner, owner) == 0 &&
        internedNamesEqual(method->name, interned))
      return method;
  }

  return NULL;
}

// The method a trait gave the type. With a `trait`, only that trait's method
// counts. At most one trait can provide a given name to one type.
static PrimitiveMethod *
typchkPrimitiveTraitMethodLookup(TypeEnv *env, const char *owner, Token name,
                                 const InternedName *trait) {
  InternedName interned = internTokenName(name);

  for (int i = 0; i < env->primitiveMethodCount; i++) {
    PrimitiveMethod *method = &env->primitiveMethods[i];

    if (method->isTrait && strcmp(method->owner, owner) == 0 &&
        internedNamesEqual(method->name, interned) &&
        (trait == NULL || internedNamesEqual(method->trait, *trait)))
      return method;
  }

  return NULL;
}

static PrimitiveMethod *typchkPrimitiveMethodAppend(TypeEnv *env) {
  if (env->primitiveMethodCount == env->primitiveMethodCapacity) {
    env->primitiveMethodCapacity =
        env->primitiveMethodCapacity < 8 ? 8 : env->primitiveMethodCapacity * 2;
    env->primitiveMethods = (PrimitiveMethod *)realloc(
        env->primitiveMethods,
        sizeof(PrimitiveMethod) * (size_t)env->primitiveMethodCapacity);
    if (env->primitiveMethods == NULL) {
      fprintf(stderr, "realloc failed in typchkPrimitiveMethodAppend\n");
      exit(1);
    }
  }

  return &env->primitiveMethods[env->primitiveMethodCount++];
}

static void typchkPrimitiveMethodAdd(TypeEnv *env, const char *owner,
                                     Token name, Type *type, bool isPublic) {
  *typchkPrimitiveMethodAppend(env) =
      (PrimitiveMethod){owner,
                        internTokenName(name),
                        type,
                        isPublic,
                        true,
                        false,
                        {0},
                        env->_currentImplReceiver,
                        env->_implGenericCount};
}

static void typchkPrimitiveTraitMethodAdd(TypeEnv *env, const char *owner,
                                          Token name, Type *type, bool hasSelf,
                                          InternedName trait) {
  *typchkPrimitiveMethodAppend(env) =
      (PrimitiveMethod){owner,
                        internTokenName(name),
                        type,
                        true,
                        hasSelf,
                        true,
                        trait,
                        env->_currentImplReceiver,
                        env->_implGenericCount};
}

static bool typchkPrimitiveImplements(TypeEnv *env, const char *owner,
                                      InternedName trait) {
  for (int i = 0; i < env->primitiveTraitImplCount; i++) {
    PrimitiveTraitImpl *impl = &env->primitiveTraitImpls[i];

    if (strcmp(impl->owner, owner) == 0 &&
        internedNamesEqual(impl->trait, trait))
      return true;
  }

  return false;
}

static void typchkPrimitiveImplementsAdd(TypeEnv *env, const char *owner,
                                         InternedName trait) {
  if (env->primitiveTraitImplCount == env->primitiveTraitImplCapacity) {
    env->primitiveTraitImplCapacity = env->primitiveTraitImplCapacity < 8
                                          ? 8
                                          : env->primitiveTraitImplCapacity * 2;
    env->primitiveTraitImpls = (PrimitiveTraitImpl *)realloc(
        env->primitiveTraitImpls,
        sizeof(PrimitiveTraitImpl) * (size_t)env->primitiveTraitImplCapacity);
    if (env->primitiveTraitImpls == NULL) {
      fprintf(stderr, "realloc failed in typchkPrimitiveImplementsAdd\n");
      exit(1);
    }
  }

  env->primitiveTraitImpls[env->primitiveTraitImplCount++] =
      (PrimitiveTraitImpl){owner, trait};
}

// Binding slots enough for the item type, the method's own type parameters,
// and the array length placeholders
#define PRIMITIVE_BINDING_SLOTS 8

// The type of `method` called on a value of type `receiver`. For a list or
// array impl, the impl's parameters take what the receiver has, so `first` of
// `impl [T]` called on `[i64]` gives `i64`.
static Type *typchkPrimitiveMethodType(const PrimitiveMethod *method,
                                       Type *receiver) {
  if (method->receiver == NULL)
    return method->type;

  Type *bindings[PRIMITIVE_BINDING_SLOTS] = {0};
  typeBindGenericParams(method->receiver, receiver, bindings,
                        PRIMITIVE_BINDING_SLOTS);
  return typeSubstituteGenericParams(method->type, bindings,
                                     PRIMITIVE_BINDING_SLOTS);
}

// Marks a call of a trait's method on a primitive for the compiler. The
// compiler outlives the checker's name storage, so it gets a copy of the trait
// name.
static void typchkMarkPrimitiveTraitCall(GetNode *get, const char *owner,
                                         const PrimitiveMethod *method) {
  int length = method->trait.length;
  char *trait = (char *)astAllocRaw((size_t)length + 1);
  memcpy(trait, internedNameChars(method->trait), (size_t)length);
  trait[length] = '\0';

  get->primitiveOwner = owner;
  get->primitiveTrait = trait;
  get->primitiveIsStatic = !method->hasSelf;
}

// `f64.default()`, or `Self.default()` inside an impl for a primitive.
// Returns false when `name` doesn't name a primitive, and true when it did,
// with `*result` the method's type or NULL after an error.
static bool typchkInferPrimitiveStaticGet(TypeEnv *env, GetNode *get,
                                          Token *name, bool isCallee,
                                          Type **result) {
  Type *primitive = NULL;

  if (tokenTextEquals(name, "Self")) {
    if (env->_currentPrimitiveOwner != NULL)
      primitive = env->_currentImplSelfType;
  } else {
    primitive = typchkPrimitiveTypeNamed(name);
  }

  if (primitive == NULL)
    return false;

  *result = NULL;

  const char *owner = typchkPrimitiveOwner(primitive);
  assert(owner != NULL);

  PrimitiveMethod *method =
      typchkPrimitiveTraitMethodLookup(env, owner, get->name, NULL);

  if (method == NULL || method->hasSelf) {
    bool isInstanceMethod =
        (method != NULL && method->hasSelf) ||
        typchkPrimitiveMethodLookup(env, owner, get->name) != NULL;

    if (isInstanceMethod) {
      typchkErrorAtTokenFmt(&get->name,
                            "'%.*s' is an instance method. Call it on a "
                            "value of '%s'.",
                            get->name.length, get->name.start, owner);
    } else {
      typchkErrorAtTokenFmt(&get->name, "%s has no static method '%.*s'.",
                            owner, get->name.length, get->name.start);
    }
    return true;
  }

  if (!isCallee) {
    typchkErrorAtTokenFmt(&get->name, "Method '%.*s' of '%s' has to be called.",
                          get->name.length, get->name.start, owner);
    return true;
  }

  typchkMarkPrimitiveTraitCall(get, owner, method);
  *result = method->type;
  return true;
}

// Infer the type of a get expression
static Type *typchkInferGet(TypeEnv *env, AstNode *node) {
  GetNode *get = &node->as.get;

  // Read before inferring the object, which may contain calls of its own
  bool isCallee = env->_calleeNode == node;

  if (get->object->kind == NODE_VARIABLE) {
    Token *objIdentifier = &get->object->as.variable.name;

    bool shadowed = typchkTypeEnvLookup(env, *objIdentifier) != NULL ||
                    typchkTypeEnvLookupFunction(env, *objIdentifier) != NULL;

    if (!shadowed &&
        bindingArrayLookup(env->_genericParams, env->_genericParamCount,
                           *objIdentifier) != NULL) {
      typchkErrorAtTokenFmt(
          objIdentifier, "Can't call a static method on type parameter '%.*s'.",
          objIdentifier->length, objIdentifier->start);
      return NULL;
    }

    if (!shadowed) {
      bool isSelf = tokenTextEquals(objIdentifier, "Self");
      Type *structType = isSelf
                             ? typchkTypeEnvGetImplTargetType(env)
                             : typchkTypeEnvLookupStruct(env, *objIdentifier);

      if (structType == NULL) {
        Type *staticResult;

        if (typchkInferPrimitiveStaticGet(env, get, objIdentifier, isCallee,
                                          &staticResult))
          return staticResult;
      }

      if (structType == NULL && isSelf) {
        typchkErrorSelfIsNotAStruct(env, objIdentifier);
        return NULL;
      }

      if (structType != NULL) {
        if (typchkStructMembersUnreliable(structType))
          return NULL;

        Type *methodType = typeStructStaticMethodLookup(structType, get->name);
        bool isOwnMethod = methodType != NULL;

        if (methodType == NULL) {
          methodType = typeStructTraitStaticMethodLookup(structType, get->name);
        }

        if (methodType == NULL) {
          typchkErrorAtTokenFmt(&get->name, "%s has no static method '%.*s'.",
                                typeToString(structType), get->name.length,
                                get->name.start);
          return NULL;
        }

        // Trait methods are always public; only an own (plain-impl)
        // method can be private.
        bool isPrivate = !typeStructStaticMethodIsPublic(structType, get->name);

        if (isOwnMethod && isPrivate &&
            typchkTypeEnvGetImplTargetType(env) !=
                typeStructDeclaration(structType)) {
          typchkErrorAtTokenFmt(&get->name, "Method '%.*s' is private to '%s'.",
                                get->name.length, get->name.start,
                                typeToString(structType));
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

  // A generic parameter has the methods of its bounds, and nothing else
  if (objectType->kind == TYPE_GENERIC_PARAM &&
      objectType->as.genericParam.boundCount > 0) {
    Type *methodType = NULL;
    InternedName first, second;
    BoundMethodResult result = typchkBoundMethodLookup(
        env, objectType, get->name, &methodType, &first, &second);

    if (result == BOUND_METHOD_AMBIGUOUS) {
      typchkErrorAtTokenFmt(
          &get->name, "Method '%.*s' is in both %.*s and %.*s.",
          get->name.length, get->name.start, first.length,
          internedNameChars(first), second.length, internedNameChars(second));
      return NULL;
    }

    if (result == BOUND_METHOD_MISSING) {
      char names[256] = "";
      bool unresolved = false;
      int count = objectType->as.genericParam.boundCount;

      for (int i = 0; i < count; i++) {
        InternedName bound = objectType->as.genericParam.boundTraits[i];
        Type *boundTrait =
            typchkTypeEnvLookupTrait(env, internedNameToToken(bound));

        unresolved = unresolved || typeTraitHasUnresolvedMembers(boundTrait);
        size_t used = strlen(names);
        snprintf(names + used, sizeof(names) - used, "%s%.*s",
                 i == 0 ? "" : " + ", bound.length, internedNameChars(bound));
      }

      // A trait that didn't resolve was already reported
      if (!unresolved)
        typchkErrorAtTokenFmt(
            &get->name, "%s's %s '%s' %s no method '%.*s'.",
            typeToString(objectType), count == 1 ? "bound" : "bounds", names,
            count == 1 ? "has" : "have", get->name.length, get->name.start);
    }

    return methodType;
  }

  const char *primitiveOwner = typchkPrimitiveOwner(objectType);

  if (primitiveOwner != NULL) {
    PrimitiveMethod *method =
        typchkPrimitiveMethodLookup(env, primitiveOwner, get->name);

    if (method != NULL) {
      if (!method->isPublic && env->_currentPrimitiveOwner != primitiveOwner) {
        typchkErrorAtTokenFmt(&get->name, "Method '%.*s' is private to '%s'.",
                              get->name.length, get->name.start,
                              primitiveOwner);
        return NULL;
      }

      if (!isCallee) {
        typchkErrorAtTokenFmt(
            &get->name, "Method '%.*s' of '%s' has to be called.",
            get->name.length, get->name.start, primitiveOwner);
        return NULL;
      }

      get->primitiveOwner = primitiveOwner;
      return typchkPrimitiveMethodType(method, objectType);
    }

    PrimitiveMethod *traitMethod =
        typchkPrimitiveTraitMethodLookup(env, primitiveOwner, get->name, NULL);

    if (traitMethod != NULL && !traitMethod->hasSelf) {
      typchkErrorAtTokenFmt(&get->name,
                            "'%.*s' is a static method. Access it on '%s' "
                            "instead of a value.",
                            get->name.length, get->name.start, primitiveOwner);
      return NULL;
    }

    if (traitMethod != NULL) {
      if (!isCallee) {
        typchkErrorAtTokenFmt(
            &get->name, "Method '%.*s' of '%s' has to be called.",
            get->name.length, get->name.start, primitiveOwner);
        return NULL;
      }

      typchkMarkPrimitiveTraitCall(get, primitiveOwner, traitMethod);
      return typchkPrimitiveMethodType(traitMethod, objectType);
    }
  }

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
  if (methodType != NULL) {
    if (!typeStructInstanceMethodIsPublic(objectType, get->name) &&
        typchkTypeEnvGetImplTargetType(env) !=
            typeStructDeclaration(objectType)) {
      typchkErrorAtTokenFmt(&get->name, "Method '%.*s' is private to '%s'.",
                            get->name.length, get->name.start,
                            typeToString(objectType));
      return NULL;
    }
    return methodType;
  }

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

// Reports an index that is a number literal past the end of an array.
static bool typchkCheckLiteralIndexInRange(AstNode *index, Type *arrayType) {
  if (arrayType->kind != TYPE_ARRAY || index->kind != NODE_LITERAL ||
      index->as.literal.kind != LITERAL_NUMBER)
    return true;

  LiteralNode *literal = &index->as.literal;

  if (!literal->as.number.literalIsPlainInteger ||
      !literal->as.number.literalValueFits ||
      literal->as.number.literalValue < (uint64_t)arrayType->as.array.length)
    return true;

  typchkErrorAtNodeFmt(index, "Index %llu is out of range for %s.",
                       (unsigned long long)literal->as.number.literalValue,
                       typeToString(arrayType));
  return false;
}

// Infer the type of an index access get expression
static Type *typchkInferIndexGet(TypeEnv *env, AstNode *node) {
  IndexGetNode *ig = &node->as.indexGet;
  Type *objectType = typchkInfer(env, ig->object);

  if (objectType == NULL) {
    typchkInfer(env, ig->index);
    return NULL;
  }

  if (objectType->kind != TYPE_LIST && objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&ig->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!typchkCheckIndex(env, ig->index) ||
      !typchkCheckLiteralIndexInRange(ig->index, objectType))
    return NULL;

  return objectType->kind == TYPE_ARRAY ? objectType->as.array.elementType
                                        : objectType->as.list.elementType;
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

  if (objectType->kind != TYPE_LIST && objectType->kind != TYPE_ARRAY) {
    typchkErrorAtTokenFmt(&is->bracket, "Can't index into a %s.",
                          typeToString(objectType));
    return NULL;
  }

  if (!typchkCheckIndex(env, is->index) ||
      !typchkCheckLiteralIndexInRange(is->index, objectType))
    return NULL;

  Type *elementType = objectType->kind == TYPE_ARRAY
                          ? objectType->as.array.elementType
                          : objectType->as.list.elementType;
  if (elementType != NULL) {
    if (!typchkCheck(env, is->value, elementType))
      return NULL;
  } else {
    typchkInfer(env, is->value); // nothing to check against yet (empty-array
                                 // case), still walk for internal errors
  }

  return elementType;
}

// The struct a literal names, or NULL. Reports nothing.
static Type *typchkStructInitDeclaration(TypeEnv *env, StructInitNode *si) {
  if (tokenTextEquals(&si->name, "Self"))
    return typchkTypeEnvGetImplTargetType(env);

  return typchkTypeEnvLookupStruct(env, si->name);
}

// Infers the type arguments of a literal of a generic struct, such as
// `Box { value: 5 }`, from its fields.
static Type *typchkInferGenericStructInit(TypeEnv *env, AstNode *node,
                                          Type *declaration) {
  StructInitNode *si = &node->as.structInit;
  int count = declaration->as.struct_.genericParamCount;
  Type **bindings = (Type **)typesAllocRaw((size_t)count * sizeof(Type *));
  memset(bindings, 0, (size_t)count * sizeof(Type *));
  bool ok = true;
  bool sawUnknown = false;

  for (int i = 0; i < si->fieldCount; i++) {
    StructInitFieldNode *field = &si->fields[i];
    Type *fieldType = typeStructFieldLookup(declaration, field->name);

    if (fieldType == NULL) {
      typchkErrorAtTokenFmt(&field->name, "%s has no field '%.*s'.",
                            typeToString(declaration), field->name.length,
                            field->name.start);
      ok = false;
      continue;
    }

    if (!typchkUnifyValue(env, field->value, fieldType, bindings, count,
                          &sawUnknown))
      ok = false;
  }

  if (!ok)
    return NULL;

  for (int i = 0; i < count && !sawUnknown; i++) {
    if (bindings[i] == NULL) {
      typchkErrorAtTokenFmt(
          &si->name,
          "Can't tell the type arguments of '%.*s' from its fields. Give the "
          "variable a type, such as `Box[i64]`.",
          si->name.length, si->name.start);
      return NULL;
    }
  }

  if (!typchkTypeArgsMeetBounds(env, declaration->as.struct_.name,
                                declaration->as.struct_.genericParams, bindings,
                                count, /*token=*/NULL, node))
    return NULL;

  return typeStructInstance(declaration, bindings, count);
}

// Checks a literal against an instance of its generic struct that the
// program asked for, such as `let b: Box[f64] = Box { value: 1 }`. Each field
// is checked against the instance's field type, so a plain `1` can be an f64.
static bool typchkCheckGenericStructInit(TypeEnv *env, AstNode *node,
                                         Type *expected) {
  StructInitNode *si = &node->as.structInit;
  bool ok = true;

  for (int i = 0; i < si->fieldCount; i++) {
    StructInitFieldNode *field = &si->fields[i];
    Type *fieldType = typeStructFieldLookup(expected, field->name);

    if (fieldType == NULL) {
      typchkErrorAtTokenFmt(&field->name, "%s has no field '%.*s'.",
                            typeToString(expected), field->name.length,
                            field->name.start);
      ok = false;
      continue;
    }

    if (!typchkCheck(env, field->value, fieldType))
      ok = false;
  }

  return ok;
}

// Infer the type of a struct initialization expression
static Type *typchkInferStructInit(TypeEnv *env, AstNode *node) {
  StructInitNode *si = &node->as.structInit;

  Type *structType;
  if (tokenTextEquals(&si->name, "Self")) {
    structType = typchkTypeEnvGetImplTargetType(env);
    if (structType == NULL) {
      typchkErrorSelfIsNotAStruct(env, &si->name);
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

  if (typeStructIsGeneric(structType))
    return typchkInferGenericStructInit(env, node, structType);

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
  if (a->count == 0) {
    typchkErrorAtNode(node, "Can't tell the item type of an empty literal. "
                            "Add a type, such as `let xs: [i64] = [];`.");
    return NULL;
  }

  a->isFixedSize = true;

  // Plain integer elements take the type of the first element that has a type
  // of its own, so `[1, 2.5]` is an array of f64.
  int typedIndex = 0;
  for (int i = 0; i < a->count; i++) {
    if (!typchkIsPlainIntegerArithmetic(a->items[i])) {
      typedIndex = i;
      break;
    }
  }

  Type *elementType = typchkInfer(env, a->items[typedIndex]);
  bool ok = true;
  for (int i = 0; i < a->count; i++) {
    if (i == typedIndex)
      continue;
    if (elementType != NULL) {
      if (!typchkCheck(env, a->items[i], elementType))
        ok = false;
    } else {
      typchkInfer(env, a->items[i]); // still walk for internal errors
    }
  }
  if (!ok)
    return NULL;
  return typeArray(elementType, a->count);
}

// Whether a node always jumps e.g. return, break, continue
//
// This means any statement in a block or it's optional implicit value, both
// branches of if
static bool typchkAlwaysJumps(AstNode *node) {
  if (node == NULL)
    return false;

  switch (node->kind) {
  case NODE_RETURN:
  case NODE_BREAK:
  case NODE_CONTINUE:
    return true;

  case NODE_BLOCK: {
    BlockNode *block = &node->as.block;
    for (int i = 0; i < block->count; i++) {
      if (typchkAlwaysJumps(block->stmts[i]))
        return true;
    }
    return typchkAlwaysJumps(block->value);
  }

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    return typchkAlwaysJumps(if_->thenBranch) &&
           typchkAlwaysJumps(if_->elseBranch);
  }

  default:
    return false;
  }
}

// Infer the type of an if expression. A diverging branch doesn't affect it.
static Type *typchkInferIf(TypeEnv *env, AstNode *node) {
  IfNode *i = &node->as.if_;
  typchkCheck(env, i->condition,
              typeBool()); // reported if wrong; still proceed

  bool thenIsNeverType = typchkAlwaysJumps(i->thenBranch);
  bool elseIsNeverType = typchkAlwaysJumps(i->elseBranch);

  Type *thenType = NULL;
  Type *elseType = i->elseBranch != NULL ? NULL : typeUnit();

  if (thenIsNeverType)
    typchkCheckStmt(env, i->thenBranch);
  else
    thenType = typchkInfer(env, i->thenBranch);

  if (elseIsNeverType)
    typchkCheckStmt(env, i->elseBranch);
  else if (i->elseBranch != NULL)
    elseType = typchkInfer(env, i->elseBranch);

  if ((!thenIsNeverType && thenType == NULL) ||
      (!elseIsNeverType && elseType == NULL))
    return NULL;

  if (thenIsNeverType && elseIsNeverType)
    return typeUnit();
  if (thenIsNeverType)
    return elseType;
  if (elseIsNeverType)
    return thenType;

  if (!typchkPairNumberType(env, i->thenBranch, &thenType, elseType) ||
      !typchkPairNumberType(env, i->elseBranch, &elseType, thenType))
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
  if (block->value != NULL && typchkAlwaysJumps(block->value)) {
    typchkCheckStmt(env, block->value);
    result = expectedValueType != NULL ? expectedValueType : typeUnit();
  } else if (block->value != NULL && expectedValueType != NULL &&
             (block->value->kind == NODE_IF ||
              block->value->kind == NODE_BLOCK) &&
             !typchkAlwaysYields(block->value)) {
    // Missing paths are reported by the function's return path check.
    typchkCheckStmt(env, block->value);
    result = expectedValueType;
  } else if (block->value != NULL) {
    if (expectedValueType != NULL) {
      // A partly unknown expected type, such as `Box[?]`, is filled in by
      // the value's own type
      Type *actual = NULL;
      if (!typchkCheckInto(env, block->value, expectedValueType, &actual))
        result = NULL;
      else
        result = actual != NULL && !typeIsFullyKnown(expectedValueType)
                     ? actual
                     : expectedValueType;
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

// Whether a block's final value is produced on every path, or the path
// returns.
static bool typchkAlwaysYields(AstNode *node) {
  switch (node->kind) {
  case NODE_RETURN:
    return true;

  case NODE_BLOCK:
    return typchkCheckIfBlockAlwaysReturns(&node->as.block);

  case NODE_IF: {
    IfNode *if_ = &node->as.if_;
    return if_->elseBranch != NULL && typchkAlwaysYields(if_->thenBranch) &&
           typchkAlwaysYields(if_->elseBranch);
  }

  case NODE_WHILE:
  case NODE_FOR:
  case NODE_BREAK:
  case NODE_CONTINUE:
    return false;

  default:
    return true;
  }
}

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
    return typchkAlwaysYields(block->value);

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

  if (hasExpectedType && varDecl->declaredType->kind == NODE_TYPE) {
    Token declaredTypeIdentifier = varDecl->declaredType->as.type_.name;
    bool isSelf = tokenTextEquals(&declaredTypeIdentifier, "Self");

    if (isSelf && typchkTypeEnvGetImplTargetType(env) == NULL &&
        env->_currentImplSelfType == NULL) {
      typchkErrorAtTokenFmt(&declaredTypeIdentifier,
                            "'Self' can only be used inside an impl block.");
      return;
    }
  }

  if (varDecl->initializer != NULL) {
    if (declaredType != NULL) {
      typchkCheck(env, varDecl->initializer, declaredType);
      typchkTypeEnvDeclare(env, varDecl->name, declaredType);
    } else if (hasExpectedType && varDecl->initializer->kind == NODE_ARRAY &&
               varDecl->initializer->as.array.count == 0) {
      // The declared type was already reported as wrong. An empty literal
      // would only add a second error, about a type that was never given.
      typchkTypeEnvDeclare(env, varDecl->name, NULL);
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

// Checks the body of a function whose signature is `fnType`. Inside the body,
// each of the function's generic parameters is one fixed type that only equals
// itself, so the signature's parameters are swapped for those first. A body
// can then do only what works for every type.
static void typchkCheckGenericFunctionBody(TypeEnv *env, FunctionNode *fn,
                                           Type *fnType, Type *selfType) {
  int count = fn->genericParamCount;
  int implCount = env->_implGenericCount;

  if (count == 0 && implCount == 0) {
    typchkCheckFunctionBody(env, fn, fnType->as.function.paramTypes,
                            fnType->as.function.returnType, selfType);
    return;
  }

  // The impl's parameters come first, then the method's own
  Type **fixed =
      (Type **)typesAllocRaw((size_t)(implCount + count) * sizeof(Type *));
  for (int i = 0; i < implCount; i++)
    fixed[i] = env->_implGenericTypes[i];

  int previousCount =
      typchkEnterGenericParams(env, fn->genericParams, fn->genericBounds, count,
                               implCount, true, fixed + implCount);

  Type **paramTypes =
      fn->arity > 0 ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;
  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = typeSubstituteGenericParams(
        fnType->as.function.paramTypes[i], fixed, implCount + count);
  }
  Type *returnType = typeSubstituteGenericParams(fnType->as.function.returnType,
                                                 fixed, implCount + count);

  typchkCheckFunctionBody(env, fn, paramTypes, returnType, selfType);

  typchkLeaveGenericParams(env, previousCount);
}

static void typchkCheckFunctionDecl(TypeEnv *env, AstNode *node) {
  FunctionNode *fn = &node->as.function;
  bool hasArity = fn->arity > 0;
  int genericCount = fn->genericParamCount;

  Type **paramTypes =
      hasArity ? (Type **)typesAllocRaw(fn->arity * sizeof(Type *)) : NULL;

  Type **placeholders =
      genericCount > 0
          ? (Type **)typesAllocRaw((size_t)genericCount * sizeof(Type *))
          : NULL;
  int previousCount = typchkEnterGenericParams(
      env, fn->genericParams, fn->genericBounds, genericCount,
      env->_implGenericCount, false, placeholders);

  for (int i = 0; i < fn->arity; i++) {
    paramTypes[i] = (fn->paramTypes != NULL && fn->paramTypes[i] != NULL)
                        ? typchkResolveType(env, fn->paramTypes[i])
                        : NULL;
  }

  Type *returnType =
      fn->returnType != NULL ? typchkResolveType(env, fn->returnType) : NULL;

  typchkLeaveGenericParams(env, previousCount);

  Type *fnType = typeFunction(paramTypes, fn->arity, returnType);
  typchkTypeEnvDeclare(env, fn->name, fnType);

  Type *selfType = typchkTypeEnvGetSelfType(env);

  typchkCheckGenericFunctionBody(env, fn, fnType, selfType);
}

void typchkCheckStmt(TypeEnv *env, AstNode *node) {
  switch (node->kind) {
  case NODE_EXPR_STMT:
    typchkInfer(env, node->as.exprStmt.expr);
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

  Type **placeholders =
      fn->genericParamCount > 0
          ? (Type **)typesAllocRaw((size_t)fn->genericParamCount *
                                   sizeof(Type *))
          : NULL;
  int previousCount = typchkEnterGenericParams(
      env, fn->genericParams, fn->genericBounds, fn->genericParamCount,
      env->_implGenericCount, false, placeholders);

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

  typchkLeaveGenericParams(env, previousCount);

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

static void typchkRegisterGenericAlias(TypeEnv *env, Token name, int paramCount,
                                       Type **params, Type *target) {
  if (env->genericAliasCount == env->genericAliasCapacity) {
    env->genericAliasCapacity =
        env->genericAliasCapacity < 8 ? 8 : env->genericAliasCapacity * 2;
    env->genericAliases = (GenericAlias *)realloc(
        env->genericAliases,
        sizeof(GenericAlias) * (size_t)env->genericAliasCapacity);

    if (env->genericAliases == NULL) {
      fprintf(stderr, "realloc failed in typchkRegisterGenericAlias\n");
      exit(1);
    }
  }

  GenericAlias *alias = &env->genericAliases[env->genericAliasCount++];
  alias->name = internTokenName(name);
  alias->paramCount = paramCount;
  alias->params = params;
  alias->target = target;
}

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

  if (typeAnnotation->kind == NODE_TYPE_LIST) {
    typchkResolveAliasDependencies(env, unresolvedAlias, count,
                                   typeAnnotation->as.typeList.elementType);
    return;
  }

  if (typeAnnotation->kind == NODE_TYPE_ARRAY) {
    typchkResolveAliasDependencies(env, unresolvedAlias, count,
                                   typeAnnotation->as.typeArray.elementType);
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

  if (decl->genericParamCount > 0) {
    Type **placeholders = (Type **)typesAllocRaw(
        (size_t)decl->genericParamCount * sizeof(Type *));
    int previousCount = typchkEnterGenericParams(
        env, decl->genericParams, decl->genericBounds, decl->genericParamCount,
        0, false, placeholders);
    Type *target = typchkResolveType(env, decl->target);
    typchkLeaveGenericParams(env, previousCount);

    typchkRegisterGenericAlias(env, decl->name, decl->genericParamCount,
                               placeholders, target);
    return;
  }

  Type *target = typchkResolveType(env, decl->target);

  if (target != NULL)
    typchkTypeEnvRegisterAlias(env, decl->name, target);
}

static void typchkResolveStructFields(TypeEnv *env, AstNode *node) {
  StructNode *struct_ = &node->as.struct_;
  Type *structType = typchkTypeEnvLookupStruct(env, struct_->name);

  if (structType == NULL)
    return;

  // A generic struct's fields are written with its parameters in scope
  int previousCount = env->_genericParamCount;

  if (struct_->genericParamCount > 0) {
    Type **params = structType->as.struct_.genericParams;

    if (params == NULL) {
      params = (Type **)typesAllocRaw((size_t)struct_->genericParamCount *
                                      sizeof(Type *));
      typchkEnterGenericParams(env, struct_->genericParams, /*bounds=*/NULL,
                               struct_->genericParamCount, 0, false, params);
    } else {
      for (int i = 0; i < struct_->genericParamCount; i++) {
        bindingArrayWrite(&env->_genericParams, &env->_genericParamCount,
                          &env->_genericParamCapacity,
                          struct_->genericParams[i], params[i]);
      }
    }
  }

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

  typchkLeaveGenericParams(env, previousCount);

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

// Walks the supertrait chain to identify circular references
static bool typchkTraitSupertraitChainCycles(TypeEnv *env,
                                             InternedName startName) {
  InternedName current = startName;
  int maxSteps = env->traitCount + 1;

  for (int step = 0; step < maxSteps; step++) {
    Type *currentTrait =
        typchkTypeEnvLookupTrait(env, internedNameToToken(current));

    if (currentTrait == NULL || !currentTrait->as.trait_.hasSupertrait)
      return false; // chain ends cleanly, no repeat

    InternedName next = currentTrait->as.trait_.supertraitName;

    if (internedNamesEqual(next, startName))
      return true; // back to where the walk started

    current = next;
  }

  return true; // walked further than there are traits -- must have repeated
}

// Checked once per trait after every trait's own supertrait field has
// been resolved, so a cycle of any length is caught regardless of which
// trait in it happens to be declared first (or checked first).
static void typchkCheckTraitSupertraitCycle(TypeEnv *env, AstNode *node) {
  TraitNode *trait_ = &node->as.trait_;
  if (!trait_->hasSupertrait)
    return;

  Type *traitType = typchkTypeEnvLookupTrait(env, trait_->name);

  // NULL or no supertrait recorded means it already failed to resolve
  // (e.g. "Unknown trait") and was reported there -- nothing to walk.
  if (traitType == NULL || !traitType->as.trait_.hasSupertrait)
    return;

  if (typchkTraitSupertraitChainCycles(env, traitType->as.trait_.name)) {
    typchkErrorAtTokenFmt(&trait_->name,
                          "Trait '%.*s' has a circular supertrait chain.",
                          trait_->name.length, trait_->name.start);
  }
}

// Resolves an impl block's target name to the concrete struct directly, or
// through a type alias. NULL if the name doesn't name either
static Type *typchkResolveImplTarget(TypeEnv *env, Token name) {
  Type *type = typchkTypeEnvLookupStruct(env, name);
  if (type != NULL)
    return type;

  type = typchkTypeEnvLookupAlias(env, name);

  // An alias of `Box[i64]` names the declaration, not the instance
  return (type != NULL && type->kind == TYPE_STRUCT)
             ? typeStructDeclaration(type)
             : NULL;
}

// True if a trait implemented for the struct already provides a method with
// this name, instance or static
static bool typchkStructTraitProvides(Type *structType, Token name) {
  return typeStructTraitInstanceMethodLookup(structType, name) != NULL ||
         typeStructTraitStaticMethodLookup(structType, name) != NULL;
}

// True if the struct has a method of this name, from any impl, in the
// category opposite to `hasSelf`. An instance method and a static method
// share one namespace at run time, so a name can't be both
static bool typchkStructHasOtherCategory(Type *structType, Token name,
                                         bool hasSelf) {
  if (hasSelf) {
    return typeStructStaticMethodLookup(structType, name) != NULL ||
           typeStructTraitStaticMethodLookup(structType, name) != NULL;
  }

  return typeStructInstanceMethodLookup(structType, name) != NULL ||
         typeStructTraitInstanceMethodLookup(structType, name) != NULL;
}

static void typchkErrorInstanceAndStatic(Token *name, Token *typeName) {
  typchkErrorAtTokenFmt(name,
                        "'%.*s' can't be both an instance method and a static "
                        "method on '%.*s'.",
                        name->length, name->start, typeName->length,
                        typeName->start);
}

// Checks one method of `impl Trait for X` against the trait. Returns the
// method's type with `Self` replaced by the target, or NULL after reporting
// why the method doesn't fit.
static Type *typchkCheckTraitImplMethod(TypeEnv *env, ImplNode *impl,
                                        Type *traitType, FunctionNode *method,
                                        Type *targetType) {
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
              : "'%.*s' is an instance method on trait '%.*s' -- add 'self'.",
          method->name.length, method->name.start, impl->traitName.length,
          impl->traitName.start);
    } else {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' isn't a method defined by trait '%.*s'.",
                            method->name.length, method->name.start,
                            impl->traitName.length, impl->traitName.start);
    }
    return NULL;
  }

  Type *methodType = typchkResolveFunctionSignature(env, method);
  if (methodType == NULL)
    return NULL;

  Type *concreteMethodType = typeSubstituteSelf(methodType, targetType);
  Type *concreteRequiredType = typeSubstituteSelf(requiredType, targetType);

  if (!typesEqual(concreteMethodType, concreteRequiredType)) {
    typchkErrorAtTokenFmt(
        &method->name,
        "'%.*s' doesn't match trait '%.*s': expected %s, got %s.",
        method->name.length, method->name.start, impl->traitName.length,
        impl->traitName.start, typeToString(concreteRequiredType),
        typeToString(concreteMethodType));
    return NULL;
  }

  return concreteMethodType;
}

// Reports each method the trait requires that the impl block leaves out.
// Returns false if any is missing.
static bool typchkTraitImplHasAllMethods(ImplNode *impl, Type *traitType) {
  bool ok = true;

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

  return ok;
}

static void typchkRegisterListOrArrayImpl(TypeEnv *env, AstNode *node,
                                          Type *traitType);

// `impl Trait for string { ... }`. Only the standard library can write one.
static void typchkRegisterPrimitiveTraitImpl(TypeEnv *env, AstNode *node,
                                             Type *traitType, Type *primitive) {
  ImplNode *impl = &node->as.impl;
  Token target = impl->targetName;

  if (!env->_isStdlib) {
    typchkErrorAtTokenFmt(
        &target, "Only the standard library can implement traits for '%.*s'.",
        target.length, target.start);
    return;
  }

  const char *owner = typchkPrimitiveOwner(primitive);
  assert(owner != NULL);

  InternedName traitName = internTokenName(impl->traitName);

  if (typchkPrimitiveImplements(env, owner, traitName)) {
    typchkErrorAtTokenFmt(&impl->traitName, "'%.*s' already implements '%.*s'.",
                          target.length, target.start, impl->traitName.length,
                          impl->traitName.start);
    return;
  }

  impl->primitiveOwner = owner;

  bool ok = true;
  int methodCountBefore = env->primitiveMethodCount;

  env->_currentPrimitiveOwner = owner;
  env->_currentImplSelfType = primitive;

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];

    Type *concreteMethodType =
        typchkCheckTraitImplMethod(env, impl, traitType, method, primitive);

    if (concreteMethodType == NULL) {
      ok = false;
      continue;
    }

    if (typchkPrimitiveTraitMethodLookup(env, owner, method->name, NULL) !=
        NULL) {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' is already provided to '%.*s' by another "
                            "trait.",
                            method->name.length, method->name.start,
                            target.length, target.start);
      ok = false;
      continue;
    }

    // Inherent methods on a primitive are all instance methods
    if (!method->hasSelf &&
        typchkPrimitiveMethodLookup(env, owner, method->name) != NULL) {
      typchkErrorInstanceAndStatic(&method->name, &target);
      ok = false;
      continue;
    }

    typchkPrimitiveTraitMethodAdd(env, owner, method->name, concreteMethodType,
                                  method->hasSelf, traitName);
  }

  env->_currentImplSelfType = NULL;
  env->_currentPrimitiveOwner = NULL;

  if (!typchkTraitImplHasAllMethods(impl, traitType))
    ok = false;

  // A block that fails registers nothing
  if (ok)
    typchkPrimitiveImplementsAdd(env, owner, traitName);
  else
    env->primitiveMethodCount = methodCountBefore;
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

  if (impl->targetKind != IMPL_TARGET_NAME) {
    typchkRegisterListOrArrayImpl(env, node, traitType);
    return;
  }

  Type *targetType = typchkResolveImplTarget(env, impl->targetName);

  if (targetType == NULL) {
    Type *primitive = typchkPrimitiveTypeNamed(&impl->targetName);

    if (primitive != NULL) {
      typchkRegisterPrimitiveTraitImpl(env, node, traitType, primitive);
    } else {
      typchkErrorAtTokenFmt(&impl->targetName, "Unknown type '%.*s'.",
                            impl->targetName.length, impl->targetName.start);
    }

    return;
  }

  resolvedImplTargetsRecord(node, targetType->as.struct_.name);

  if (typeStructIsGeneric(targetType)) {
    typchkErrorAtTokenFmt(
        &impl->targetName,
        "Trait implementations on generic structs aren't supported yet.");
    return;
  }

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

    Type *concreteMethodType =
        typchkCheckTraitImplMethod(env, impl, traitType, method, targetType);

    if (concreteMethodType == NULL) {
      ok = false;
      continue;
    }

    if (typchkStructTraitProvides(targetType, method->name)) {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' is already provided to '%.*s' by another "
                            "trait.",
                            method->name.length, method->name.start,
                            impl->targetName.length, impl->targetName.start);
      ok = false;
      continue;
    }

    if (typchkStructHasOtherCategory(targetType, method->name,
                                     method->hasSelf)) {
      typchkErrorInstanceAndStatic(&method->name, &impl->targetName);
      ok = false;
      continue;
    }

    typeStructAddTraitMethod(targetType, method->name, concreteMethodType,
                             method->hasSelf);
  }

  typchkTypeEnvSetImplTargetType(env, NULL);

  if (!typchkTraitImplHasAllMethods(impl, traitType))
    ok = false;

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

  Type *targetType = typchkResolveImplTarget(env, impl->targetName);
  InternedName traitName = internTokenName(impl->traitName);
  InternedName supertraitName = traitType->as.trait_.supertraitName;
  bool implementsSupertrait;

  if (targetType == NULL) {
    Type *primitive = typchkPrimitiveTypeNamed(&impl->targetName);
    const char *owner =
        primitive != NULL ? typchkPrimitiveOwner(primitive) : NULL;

    if (impl->targetKind == IMPL_TARGET_LIST)
      owner = "list";
    else if (impl->targetKind == IMPL_TARGET_ARRAY)
      owner = "array";

    // Anything else was already reported
    if (owner == NULL || !typchkPrimitiveImplements(env, owner, traitName))
      return;

    implementsSupertrait =
        typchkPrimitiveImplements(env, owner, supertraitName);
  } else {
    if (!typeStructImplementsTrait(targetType, traitName))
      return;

    implementsSupertrait =
        typeStructImplementsTrait(targetType, supertraitName);
  }

  if (!implementsSupertrait) {
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

// Registers the methods of an impl block on a primitive type, such as
// `impl string { ... }`. Only the standard library can add methods to one.
static void typchkRegisterPrimitiveImplMethods(TypeEnv *env, AstNode *node,
                                               Type *primitive) {
  ImplNode *impl = &node->as.impl;
  Token target = impl->targetName;

  if (!env->_isStdlib) {
    typchkErrorAtTokenFmt(
        &target, "Only the standard library can add methods to '%.*s'.",
        target.length, target.start);
    return;
  }

  const char *owner = typchkPrimitiveOwner(primitive);
  assert(owner != NULL);

  impl->primitiveOwner = owner;

  env->_currentPrimitiveOwner = owner;
  env->_currentImplSelfType = primitive;

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];

    if (!method->hasSelf) {
      typchkErrorAtTokenFmt(
          &method->name, "Static methods on '%s' aren't supported yet.", owner);
      continue;
    }

    Type *methodType = typchkResolveFunctionSignature(env, method);

    if (methodType == NULL)
      continue; // already reported

    if (typchkPrimitiveMethodLookup(env, owner, method->name) != NULL) {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' is already declared on '%s'.",
                            method->name.length, method->name.start, owner);
      continue;
    }

    // A trait's instance method of this name is shadowed, which is fine. A
    // static one would share the name at run time
    PrimitiveMethod *traitMethod =
        typchkPrimitiveTraitMethodLookup(env, owner, method->name, NULL);

    if (traitMethod != NULL && !traitMethod->hasSelf) {
      typchkErrorInstanceAndStatic(&method->name, &target);
      continue;
    }

    typchkPrimitiveMethodAdd(env, owner, method->name, methodType,
                             method->isPublic);
  }

  env->_currentImplSelfType = NULL;
  env->_currentPrimitiveOwner = NULL;
}

// Puts the type parameters of `impl [T]` or `impl [T; N]` in scope and returns
// the receiver type, or NULL after an error. `T` is the item type. `N` is the
// length, and `_` is a length that is not used. The length placeholders have
// no spelling of their own, so `_` gets one that no signature can write.
static Type *typchkEnterListOrArrayImpl(TypeEnv *env, ImplNode *impl,
                                        bool isOpaque, int *previousCount) {
  *previousCount = env->_genericParamCount;
  Token *item = &impl->genericParams[0];
  int length = 0;

  if (!isOpaque && typchkPrimitiveTypeNamed(item) != NULL) {
    typchkErrorAtTokenFmt(item,
                          "'%.*s' is a type. An impl names the item type "
                          "with a type parameter, as in 'impl [T]'.",
                          item->length, item->start);
    return NULL;
  }

  if (impl->targetKind == IMPL_TARGET_ARRAY) {
    Token *lengthName = &impl->genericParams[1];

    if (tokenTextEquals(lengthName, "N")) {
      length = ARRAY_LENGTH_PLACEHOLDER(0);
    } else if (tokenTextEquals(lengthName, "_")) {
      length = ARRAY_LENGTH_PLACEHOLDER(2);
    } else {
      typchkErrorAtTokenFmt(lengthName,
                            "An array impl names its length 'N', or '_' to "
                            "leave it unnamed, not '%.*s'.",
                            lengthName->length, lengthName->start);
      return NULL;
    }
  }

  Type **itemTypes = (Type **)typesAllocRaw(sizeof(Type *));
  typchkEnterGenericParams(env, impl->genericParams, /*bounds=*/NULL, 1, 0,
                           isOpaque, itemTypes);

  Type *receiver = impl->targetKind == IMPL_TARGET_LIST
                       ? typeList(itemTypes[0])
                       : typeArray(itemTypes[0], length);

  env->_implGenericTypes = itemTypes;
  env->_implGenericCount = 1;
  env->_currentImplReceiver = receiver;
  return receiver;
}

static void typchkLeaveListOrArrayImpl(TypeEnv *env, int previousCount) {
  env->_implGenericTypes = NULL;
  env->_implGenericCount = 0;
  env->_currentImplReceiver = NULL;
  typchkLeaveGenericParams(env, previousCount);
}

// Registers `impl [T] { ... }`, `impl [T; N] { ... }`, or the same with a
// trait when `traitType` isn't NULL.
static void typchkRegisterListOrArrayImpl(TypeEnv *env, AstNode *node,
                                          Type *traitType) {
  ImplNode *impl = &node->as.impl;
  int previousCount;
  Type *receiver = typchkEnterListOrArrayImpl(env, impl, false, &previousCount);

  if (receiver == NULL)
    return;

  if (traitType != NULL)
    typchkRegisterPrimitiveTraitImpl(env, node, traitType, receiver);
  else
    typchkRegisterPrimitiveImplMethods(env, node, receiver);

  typchkLeaveListOrArrayImpl(env, previousCount);
}

static void typchkRegisterImplMethods(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  if (impl->hasTraitName) {
    typchkRegisterTraitImpl(env, node);
    return;
  }

  if (impl->targetKind != IMPL_TARGET_NAME) {
    typchkRegisterListOrArrayImpl(env, node, NULL);
    return;
  }

  Type *structType = typchkResolveImplTarget(env, impl->targetName);

  if (structType == NULL) {
    Type *primitive = typchkPrimitiveTypeNamed(&impl->targetName);

    if (primitive != NULL) {
      typchkRegisterPrimitiveImplMethods(env, node, primitive);
    } else {
      typchkErrorAtTokenFmt(&impl->targetName, "Unknown type '%.*s'.",
                            impl->targetName.length, impl->targetName.start);
    }
    return;
  }

  resolvedImplTargetsRecord(node, structType->as.struct_.name);

  int declaredCount = structType->as.struct_.genericParamCount;

  if (impl->genericParamCount != declaredCount) {
    typchkErrorAtTokenFmt(&impl->targetName,
                          "'%.*s' takes %d type parameter(s), but this impl "
                          "declares %d.",
                          impl->targetName.length, impl->targetName.start,
                          declaredCount, impl->genericParamCount);
    typeStructMarkUnresolvedMembers(structType);
    return;
  }

  // The impl's own parameters, such as the `T` in `impl Box[T]`, stand for
  // the struct's parameters in order
  Type **implParams =
      declaredCount > 0
          ? (Type **)typesAllocRaw((size_t)declaredCount * sizeof(Type *))
          : NULL;
  int previousGenericCount =
      typchkEnterGenericParams(env, impl->genericParams, /*bounds=*/NULL,
                               declaredCount, 0, false, implParams);
  typchkInheritStructBounds(structType, implParams, declaredCount);

  typchkTypeEnvSetImplTargetType(env, structType);
  env->_implGenericTypes = implParams;
  env->_implGenericCount = declaredCount;
  env->_currentImplSelfType =
      declaredCount > 0
          ? typeStructInstance(structType, implParams, declaredCount)
          : NULL;

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    Type *methodType = typchkResolveFunctionSignature(env, method);

    if (methodType == NULL) {
      typeStructMarkUnresolvedMembers(structType); // error already reported
      continue;
    }

    Type *existing =
        method->hasSelf
            ? typeStructInstanceMethodLookup(structType, method->name)
            : typeStructStaticMethodLookup(structType, method->name);
    if (existing != NULL) {
      typchkErrorAtTokenFmt(&method->name,
                            "'%.*s' is already declared on '%.*s'.",
                            method->name.length, method->name.start,
                            impl->targetName.length, impl->targetName.start);
      continue;
    }

    if (typchkStructHasOtherCategory(structType, method->name,
                                     method->hasSelf)) {
      typchkErrorInstanceAndStatic(&method->name, &impl->targetName);
      continue;
    }

    if (method->hasSelf) {
      typeStructAddInstanceMethod(structType, method->name, methodType,
                                  method->isPublic);
    } else {
      typeStructAddStaticMethod(structType, method->name, methodType,
                                method->isPublic);
    }
  }

  typchkTypeEnvSetImplTargetType(env, NULL);
  env->_implGenericTypes = NULL;
  env->_implGenericCount = 0;
  env->_currentImplSelfType = NULL;
  typchkLeaveGenericParams(env, previousGenericCount);
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
  typchkCheckGenericFunctionBody(env, fn, fnType, NULL);
}

// Checks the bodies of a primitive type's methods. An impl that failed to
// register was already reported and has no owner.
static void checkPrimitiveImplMethodBodies(TypeEnv *env, AstNode *node,
                                           Type *primitive) {
  ImplNode *impl = &node->as.impl;
  const char *owner = impl->primitiveOwner;

  if (owner == NULL)
    return;

  env->_currentPrimitiveOwner = owner;
  env->_currentImplSelfType = primitive;

  for (int i = 0; i < impl->methodCount; i++) {
    FunctionNode *method = impl->methods[i];
    InternedName trait = {0};
    PrimitiveMethod *registered;

    if (impl->hasTraitName) {
      trait = internTokenName(impl->traitName);
      registered =
          typchkPrimitiveTraitMethodLookup(env, owner, method->name, &trait);
    } else {
      registered = typchkPrimitiveMethodLookup(env, owner, method->name);
    }

    // A method that failed to register was already reported
    if (registered == NULL || (!impl->hasTraitName && !method->hasSelf))
      continue;

    typchkCheckGenericFunctionBody(env, method, registered->type,
                                   method->hasSelf ? primitive : NULL);
  }

  env->_currentImplSelfType = NULL;
  env->_currentPrimitiveOwner = NULL;
}

static void checkImplMethodBodies(TypeEnv *env, AstNode *node) {
  ImplNode *impl = &node->as.impl;

  if (impl->targetKind != IMPL_TARGET_NAME) {
    if (impl->primitiveOwner == NULL)
      return; // failed to register; already reported

    int previousCount;
    Type *receiver = typchkEnterListOrArrayImpl(env, impl, /*isOpaque=*/true,
                                                &previousCount);
    checkPrimitiveImplMethodBodies(env, node, receiver);
    typchkLeaveListOrArrayImpl(env, previousCount);
    return;
  }

  Type *structType = typchkResolveImplTarget(env, impl->targetName);

  if (structType == NULL &&
      typchkPrimitiveTypeNamed(&impl->targetName) != NULL) {
    checkPrimitiveImplMethodBodies(env, node,
                                   typchkPrimitiveTypeNamed(&impl->targetName));
    return;
  }

  // Inside a generic impl, each parameter is one fixed type
  int declaredCount = structType != NULL && !impl->hasTraitName
                          ? structType->as.struct_.genericParamCount
                          : 0;
  bool isGenericImpl =
      declaredCount > 0 && impl->genericParamCount == declaredCount;
  Type **fixedParams = NULL;
  int previousGenericCount = env->_genericParamCount;

  if (isGenericImpl) {
    fixedParams =
        (Type **)typesAllocRaw((size_t)declaredCount * sizeof(Type *));
    typchkEnterGenericParams(env, impl->genericParams, /*bounds=*/NULL,
                             declaredCount, 0, true, fixedParams);
    typchkInheritStructBounds(structType, fixedParams, declaredCount);
    env->_implGenericTypes = fixedParams;
    env->_implGenericCount = declaredCount;
    env->_currentImplSelfType =
        typeStructInstance(structType, fixedParams, declaredCount);
  }

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

    Type *selfType = !method->hasSelf ? NULL
                     : isGenericImpl  ? env->_currentImplSelfType
                                      : structType;
    typchkCheckGenericFunctionBody(env, method, methodType, selfType);
  }

  typchkTypeEnvSetImplTargetType(env, NULL);
  env->_implGenericTypes = NULL;
  env->_implGenericCount = 0;
  env->_currentImplSelfType = NULL;
  typchkLeaveGenericParams(env, previousGenericCount);
}

// Checks one unit. The traits the standard library declares are builtin, so a
// later unit can't declare one with the same name.
static bool typchkCheckUnit(AstNode **program, int count, bool isStdlib) {
  // Diagnostics are per-unit/program
  typchkResetError();

  bool ownsEnv = sessionEnv == NULL;
  TypeEnv *env = ownsEnv ? typchkTypeEnvCreate() : sessionEnv;
  env->_isStdlib = isStdlib;

  if (ownsEnv)
    typchkTypeEnvBeginScope(env);

  // Structs

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_STRUCT) {
      StructNode *sn = &program[i]->as.struct_;
      Type *placeholder = typeStruct(sn->name, NULL, 0);

      typeStructSetGenericParamCount(placeholder, sn->genericParamCount);

      typchkTypeEnvRegisterStruct(env, sn->name, placeholder);
    }
  }

  bool *isDuplicateTrait =
      count > 0 ? (bool *)calloc((size_t)count, sizeof(bool)) : NULL;

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      TraitNode *trait_ = &program[i]->as.trait_;

      Type *existing = typchkTypeEnvLookupTrait(env, trait_->name);

      if (existing != NULL) {
        if (typeTraitIsBuiltin(existing)) {
          typchkErrorAtTokenFmt(
              &trait_->name,
              "'%.*s' is a builtin trait and can't be redeclared.",
              trait_->name.length, trait_->name.start);
        } else {
          typchkErrorAtTokenFmt(&trait_->name,
                                "'%.*s' is already declared as a trait.",
                                trait_->name.length, trait_->name.start);
        }

        if (isDuplicateTrait != NULL) {
          isDuplicateTrait[i] = true;
        }

        continue;
      }

      Type *placeholder = typeTrait(trait_->name, NULL, 0, NULL, 0);

      if (isStdlib)
        typeMarkTraitAsBuiltin(placeholder);

      typchkTypeEnvRegisterTrait(env, trait_->name, placeholder);
    }
  }

  // Struct type parameter bounds
  //
  // After trait placeholders so a bound can name a trait, before aliases and
  // struct fields so a use of `Box[X]` anywhere can be checked against them.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind != NODE_STRUCT)
      continue;

    StructNode *struct_ = &program[i]->as.struct_;
    Type *structType = typchkTypeEnvLookupStruct(env, struct_->name);

    if (structType == NULL || struct_->genericParamCount == 0)
      continue;

    Type **params = (Type **)typesAllocRaw((size_t)struct_->genericParamCount *
                                           sizeof(Type *));
    int previousCount = typchkEnterGenericParams(
        env, struct_->genericParams, struct_->genericBounds,
        struct_->genericParamCount, 0, false, params);
    typchkLeaveGenericParams(env, previousCount);
    typeStructSetGenericParams(structType, params);
  }

  // Type aliases
  //
  // After struct placeholders so an alias can name a struct, before struct
  // fields so a field can be annotated with an alias.

  // TODO: Refactor this

  UnresolvedTypeAlias *unresolvedAliasAliases = NULL;
  int pendingAliasCount = 0;

  for (int i = 0; i < count; i++) {
    // Generic aliases aren't checked yet. Remember their names so that using
    // one reports "not supported" instead of "Unknown type."
    if (program[i]->kind == NODE_TYPE_ALIAS) {
      pendingAliasCount++;
    }
  }

  if (pendingAliasCount > 0) {
    unresolvedAliasAliases = (UnresolvedTypeAlias *)calloc(
        (size_t)pendingAliasCount, sizeof(UnresolvedTypeAlias));

    int next = 0;
    for (int i = 0; i < count; i++) {
      if (program[i]->kind == NODE_TYPE_ALIAS)
        unresolvedAliasAliases[next++].node = program[i];
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
    if (program[i]->kind == NODE_TRAIT &&
        !(isDuplicateTrait != NULL && isDuplicateTrait[i])) {
      typchkResolveTraitMethods(env, program[i]);
    }
  }

  free(isDuplicateTrait);

  // Supertrait cycle detection
  //
  // After every trait's own supertrait field is resolved, so a chain
  // (however many traits long) can be walked in either direction
  // regardless of declaration order.

  for (int i = 0; i < count; i++) {
    if (program[i]->kind == NODE_TRAIT) {
      typchkCheckTraitSupertraitCycle(env, program[i]);
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

bool typchkCheckProgram(AstNode **program, int count) {
  return typchkCheckUnit(program, count, /*isStdlib=*/false);
}

bool typchkCheckStdlib(AstNode **program, int count) {
  return typchkCheckUnit(program, count, /*isStdlib=*/true);
}
