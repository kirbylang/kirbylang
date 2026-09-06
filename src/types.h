#ifndef kirby_types_h
#define kirby_types_h

#include <stdbool.h>
#include <stddef.h>

#include "stringset.h"
#include "token.h"

typedef enum {
  TYPE_UNIT,
  TYPE_BOOL,
  TYPE_STRING,
  TYPE_F64,
  TYPE_STRUCT,
  TYPE_FN,
  TYPE_ARRAY,
  TYPE_TRAIT,
  TYPE_SELF, // `Self`
} TypeKind;

typedef struct Type Type;

typedef struct {
  int offset; // Offset into the owning StringSet
  int length;
} InternedName;

typedef struct {
  Token name;
  Type *type;
} UninternedTypeMember;

typedef struct {
  InternedName name;
  Type *type;
} TypeMember;

struct Type {
  TypeKind kind;
  union {
    // Nominal Equality
    struct {
      InternedName name;
      TypeMember *fields;
      int fieldCount;
      TypeMember *staticMethods;
      int staticMethodCount;
      TypeMember *instanceMethods;
      int instanceMethodCount;
      TypeMember *traitInstanceMethods;
      int traitInstanceMethodCount;
      TypeMember *traitStaticMethods;
      int traitStaticMethodCount;
      InternedName *implementedTraits;
      int implementedTraitCount;
      bool isGeneric;
      // True if any field/method failed to resolve for any reason
      // (missing annotation, unknown type, etc)
      bool hasUnresolvedMembers;
    } struct_;
    // Structural Equality
    struct {
      Type **paramTypes;
      int paramCount;
      Type *returnType;
    } function;
    struct {
      Type *elementType;
    } array;
    struct {
      InternedName name;
      TypeMember *staticMethods;
      int staticMethodCount;
      TypeMember *instanceMethods;
      int instanceMethodCount;
      bool hasSupertrait;
      InternedName supertraitName;
      bool hasUnresolvedMembers;
    } trait_;
  } as;
};

void *typesAllocRaw(size_t size);
void typesFreeAll(void);

InternedName internTokenName(Token token);

// Not a C string
const char *internedNameChars(InternedName name);

bool internedNamesEqual(InternedName a, InternedName b);

bool internedNameEqualsToken(InternedName name, Token token);

// Returns pointer to the `unit` type
Type *typeUnit(void);

// Returns pointer to the `bool` type
Type *typeBool(void);

// Returns pointer to the `string` type
Type *typeString(void);

// Returns pointer to the `f64` type
Type *typeF64(void);

// Returns pointer to the `Self` placeholder type
Type *typeSelfPlaceholder(void);

// Allocates a new struct type
// Returns pointer to the new type in the types arena
Type *typeStruct(Token name, UninternedTypeMember *fields, int fieldCount,
                 UninternedTypeMember *staticMethods, int staticMethodCount,
                 UninternedTypeMember *instanceMethods,
                 int instanceMethodCount);

// Allocates a new function type
// Returns pointer to the new type in the types arena
Type *typeFunction(Type **paramTypes, int paramCount, Type *returnType);

// Allocates a new array type
// Returns pointer to the new type in the types arena
Type *typeArray(Type *elementType);

// Allocates a new trait type.
// Returns pointer to the new type in the types arena
Type *typeTrait(Token name, UninternedTypeMember *staticMethods,
                int staticMethodCount, UninternedTypeMember *instanceMethods,
                int instanceMethodCount);

void typeStructSetFields(Type *type, UninternedTypeMember *fields,
                         int fieldCount);
void typeStructAddStaticMethod(Type *type, Token name, Type *methodType);
void typeStructAddInstanceMethod(Type *type, Token name, Type *methodType);
// `hasSelf` picks which of traitInstanceMethods/traitStaticMethods the
// method is stored in -- see the field's doc comment above.
void typeStructAddTraitMethod(Type *type, Token name, Type *methodType,
                              bool hasSelf);

// Records that `type` has an `impl Trait for` block, where `traitName` is
// the trait's interned name. Used for flat coherence checking (has this
// (trait, type) pair already been implemented?) and for checking whether a
// type implements a given trait (e.g. `==` requiring Eq).
void typeStructMarkTraitImplemented(Type *type, InternedName traitName);
bool typeStructImplementsTrait(Type *type, InternedName traitName);

void typeTraitSetMethods(Type *type, UninternedTypeMember *staticMethods,
                         int staticMethodCount,
                         UninternedTypeMember *instanceMethods,
                         int instanceMethodCount);
void typeTraitSetSupertrait(Type *type, InternedName supertraitName);

void typeTraitMarkUnresolvedMembers(Type *type);
bool typeTraitHasUnresolvedMembers(Type *type);

// Replaces every TYPE_SELF found inside `type` with a `concrete` type.
Type *typeSubstituteSelf(Type *type, Type *concrete);

// Marks a struct as using generic parameters in its own declaration. See
// the isGeneric field's doc comment above for why this exists.
void typeStructMarkGeneric(Type *type);
bool typeStructIsGeneric(Type *type);

// Marks a struct as having one or more fields/methods that failed to
// resolve for any reason. See the hasUnresolvedMembers field's doc
// comment above for why this exists, and how it differs from isGeneric.
void typeStructMarkUnresolvedMembers(Type *type);
bool typeStructHasUnresolvedMembers(Type *type);

// Returns bool if two types are equal
bool typesEqual(Type *a, Type *b);

// Returns NULL if `type` isn't TYPE_STRUCT or has no field with this name.
Type *typeStructFieldLookup(Type *type, Token fieldName);
Type *typeStructInstanceMethodLookup(Type *type, Token methodName);
Type *typeStructStaticMethodLookup(Type *type, Token methodName);
Type *typeStructTraitInstanceMethodLookup(Type *type, Token methodName);
Type *typeStructTraitStaticMethodLookup(Type *type, Token methodName);

// Returns NULL if `type` isn't TYPE_TRAIT or has no required method with
// this name in the given category.
Type *typeTraitInstanceMethodLookup(Type *type, Token methodName);
Type *typeTraitStaticMethodLookup(Type *type, Token methodName);

int typeTraitInstanceMethodCount(Type *type);
TypeMember typeTraitInstanceMethodAt(Type *type, int index);
int typeTraitStaticMethodCount(Type *type);
TypeMember typeTraitStaticMethodAt(Type *type, int index);

// A short, human-readable name for error messages, e.g. "f64", "bool",
// "Point", "fun (f64) => f64".
const char *typeToString(Type *type);

#endif
