#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "strbuf.h"
#include "stringset.h"
#include "types.h"

#define SLAB_SIZE 8192

typedef struct Slab {
  struct Slab *next;
  size_t capacity;
  uint8_t data[];
} Slab;

static Slab *arenaHead = NULL;
static size_t arenaOffset = 0;

static StringSet typeNames;

static Slab *allocSlab(size_t capacity) {
  Slab *slab = (Slab *)malloc(sizeof(Slab) + capacity);
  if (slab == NULL) {
    fprintf(stderr, "Out of memory allocating type arena slab.\n");
    exit(EXIT_CODE_OS_ERR);
  }
  slab->next = arenaHead;
  slab->capacity = capacity;
  arenaHead = slab;
  arenaOffset = 0;

  return slab;
}

void *typesAllocRaw(size_t size) {
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

static Type *unitSingleton = NULL;
static Type *boolSingleton = NULL;
static Type *stringSingleton = NULL;
static Type *f64Singleton = NULL;
static Type *selfPlaceholderSingleton = NULL;

void typesFreeAll(void) {
  Slab *s = arenaHead;
  while (s != NULL) {
    Slab *next = s->next;
    free(s);
    s = next;
  }
  arenaHead = NULL;
  arenaOffset = 0;

  unitSingleton = NULL;
  boolSingleton = NULL;
  stringSingleton = NULL;
  f64Singleton = NULL;
  selfPlaceholderSingleton = NULL;

  stringSetFree(&typeNames);
}

InternedName internTokenName(Token token) {
  if (token.start == NULL || token.length <= 0)
    return (InternedName){.offset = 0, .length = 0};

  int offset = stringSetIntern(&typeNames, token.start, token.length);
  return (InternedName){.offset = offset, .length = token.length};
}

const char *internedNameChars(InternedName name) {
  return typeNames.arena + name.offset;
}

bool internedNamesEqual(InternedName a, InternedName b) {
  return a.offset == b.offset;
}

bool internedNameEqualsToken(InternedName name, Token token) {
  if (name.length != token.length)
    return false;
  return memcmp(internedNameChars(name), token.start, (size_t)token.length) ==
         0;
}

static Type *allocType(TypeKind kind) {
  Type *type = (Type *)typesAllocRaw(sizeof(Type));
  memset(type, 0, sizeof(Type));
  type->kind = kind;
  return type;
}

Type *typeUnit(void) {
  if (unitSingleton == NULL)
    unitSingleton = allocType(TYPE_UNIT);
  return unitSingleton;
}

Type *typeBool(void) {
  if (boolSingleton == NULL)
    boolSingleton = allocType(TYPE_BOOL);
  return boolSingleton;
}

Type *typeString(void) {
  if (stringSingleton == NULL)
    stringSingleton = allocType(TYPE_STRING);
  return stringSingleton;
}

Type *typeF64(void) {
  if (f64Singleton == NULL)
    f64Singleton = allocType(TYPE_F64);
  return f64Singleton;
}

Type *typeSelfPlaceholder(void) {
  if (selfPlaceholderSingleton == NULL)
    selfPlaceholderSingleton = allocType(TYPE_SELF);
  return selfPlaceholderSingleton;
}

Type *typeGenericParam(Token name) {
  Type *type = allocType(TYPE_GENERIC_PARAM);

  type->as.genericParam.name = internTokenName(name);

  return type;
}

// Copies `pending` into a durable TypeMember array, interning each name.
static TypeMember *internMembers(UninternedTypeMember *pending, int count) {
  if (count == 0)
    return NULL;

  TypeMember *members =
      (TypeMember *)typesAllocRaw((size_t)count * sizeof(TypeMember));

  for (int i = 0; i < count; i++) {
    members[i].name = internTokenName(pending[i].name);
    members[i].type = pending[i].type;
  }

  return members;
}

static void appendMember(TypeMember **array, int *count, Token name,
                         Type *memberType) {
  int newCount = *count + 1;
  TypeMember *newArray =
      (TypeMember *)typesAllocRaw(newCount * sizeof(TypeMember));
  if (*count > 0)
    memcpy(newArray, *array, (size_t)(*count) * sizeof(TypeMember));
  newArray[newCount - 1].name = internTokenName(name);
  newArray[newCount - 1].type = memberType;
  *array = newArray;
  *count = newCount;
}

static void appendMemberWithVisibility(TypeMember **array, bool **isPublicArray,
                                       int *count, Token name, Type *memberType,
                                       bool isPublic) {
  int newCount = *count + 1;

  TypeMember *newMembers =
      (TypeMember *)typesAllocRaw((size_t)newCount * sizeof(TypeMember));
  if (*count > 0)
    memcpy(newMembers, *array, (size_t)(*count) * sizeof(TypeMember));
  newMembers[newCount - 1].name = internTokenName(name);
  newMembers[newCount - 1].type = memberType;

  bool *newIsPublic = (bool *)typesAllocRaw((size_t)newCount * sizeof(bool));
  if (*count > 0)
    memcpy(newIsPublic, *isPublicArray, (size_t)(*count) * sizeof(bool));
  newIsPublic[newCount - 1] = isPublic;

  *array = newMembers;
  *isPublicArray = newIsPublic;
  *count = newCount;
}

Type *typeStruct(Token name, UninternedTypeMember *fields, int fieldCount,
                 UninternedTypeMember *staticMethods, int staticMethodCount,
                 UninternedTypeMember *instanceMethods,
                 int instanceMethodCount) {
  Type *type = allocType(TYPE_STRUCT);
  type->as.struct_.name = internTokenName(name);
  type->as.struct_.fields = internMembers(fields, fieldCount);
  type->as.struct_.fieldCount = fieldCount;

  for (int i = 0; i < staticMethodCount; i++) {
    appendMemberWithVisibility(
        &type->as.struct_.staticMethods, &type->as.struct_.staticMethodIsPublic,
        &type->as.struct_.staticMethodCount, staticMethods[i].name,
        staticMethods[i].type,
        /*isPublic=*/&type->as.struct_.staticMethodIsPublic[i]);
  }

  for (int i = 0; i < instanceMethodCount; i++) {
    appendMemberWithVisibility(
        &type->as.struct_.instanceMethods,
        &type->as.struct_.instanceMethodIsPublic,
        &type->as.struct_.instanceMethodCount, instanceMethods[i].name,
        instanceMethods[i].type,
        /*isPublic=*/&type->as.struct_.instanceMethodIsPublic[i]);
  }

  return type;
}

Type *typeFunction(Type **paramTypes, int paramCount, Type *returnType) {
  Type *type = allocType(TYPE_FN);
  type->as.function.paramTypes = paramTypes;
  type->as.function.paramCount = paramCount;
  type->as.function.returnType = returnType;
  return type;
}

Type *typeArray(Type *elementType) {
  Type *type = allocType(TYPE_ARRAY);
  type->as.array.elementType = elementType;
  return type;
}

Type *typeTrait(Token name, UninternedTypeMember *staticMethods,
                int staticMethodCount, UninternedTypeMember *instanceMethods,
                int instanceMethodCount) {
  Type *type = allocType(TYPE_TRAIT);
  type->as.trait_.name = internTokenName(name);
  type->as.trait_.staticMethods =
      internMembers(staticMethods, staticMethodCount);
  type->as.trait_.staticMethodCount = staticMethodCount;
  type->as.trait_.instanceMethods =
      internMembers(instanceMethods, instanceMethodCount);
  type->as.trait_.instanceMethodCount = instanceMethodCount;
  return type;
}

void typeStructSetFields(Type *type, UninternedTypeMember *fields,
                         int fieldCount) {
  type->as.struct_.fields = internMembers(fields, fieldCount);
  type->as.struct_.fieldCount = fieldCount;
}

void typeStructAddStaticMethod(Type *type, Token name, Type *methodType,
                               bool isPublic) {
  appendMemberWithVisibility(
      &type->as.struct_.staticMethods, &type->as.struct_.staticMethodIsPublic,
      &type->as.struct_.staticMethodCount, name, methodType, isPublic);
}

void typeStructAddInstanceMethod(Type *type, Token name, Type *methodType,
                                 bool isPublic) {
  appendMemberWithVisibility(&type->as.struct_.instanceMethods,
                             &type->as.struct_.instanceMethodIsPublic,
                             &type->as.struct_.instanceMethodCount, name,
                             methodType, isPublic);
}

void typeStructAddTraitMethod(Type *type, Token name, Type *methodType,
                              bool hasSelf) {
  if (hasSelf) {
    appendMember(&type->as.struct_.traitInstanceMethods,
                 &type->as.struct_.traitInstanceMethodCount, name, methodType);
  } else {
    appendMember(&type->as.struct_.traitStaticMethods,
                 &type->as.struct_.traitStaticMethodCount, name, methodType);
  }
}

void typeStructMarkTraitImplemented(Type *type, InternedName traitName) {
  int newCount = type->as.struct_.implementedTraitCount + 1;
  InternedName *newArray =
      (InternedName *)typesAllocRaw(newCount * sizeof(InternedName));

  if (type->as.struct_.implementedTraitCount > 0) {
    memcpy(newArray, type->as.struct_.implementedTraits,
           (size_t)type->as.struct_.implementedTraitCount *
               sizeof(InternedName));
  }

  newArray[newCount - 1] = traitName;
  type->as.struct_.implementedTraits = newArray;
  type->as.struct_.implementedTraitCount = newCount;
}

bool typeStructImplementsTrait(Type *type, InternedName traitName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return false;
  for (int i = 0; i < type->as.struct_.implementedTraitCount; i++) {
    if (internedNamesEqual(type->as.struct_.implementedTraits[i], traitName))
      return true;
  }
  return false;
}

void typeTraitSetMethods(Type *type, UninternedTypeMember *staticMethods,
                         int staticMethodCount,
                         UninternedTypeMember *instanceMethods,
                         int instanceMethodCount) {
  type->as.trait_.staticMethods =
      internMembers(staticMethods, staticMethodCount);
  type->as.trait_.staticMethodCount = staticMethodCount;
  type->as.trait_.instanceMethods =
      internMembers(instanceMethods, instanceMethodCount);
  type->as.trait_.instanceMethodCount = instanceMethodCount;
}

void typeTraitSetSupertrait(Type *type, InternedName supertraitName) {
  type->as.trait_.hasSupertrait = true;
  type->as.trait_.supertraitName = supertraitName;
}

void typeTraitMarkUnresolvedMembers(Type *type) {
  type->as.trait_.hasUnresolvedMembers = true;
}

bool typeTraitHasUnresolvedMembers(Type *type) {
  return type != NULL && type->kind == TYPE_TRAIT &&
         type->as.trait_.hasUnresolvedMembers;
}

void typeTraitMarkBuiltin(Type *type) { type->as.trait_.isBuiltin = true; }

bool typeTraitIsBuiltin(Type *type) {
  return type != NULL && type->kind == TYPE_TRAIT && type->as.trait_.isBuiltin;
}

Type *typeSubstituteSelf(Type *type, Type *concrete) {
  if (type == NULL)
    return NULL;

  switch (type->kind) {
  case TYPE_SELF:
    return concrete;

  case TYPE_FN: {
    Type **paramTypes = type->as.function.paramCount > 0
                            ? (Type **)typesAllocRaw(
                                  type->as.function.paramCount * sizeof(Type *))
                            : NULL;
    bool changed = false;
    for (int i = 0; i < type->as.function.paramCount; i++) {
      paramTypes[i] =
          typeSubstituteSelf(type->as.function.paramTypes[i], concrete);
      if (paramTypes[i] != type->as.function.paramTypes[i])
        changed = true;
    }
    Type *returnType =
        typeSubstituteSelf(type->as.function.returnType, concrete);
    if (!changed && returnType == type->as.function.returnType)
      return type;
    return typeFunction(paramTypes, type->as.function.paramCount, returnType);
  }

  case TYPE_ARRAY: {
    Type *elementType =
        typeSubstituteSelf(type->as.array.elementType, concrete);
    if (elementType == type->as.array.elementType)
      return type;
    return typeArray(elementType);
  }

  default:
    // Primitives, structs, and traits don't themselves contain Self --
    // only a signature built from them (a TYPE_FN) can.
    return type;
  }
}

void typeStructMarkGeneric(Type *type) { type->as.struct_.isGeneric = true; }

bool typeStructIsGeneric(Type *type) {
  return type != NULL && type->kind == TYPE_STRUCT &&
         type->as.struct_.isGeneric;
}

void typeStructSetGenericParams(Type *type, Type **params, int count) {
  type->as.struct_.genericTypeParams = params;
  type->as.struct_.genericTypeParamCount = count;
}

int typeStructGenericParamCount(Type *type) {
  return type->as.struct_.genericTypeParamCount;
}

Type *typeStructGenericParamAt(Type *type, int index) {
  return type->as.struct_.genericTypeParams[index];
}

Type *typeSubstituteGenericParams(Type *type, Type **params, Type **args,
                                  int count) {
  if (type == NULL)
    return NULL;

  switch (type->kind) {
  case TYPE_GENERIC_PARAM:
    for (int i = 0; i < count; i++) {
      if (type == params[i])
        return args[i];
    }

    return type;

  case TYPE_FN: {
    Type **paramTypes = type->as.function.paramCount > 0
                            ? (Type **)typesAllocRaw(
                                  type->as.function.paramCount * sizeof(Type *))
                            : NULL;

    bool changed = false;
    for (int i = 0; i < type->as.function.paramCount; i++) {
      paramTypes[i] = typeSubstituteGenericParams(
          type->as.function.paramTypes[i], params, args, count);

      if (paramTypes[i] != type->as.function.paramTypes[i])
        changed = true;
    }

    Type *returnType = typeSubstituteGenericParams(type->as.function.returnType,
                                                   params, args, count);
    if (!changed && returnType == type->as.function.returnType)
      return type;

    return typeFunction(paramTypes, type->as.function.paramCount, returnType);
  }

  case TYPE_ARRAY: {
    Type *elementType = typeSubstituteGenericParams(type->as.array.elementType,
                                                    params, args, count);

    if (elementType == type->as.array.elementType)
      return type;

    return typeArray(elementType);
  }

  case TYPE_STRUCT: {
    // A field/return type that's itself another generic instantiation
    // (e.g. `value: Box[T]` inside `struct Wrapper[T]`, or a generic
    // method's return type `Box[T]`) -- re-instantiate its origin with
    // its arguments substituted too.
    if (type->as.struct_.genericTypeArgCount == 0)
      return type;

    Type **newArgs = (Type **)typesAllocRaw(
        (size_t)type->as.struct_.genericTypeArgCount * sizeof(Type *));
    bool changed = false;
    for (int i = 0; i < type->as.struct_.genericTypeArgCount; i++) {
      newArgs[i] = typeSubstituteGenericParams(
          type->as.struct_.genericTypeArgs[i], params, args, count);
      if (newArgs[i] != type->as.struct_.genericTypeArgs[i])
        changed = true;
    }
    if (!changed)
      return type;

    if (type->as.struct_.genericOrigin == NULL)
      return type; // shouldn't happen -- an instantiated type always has one

    return typeStructInstantiate(type->as.struct_.genericOrigin, newArgs,
                                 type->as.struct_.genericTypeArgCount);
  }

  default:
    // Primitives and traits don't themselves contain a generic param --
    // only a signature or struct built from one can.
    return type;
  }
}

// Fields and methods are resolved lazily, on demand, by
// typeStructFieldLookup/typeStructInstanceMethodLookup/
// typeStructStaticMethodLookup below -- substituting from
// genericOrigin only for the one member actually being looked up.
//
// Eagerly copying and substituting every member here recurses forever for a
// method that returns the struct's own generic type -- e.g. a `new`
// static method returning `Box[T]`. Building Box[f64] would eagerly
// substitute `new`'s return type too, which is Box[T] again, which
// needs Box[f64] built again to substitute *its* `new`, forever. Doing
// this lazily breaks the cycle: instantiating Box[f64] costs nothing
// up front, and substituting `new`'s return type only happens if and
// when something actually looks `new` up.
Type *typeStructInstantiate(Type *genericType, Type **typeArgs,
                            int typeArgCount) {
  Type *instantiated = allocType(TYPE_STRUCT);
  instantiated->as.struct_.name = genericType->as.struct_.name;
  instantiated->as.struct_.genericTypeArgs = typeArgs;
  instantiated->as.struct_.genericTypeArgCount = typeArgCount;
  instantiated->as.struct_.genericOrigin = genericType;
  instantiated->as.struct_.hasUnresolvedMembers =
      genericType->as.struct_.hasUnresolvedMembers;

  return instantiated;
}

void typeStructMarkUnresolvedMembers(Type *type) {
  type->as.struct_.hasUnresolvedMembers = true;
}

bool typeStructHasUnresolvedMembers(Type *type) {
  return type != NULL && type->kind == TYPE_STRUCT &&
         type->as.struct_.hasUnresolvedMembers;
}

bool typesEqual(Type *a, Type *b) {
  if (a == b)
    return true;
  if (a == NULL || b == NULL)
    return false;
  if (a->kind != b->kind)
    return false;

  switch (a->kind) {
  case TYPE_UNIT:
  case TYPE_BOOL:
  case TYPE_STRING:
  case TYPE_F64:
  case TYPE_SELF:
    return true;

  case TYPE_GENERIC_PARAM:
    return false;

  case TYPE_STRUCT: {
    if (!internedNamesEqual(a->as.struct_.name, b->as.struct_.name))
      return false;

    if (a->as.struct_.genericTypeArgCount != b->as.struct_.genericTypeArgCount)
      return false;

    for (int i = 0; i < a->as.struct_.genericTypeArgCount; i++) {
      if (!typesEqual(a->as.struct_.genericTypeArgs[i],
                      b->as.struct_.genericTypeArgs[i]))
        return false;
    }

    return true;
  }

  case TYPE_TRAIT:
    // Nominal Equality
    return internedNamesEqual(a->as.trait_.name, b->as.trait_.name);

  case TYPE_FN:
    // Structural Equality
    if (a->as.function.paramCount != b->as.function.paramCount)
      return false;
    for (int i = 0; i < a->as.function.paramCount; i++) {
      if (!typesEqual(a->as.function.paramTypes[i],
                      b->as.function.paramTypes[i]))
        return false;
    }
    return typesEqual(a->as.function.returnType, b->as.function.returnType);

  case TYPE_ARRAY:
    return typesEqual(a->as.array.elementType, b->as.array.elementType);
  }

  return false; // unreachable
}

static Type *memberLookup(TypeMember *members, int count, Token name) {
  for (int i = 0; i < count; i++) {
    if (internedNameEqualsToken(members[i].name, name))
      return members[i].type;
  }
  return NULL;
}

Type *typeStructFieldLookup(Type *type, Token fieldName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  if (type->as.struct_.genericOrigin != NULL) {
    Type *origin = type->as.struct_.genericOrigin;
    Type *declared = memberLookup(origin->as.struct_.fields,
                                  origin->as.struct_.fieldCount, fieldName);

    if (declared == NULL)
      return NULL;

    return typeSubstituteGenericParams(
        declared, origin->as.struct_.genericTypeParams,
        type->as.struct_.genericTypeArgs, type->as.struct_.genericTypeArgCount);
  }

  return memberLookup(type->as.struct_.fields, type->as.struct_.fieldCount,
                      fieldName);
}

Type *typeStructInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  if (type->as.struct_.genericOrigin != NULL) {
    Type *origin = type->as.struct_.genericOrigin;
    Type *declared =
        memberLookup(origin->as.struct_.instanceMethods,
                     origin->as.struct_.instanceMethodCount, methodName);

    if (declared == NULL)
      return NULL;

    return typeSubstituteGenericParams(
        declared, origin->as.struct_.genericTypeParams,
        type->as.struct_.genericTypeArgs, type->as.struct_.genericTypeArgCount);
  }

  return memberLookup(type->as.struct_.instanceMethods,
                      type->as.struct_.instanceMethodCount, methodName);
}

Type *typeStructStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  if (type->as.struct_.genericOrigin != NULL) {
    Type *origin = type->as.struct_.genericOrigin;
    Type *declared =
        memberLookup(origin->as.struct_.staticMethods,
                     origin->as.struct_.staticMethodCount, methodName);

    if (declared == NULL)
      return NULL;

    return typeSubstituteGenericParams(
        declared, origin->as.struct_.genericTypeParams,
        type->as.struct_.genericTypeArgs, type->as.struct_.genericTypeArgCount);
  }

  return memberLookup(type->as.struct_.staticMethods,
                      type->as.struct_.staticMethodCount, methodName);
}

static bool memberIsPublicLookup(TypeMember *members, bool *isPublicArray,
                                 int count, Token name) {
  for (int i = 0; i < count; i++) {
    if (internedNameEqualsToken(members[i].name, name))
      return isPublicArray[i];
  }

  return false;
}

bool typeStructInstanceMethodIsPublic(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return false;

  if (type->as.struct_.genericOrigin != NULL) {
    Type *origin = type->as.struct_.genericOrigin;

    return memberIsPublicLookup(origin->as.struct_.instanceMethods,
                                origin->as.struct_.instanceMethodIsPublic,
                                origin->as.struct_.instanceMethodCount,
                                methodName);
  }

  return memberIsPublicLookup(type->as.struct_.instanceMethods,
                              type->as.struct_.instanceMethodIsPublic,
                              type->as.struct_.instanceMethodCount, methodName);
}

bool typeStructStaticMethodIsPublic(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return false;

  if (type->as.struct_.genericOrigin != NULL) {
    Type *origin = type->as.struct_.genericOrigin;

    return memberIsPublicLookup(origin->as.struct_.staticMethods,
                                origin->as.struct_.staticMethodIsPublic,
                                origin->as.struct_.staticMethodCount,
                                methodName);
  }

  return memberIsPublicLookup(type->as.struct_.staticMethods,
                              type->as.struct_.staticMethodIsPublic,
                              type->as.struct_.staticMethodCount, methodName);
}

Type *typeStructTraitInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  return memberLookup(type->as.struct_.traitInstanceMethods,
                      type->as.struct_.traitInstanceMethodCount, methodName);
}

Type *typeStructTraitStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  return memberLookup(type->as.struct_.traitStaticMethods,
                      type->as.struct_.traitStaticMethodCount, methodName);
}

Type *typeTraitInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_TRAIT)
    return NULL;

  return memberLookup(type->as.trait_.instanceMethods,
                      type->as.trait_.instanceMethodCount, methodName);
}

Type *typeTraitStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_TRAIT)
    return NULL;
  return memberLookup(type->as.trait_.staticMethods,
                      type->as.trait_.staticMethodCount, methodName);
}

int typeTraitInstanceMethodCount(Type *type) {
  if (type == NULL || type->kind != TYPE_TRAIT)
    return 0;
  return type->as.trait_.instanceMethodCount;
}

TypeMember typeTraitInstanceMethodAt(Type *type, int index) {
  return type->as.trait_.instanceMethods[index];
}

int typeTraitStaticMethodCount(Type *type) {
  if (type == NULL || type->kind != TYPE_TRAIT)
    return 0;
  return type->as.trait_.staticMethodCount;
}

TypeMember typeTraitStaticMethodAt(Type *type, int index) {
  return type->as.trait_.staticMethods[index];
}

static void appendTypeName(StrBuf *sb, Type *type) {
  if (type == NULL) {
    sb_append(sb, "<unknown>");
    return;
  }

  switch (type->kind) {
  case TYPE_UNIT:
    sb_append(sb, "unit");
    break;
  case TYPE_BOOL:
    sb_append(sb, "bool");
    break;
  case TYPE_STRING:
    sb_append(sb, "string");
    break;
  case TYPE_F64:
    sb_append(sb, "f64");
    break;
  case TYPE_STRUCT:
    sb_appendf(sb, "%.*s", type->as.struct_.name.length,
               internedNameChars(type->as.struct_.name));
    if (type->as.struct_.genericTypeArgCount > 0) {
      sb_append(sb, "[");

      for (int i = 0; i < type->as.struct_.genericTypeArgCount; i++) {
        if (i > 0)
          sb_append(sb, ", ");
        appendTypeName(sb, type->as.struct_.genericTypeArgs[i]);
      }

      sb_append(sb, "]");
    }
    break;
  case TYPE_GENERIC_PARAM:
    sb_appendf(sb, "%.*s", type->as.genericParam.name.length,
               internedNameChars(type->as.genericParam.name));
    break;
  case TYPE_TRAIT:
    sb_appendf(sb, "%.*s", type->as.trait_.name.length,
               internedNameChars(type->as.trait_.name));
    break;
  case TYPE_SELF:
    sb_append(sb, "Self");
    break;
  case TYPE_FN:
    sb_append(sb, "fun (");
    for (int i = 0; i < type->as.function.paramCount; i++) {
      if (i > 0)
        sb_append(sb, ", ");
      appendTypeName(sb, type->as.function.paramTypes[i]);
    }
    sb_append(sb, ") => ");
    appendTypeName(sb, type->as.function.returnType);
    break;
  case TYPE_ARRAY:
    sb_append(sb, "[");
    appendTypeName(sb, type->as.array.elementType);
    sb_append(sb, "]");
    break;
  }
}

const char *typeToString(Type *type) {
  StrBuf sb;
  sb_init(&sb);

  appendTypeName(&sb, type);

  char *result = (char *)typesAllocRaw(sb.len + 1);
  memcpy(result, sb.data, sb.len + 1);

  sb_free(&sb);
  return result;
}
