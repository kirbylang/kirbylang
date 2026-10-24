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
static Type *numberSingletons[NUMBER_KIND_COUNT + 1];
static Type *selfPlaceholderSingleton = NULL;
static Type *anyIntegerSingleton = NULL;

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
  memset(numberSingletons, 0, sizeof(numberSingletons));
  selfPlaceholderSingleton = NULL;
  anyIntegerSingleton = NULL;

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

Token internedNameToToken(InternedName name) {
  Token token;
  token.type = TOKEN_IDENTIFIER;
  token.start = internedNameChars(name);
  token.length = name.length;
  token.line = 0;
  return token;
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

Type *typeNumber(NumberKind kind) {
  if (numberSingletons[kind] == NULL) {
    numberSingletons[kind] = allocType(TYPE_NUMBER);
    numberSingletons[kind]->as.number.kind = kind;
  }
  return numberSingletons[kind];
}

Type *typeAnyNumber(void) { return typeNumber(NUMBER_KIND_COUNT); }

Type *typeAnyInteger(void) {
  if (anyIntegerSingleton == NULL) {
    anyIntegerSingleton = allocType(TYPE_NUMBER);
    anyIntegerSingleton->as.number.kind = NUMBER_KIND_COUNT;
  }
  return anyIntegerSingleton;
}

Type *typeF64(void) { return typeNumber(NUMBER_KIND_F64); }

Type *typeU8(void) { return typeNumber(NUMBER_KIND_U8); }

bool typeIsNumber(const Type *type) { return type->kind == TYPE_NUMBER; }

bool typeAccepts(const Type *expected, const Type *actual) {
  if (expected == NULL || actual == NULL)
    return true;

  if (expected == typeAnyNumber())
    return typeIsNumber(actual);

  if (expected == typeAnyInteger())
    return typeIsNumber(actual) &&
           actual->as.number.kind != NUMBER_KIND_COUNT &&
           !numberKindInfo(actual->as.number.kind)->isFloat;

  if (expected->kind == TYPE_LIST && actual->kind == TYPE_LIST)
    return typeAccepts(expected->as.list.elementType,
                       actual->as.list.elementType);

  if (expected->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY)
    return (ARRAY_LENGTH_IS_PLACEHOLDER(expected->as.array.length) ||
            expected->as.array.length == actual->as.array.length) &&
           typeAccepts(expected->as.array.elementType,
                       actual->as.array.elementType);

  if (expected->kind == TYPE_FN && actual->kind == TYPE_FN) {
    if (expected->as.function.paramCount != actual->as.function.paramCount)
      return false;

    for (int i = 0; i < expected->as.function.paramCount; i++) {
      if (!typeAccepts(expected->as.function.paramTypes[i],
                       actual->as.function.paramTypes[i]))
        return false;
    }

    return typeAccepts(expected->as.function.returnType,
                       actual->as.function.returnType);
  }

  if (expected->kind == TYPE_STRUCT && actual->kind == TYPE_STRUCT &&
      expected->as.struct_.typeArgCount > 0) {
    if (!internedNamesEqual(expected->as.struct_.name,
                            actual->as.struct_.name) ||
        expected->as.struct_.typeArgCount != actual->as.struct_.typeArgCount)
      return false;

    for (int i = 0; i < expected->as.struct_.typeArgCount; i++) {
      if (!typeAccepts(expected->as.struct_.typeArgs[i],
                       actual->as.struct_.typeArgs[i]))
        return false;
    }
    return true;
  }

  return typesEqual(expected, actual);
}

Type *typeSelfPlaceholder(void) {
  if (selfPlaceholderSingleton == NULL)
    selfPlaceholderSingleton = allocType(TYPE_SELF);
  return selfPlaceholderSingleton;
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

Type *typeStruct(Token name, UninternedTypeMember *fields, int fieldCount) {
  Type *type = allocType(TYPE_STRUCT);
  type->as.struct_.name = internTokenName(name);
  type->as.struct_.fields = internMembers(fields, fieldCount);
  type->as.struct_.fieldCount = fieldCount;

  return type;
}

Type *typeFunction(Type **paramTypes, int paramCount, Type *returnType) {
  Type *type = allocType(TYPE_FN);
  type->as.function.paramTypes = paramTypes;
  type->as.function.paramCount = paramCount;
  type->as.function.returnType = returnType;
  return type;
}

Type *typeList(Type *elementType) {
  Type *type = allocType(TYPE_LIST);
  type->as.list.elementType = elementType;
  return type;
}

Type *typeArray(Type *elementType, int length) {
  Type *type = allocType(TYPE_ARRAY);
  type->as.array.elementType = elementType;
  type->as.array.length = length;
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
  InternedName *newList =
      (InternedName *)typesAllocRaw(newCount * sizeof(InternedName));

  if (type->as.struct_.implementedTraitCount > 0) {
    memcpy(newList, type->as.struct_.implementedTraits,
           (size_t)type->as.struct_.implementedTraitCount *
               sizeof(InternedName));
  }

  newList[newCount - 1] = traitName;
  type->as.struct_.implementedTraits = newList;
  type->as.struct_.implementedTraitCount = newCount;
}

bool typeStructImplementsTrait(Type *type, InternedName traitName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return false;
  Type *declaration = typeStructDeclaration(type);
  for (int i = 0; i < declaration->as.struct_.implementedTraitCount; i++) {
    if (internedNamesEqual(declaration->as.struct_.implementedTraits[i],
                           traitName))
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

  case TYPE_LIST: {
    Type *elementType = typeSubstituteSelf(type->as.list.elementType, concrete);
    if (elementType == type->as.list.elementType)
      return type;
    return typeList(elementType);
  }

  case TYPE_ARRAY: {
    Type *elementType =
        typeSubstituteSelf(type->as.array.elementType, concrete);
    if (elementType == type->as.array.elementType)
      return type;
    return typeArray(elementType, type->as.array.length);
  }

  default:
    // Primitives, structs, and traits don't themselves contain Self --
    // only a signature built from them (a TYPE_FN) can.
    return type;
  }
}

Type *typeGenericParam(int index) {
  Type *type = allocType(TYPE_GENERIC_PARAM);
  type->as.genericParam.index = index;
  return type;
}

Type *typeDeclaredGenericParam(Token name, int index, bool isOpaque) {
  Type *type = typeGenericParam(index);
  type->as.genericParam.name = internTokenName(name);
  type->as.genericParam.hasName = true;
  type->as.genericParam.isOpaque = isOpaque;
  return type;
}

Type *typeFindGenericParam(Type *type, int index) {
  if (type == NULL)
    return NULL;

  switch (type->kind) {
  case TYPE_GENERIC_PARAM:
    return type->as.genericParam.index == index ? type : NULL;

  case TYPE_LIST:
    return typeFindGenericParam(type->as.list.elementType, index);

  case TYPE_STRUCT:
    for (int i = 0; i < type->as.struct_.typeArgCount; i++) {
      Type *found = typeFindGenericParam(type->as.struct_.typeArgs[i], index);
      if (found != NULL)
        return found;
    }
    return NULL;

  case TYPE_ARRAY:
    return typeFindGenericParam(type->as.array.elementType, index);

  case TYPE_FN: {
    for (int i = 0; i < type->as.function.paramCount; i++) {
      Type *found =
          typeFindGenericParam(type->as.function.paramTypes[i], index);
      if (found != NULL)
        return found;
    }
    return typeFindGenericParam(type->as.function.returnType, index);
  }

  default:
    return NULL;
  }
}

// A bound length is kept in the slot as an array type with no item type.
static Type *lengthBinding(int length) { return typeArray(NULL, length); }

int typeGenericParamCount(Type *type) {
  if (type == NULL)
    return 0;

  switch (type->kind) {
  case TYPE_GENERIC_PARAM:
    return type->as.genericParam.isOpaque ? 0 : type->as.genericParam.index + 1;

  case TYPE_LIST:
    return typeGenericParamCount(type->as.list.elementType);

  case TYPE_STRUCT: {
    int count = 0;

    for (int i = 0; i < type->as.struct_.typeArgCount; i++) {
      int argCount = typeGenericParamCount(type->as.struct_.typeArgs[i]);
      if (argCount > count)
        count = argCount;
    }

    return count;
  }

  case TYPE_ARRAY: {
    int count = typeGenericParamCount(type->as.array.elementType);

    if (ARRAY_LENGTH_IS_PLACEHOLDER(type->as.array.length)) {
      int slots = ARRAY_LENGTH_SLOT(type->as.array.length) + 1;
      if (slots > count)
        count = slots;
    }

    return count;
  }

  case TYPE_FN: {
    int count = typeGenericParamCount(type->as.function.returnType);

    for (int i = 0; i < type->as.function.paramCount; i++) {
      int paramCount = typeGenericParamCount(type->as.function.paramTypes[i]);
      if (paramCount > count)
        count = paramCount;
    }

    return count;
  }

  default:
    return 0;
  }
}

void typeBindGenericParams(Type *pattern, Type *actual, Type **bindings,
                           int count) {
  if (pattern == NULL || actual == NULL)
    return;

  switch (pattern->kind) {
  case TYPE_GENERIC_PARAM: {
    int index = pattern->as.genericParam.index;

    if (pattern->as.genericParam.isOpaque)
      return;

    if (index < count && bindings[index] == NULL)
      bindings[index] = actual;
    return;
  }

  case TYPE_LIST:
    if (actual->kind == TYPE_LIST)
      typeBindGenericParams(pattern->as.list.elementType,
                            actual->as.list.elementType, bindings, count);
    return;

  case TYPE_STRUCT:
    if (actual->kind != TYPE_STRUCT ||
        !internedNamesEqual(pattern->as.struct_.name,
                            actual->as.struct_.name) ||
        pattern->as.struct_.typeArgCount != actual->as.struct_.typeArgCount)
      return;

    for (int i = 0; i < pattern->as.struct_.typeArgCount; i++) {
      typeBindGenericParams(pattern->as.struct_.typeArgs[i],
                            actual->as.struct_.typeArgs[i], bindings, count);
    }
    return;

  case TYPE_ARRAY:
    if (actual->kind != TYPE_ARRAY)
      return;

    typeBindGenericParams(pattern->as.array.elementType,
                          actual->as.array.elementType, bindings, count);

    if (ARRAY_LENGTH_IS_PLACEHOLDER(pattern->as.array.length)) {
      int slot = ARRAY_LENGTH_SLOT(pattern->as.array.length);

      if (slot < count && bindings[slot] == NULL &&
          !ARRAY_LENGTH_IS_PLACEHOLDER(actual->as.array.length))
        bindings[slot] = lengthBinding(actual->as.array.length);
    }
    return;

  case TYPE_FN:
    if (actual->kind != TYPE_FN ||
        actual->as.function.paramCount != pattern->as.function.paramCount)
      return;

    for (int i = 0; i < pattern->as.function.paramCount; i++) {
      typeBindGenericParams(pattern->as.function.paramTypes[i],
                            actual->as.function.paramTypes[i], bindings, count);
    }

    typeBindGenericParams(pattern->as.function.returnType,
                          actual->as.function.returnType, bindings, count);
    return;

  default:
    return;
  }
}

// Replaces each generic parameter with its slot. An empty slot keeps the
// parameter, or makes it unknown when `eraseEmpty`.
static Type *substituteGenericParams(Type *type, Type **bindings, int count,
                                     bool eraseEmpty) {
  if (type == NULL)
    return NULL;

  switch (type->kind) {
  case TYPE_GENERIC_PARAM: {
    int index = type->as.genericParam.index;
    Type *bound = index < count ? bindings[index] : NULL;

    if (type->as.genericParam.isOpaque)
      return type;

    if (bound != NULL)
      return bound;

    return eraseEmpty ? NULL : type;
  }

  case TYPE_LIST: {
    Type *elementType = substituteGenericParams(type->as.list.elementType,
                                                bindings, count, eraseEmpty);

    if (elementType == type->as.list.elementType)
      return type;

    return typeList(elementType);
  }

  case TYPE_STRUCT: {
    int argCount = type->as.struct_.typeArgCount;
    if (argCount == 0)
      return type;

    Type **typeArgs = (Type **)typesAllocRaw((size_t)argCount * sizeof(Type *));
    bool changed = false;

    for (int i = 0; i < argCount; i++) {
      typeArgs[i] = substituteGenericParams(type->as.struct_.typeArgs[i],
                                            bindings, count, eraseEmpty);
      if (typeArgs[i] != type->as.struct_.typeArgs[i])
        changed = true;
    }

    if (!changed)
      return type;

    return typeStructInstance(type->as.struct_.typeConstructor, typeArgs,
                              argCount);
  }

  case TYPE_ARRAY: {
    Type *elementType = substituteGenericParams(type->as.array.elementType,
                                                bindings, count, eraseEmpty);
    int length = type->as.array.length;

    if (ARRAY_LENGTH_IS_PLACEHOLDER(length)) {
      int slot = ARRAY_LENGTH_SLOT(length);

      if (slot < count && bindings[slot] != NULL)
        length = bindings[slot]->as.array.length;
    }

    if (elementType == type->as.array.elementType &&
        length == type->as.array.length)
      return type;

    return typeArray(elementType, length);
  }

  case TYPE_FN: {
    int paramCount = type->as.function.paramCount;
    Type **paramTypes =
        paramCount > 0
            ? (Type **)typesAllocRaw((size_t)paramCount * sizeof(Type *))
            : NULL;
    bool changed = false;

    for (int i = 0; i < paramCount; i++) {
      paramTypes[i] = substituteGenericParams(type->as.function.paramTypes[i],
                                              bindings, count, eraseEmpty);
      if (paramTypes[i] != type->as.function.paramTypes[i])
        changed = true;
    }

    Type *returnType = substituteGenericParams(type->as.function.returnType,
                                               bindings, count, eraseEmpty);

    if (!changed && returnType == type->as.function.returnType)
      return type;

    return typeFunction(paramTypes, paramCount, returnType);
  }

  default:
    return type;
  }
}

Type *typeSubstituteGenericParams(Type *type, Type **bindings, int count) {
  return substituteGenericParams(type, bindings, count, false);
}

Type *typeEraseGenericParams(Type *type) {
  return substituteGenericParams(type, NULL, 0, true);
}

void typeStructSetGenericParamCount(Type *type, int count) {
  type->as.struct_.genericParamCount = count;
}

bool typeStructIsGeneric(Type *type) {
  return type != NULL && type->kind == TYPE_STRUCT &&
         type->as.struct_.genericParamCount > 0;
}

Type *typeStructInstance(Type *base, Type **typeArgs, int typeArgCount) {
  Type *type = allocType(TYPE_STRUCT);
  type->as.struct_.name = base->as.struct_.name;
  type->as.struct_.typeConstructor = base;
  type->as.struct_.typeArgCount = typeArgCount;

  if (typeArgCount > 0) {
    type->as.struct_.typeArgs =
        (Type **)typesAllocRaw((size_t)typeArgCount * sizeof(Type *));
    memcpy(type->as.struct_.typeArgs, typeArgs,
           (size_t)typeArgCount * sizeof(Type *));
  }

  return type;
}

Type *typeStructDeclaration(Type *type) {
  return type->as.struct_.typeConstructor != NULL
             ? type->as.struct_.typeConstructor
             : type;
}

// A member found on a declaration, as seen from `type`: for an instance such
// as `Box[f64]`, its type arguments stand in for the declaration's parameters.
static Type *instantiateMember(Type *type, Type *member) {
  if (member == NULL || type->as.struct_.typeConstructor == NULL)
    return member;

  return typeSubstituteGenericParams(member, type->as.struct_.typeArgs,
                                     type->as.struct_.typeArgCount);
}

void typeStructMarkUnresolvedMembers(Type *type) {
  type->as.struct_.hasUnresolvedMembers = true;
}

bool typeStructHasUnresolvedMembers(Type *type) {
  return type != NULL && type->kind == TYPE_STRUCT &&
         typeStructDeclaration(type)->as.struct_.hasUnresolvedMembers;
}

bool typesEqual(const Type *a, const Type *b) {
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
  case TYPE_SELF:
    return true;

  case TYPE_NUMBER:
    return a->as.number.kind == b->as.number.kind;

  case TYPE_STRUCT:
    // Nominal Equality, and the type arguments of an instance
    if (!internedNamesEqual(a->as.struct_.name, b->as.struct_.name) ||
        a->as.struct_.typeArgCount != b->as.struct_.typeArgCount)
      return false;

    for (int i = 0; i < a->as.struct_.typeArgCount; i++) {
      if (!typesEqual(a->as.struct_.typeArgs[i], b->as.struct_.typeArgs[i]))
        return false;
    }
    return true;

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

  case TYPE_LIST:
    return typesEqual(a->as.list.elementType, b->as.list.elementType);

  case TYPE_ARRAY:
    return a->as.array.length == b->as.array.length &&
           typesEqual(a->as.array.elementType, b->as.array.elementType);

  case TYPE_GENERIC_PARAM:
    // Opaque parameters are equal only to themselves, which the check at the
    // top already caught
    if (a->as.genericParam.isOpaque || b->as.genericParam.isOpaque)
      return false;
    return a->as.genericParam.index == b->as.genericParam.index;
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
  Type *declaration = typeStructDeclaration(type);
  return instantiateMember(
      type, memberLookup(declaration->as.struct_.fields,
                         declaration->as.struct_.fieldCount, fieldName));
}

Type *typeStructInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;
  Type *declaration = typeStructDeclaration(type);
  return instantiateMember(
      type,
      memberLookup(declaration->as.struct_.instanceMethods,
                   declaration->as.struct_.instanceMethodCount, methodName));
}

Type *typeStructStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;
  Type *declaration = typeStructDeclaration(type);
  return instantiateMember(
      type,
      memberLookup(declaration->as.struct_.staticMethods,
                   declaration->as.struct_.staticMethodCount, methodName));
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

  Type *declaration = typeStructDeclaration(type);
  return memberIsPublicLookup(declaration->as.struct_.instanceMethods,
                              declaration->as.struct_.instanceMethodIsPublic,
                              declaration->as.struct_.instanceMethodCount,
                              methodName);
}

bool typeStructStaticMethodIsPublic(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return false;

  Type *declaration = typeStructDeclaration(type);
  return memberIsPublicLookup(declaration->as.struct_.staticMethods,
                              declaration->as.struct_.staticMethodIsPublic,
                              declaration->as.struct_.staticMethodCount,
                              methodName);
}

Type *typeStructTraitInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  Type *declaration = typeStructDeclaration(type);
  return instantiateMember(
      type, memberLookup(declaration->as.struct_.traitInstanceMethods,
                         declaration->as.struct_.traitInstanceMethodCount,
                         methodName));
}

Type *typeStructTraitStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;

  Type *declaration = typeStructDeclaration(type);
  return instantiateMember(
      type,
      memberLookup(declaration->as.struct_.traitStaticMethods,
                   declaration->as.struct_.traitStaticMethodCount, methodName));
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

static void appendArrayLength(StrBuf *sb, int length) {
  if (ARRAY_LENGTH_IS_PLACEHOLDER(length)) {
    static const char names[] = "NMLK";
    int index = -1 - length;
    char name[2] = {index < (int)sizeof(names) - 1 ? names[index] : '?', '\0'};
    sb_append(sb, name);
    return;
  }

  char digits[16];
  snprintf(digits, sizeof(digits), "%d", length);
  sb_append(sb, digits);
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
  case TYPE_NUMBER:
    if (type == anyIntegerSingleton)
      sb_append(sb, "integer");
    else
      sb_append(sb, type->as.number.kind == NUMBER_KIND_COUNT
                        ? "number"
                        : numberKindInfo(type->as.number.kind)->name);
    break;
  case TYPE_STRUCT:
    sb_appendf(sb, "%.*s", type->as.struct_.name.length,
               internedNameChars(type->as.struct_.name));
    if (type->as.struct_.typeArgCount > 0) {
      sb_append(sb, "[");
      for (int i = 0; i < type->as.struct_.typeArgCount; i++) {
        if (i > 0)
          sb_append(sb, ", ");
        appendTypeName(sb, type->as.struct_.typeArgs[i]);
      }
      sb_append(sb, "]");
    }
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
  case TYPE_LIST:
    sb_append(sb, "[");
    appendTypeName(sb, type->as.list.elementType);
    sb_append(sb, "]");
    break;
  case TYPE_ARRAY:
    sb_append(sb, "[");
    appendTypeName(sb, type->as.array.elementType);
    sb_append(sb, "; ");
    appendArrayLength(sb, type->as.array.length);
    sb_append(sb, "]");
    break;
  case TYPE_GENERIC_PARAM: {
    static const char names[] = "TUVW";
    int index = type->as.genericParam.index;

    if (type->as.genericParam.hasName)
      sb_appendf(sb, "%.*s", type->as.genericParam.name.length,
                 internedNameChars(type->as.genericParam.name));
    else if (index < (int)sizeof(names) - 1)
      sb_appendf(sb, "%c", names[index]);
    else
      sb_appendf(sb, "T%d", index);
    break;
  }
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
