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

Type *typeStruct(Token name, UninternedTypeMember *fields, int fieldCount,
                 UninternedTypeMember *staticMethods, int staticMethodCount,
                 UninternedTypeMember *instanceMethods,
                 int instanceMethodCount) {
  Type *type = allocType(TYPE_STRUCT);
  type->as.struct_.name = internTokenName(name);
  type->as.struct_.fields = internMembers(fields, fieldCount);
  type->as.struct_.fieldCount = fieldCount;
  type->as.struct_.staticMethods =
      internMembers(staticMethods, staticMethodCount);
  type->as.struct_.staticMethodCount = staticMethodCount;
  type->as.struct_.instanceMethods =
      internMembers(instanceMethods, instanceMethodCount);
  type->as.struct_.instanceMethodCount = instanceMethodCount;
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

void typeStructAddStaticMethod(Type *type, Token name, Type *methodType) {
  appendMember(&type->as.struct_.staticMethods,
               &type->as.struct_.staticMethodCount, name, methodType);
}

void typeStructAddInstanceMethod(Type *type, Token name, Type *methodType) {
  appendMember(&type->as.struct_.instanceMethods,
               &type->as.struct_.instanceMethodCount, name, methodType);
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

  case TYPE_STRUCT:
    // Nominal Equality
    return internedNamesEqual(a->as.struct_.name, b->as.struct_.name);

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
  return memberLookup(type->as.struct_.fields, type->as.struct_.fieldCount,
                      fieldName);
}

Type *typeStructInstanceMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;
  return memberLookup(type->as.struct_.instanceMethods,
                      type->as.struct_.instanceMethodCount, methodName);
}

Type *typeStructStaticMethodLookup(Type *type, Token methodName) {
  if (type == NULL || type->kind != TYPE_STRUCT)
    return NULL;
  return memberLookup(type->as.struct_.staticMethods,
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
