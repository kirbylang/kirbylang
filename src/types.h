#ifndef kirby_types_h
#define kirby_types_h

#include <stdbool.h>
#include <stddef.h>

#include "number_kind.h"
#include "stringset.h"
#include "token.h"

typedef enum {
  TYPE_UNIT,
  TYPE_BOOL,
  TYPE_STRING,
  TYPE_NUMBER,
  TYPE_STRUCT,
  TYPE_FN,
  TYPE_LIST,
  TYPE_ARRAY,
  TYPE_TRAIT,
  TYPE_SELF,          // `Self`
  TYPE_GENERIC_PARAM, // A placeholder such as the `T` in `fun ([T]) => T`
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
      bool *staticMethodIsPublic;
      TypeMember *instanceMethods;
      int instanceMethodCount;
      bool *instanceMethodIsPublic;
      TypeMember *traitInstanceMethods;
      int traitInstanceMethodCount;
      TypeMember *traitStaticMethods;
      int traitStaticMethodCount;
      InternedName *implementedTraits;
      int implementedTraitCount;
      // How many type parameters `struct Box[T]` declares. 0 if none.
      int genericParamCount;
      // For an instance such as `Box[f64]`: the declaration it comes from and
      // the type arguments. An instance holds no members of its own. Looking
      // one up finds it on the declaration and substitutes the arguments.

      // The generic struct this is an instance of: `Box` for `Box[f64]`.
      // NULL if this type is a type constructor, or a struct that isn't generic
      Type *typeConstructor;
      Type **typeArgs;
      int typeArgCount;
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
      NumberKind kind;
    } number;
    struct {
      Type *elementType;
    } list;
    struct {
      Type *elementType;
      int length; // Negative values are length placeholders, see below
    } array;
    struct {
      // 0 is T, 1 is U, 2 is V, and so on
      int index;
      // The name the program gave it. Native signatures have none.
      InternedName name;
      bool hasName;
      // A declared parameter as seen inside its own declaration: one unknown
      // type that is never filled in and is only equal to itself.
      bool isOpaque;
    } genericParam;
    struct {
      InternedName name;
      TypeMember *staticMethods;
      int staticMethodCount;
      TypeMember *instanceMethods;
      int instanceMethodCount;
      bool hasSupertrait;
      InternedName supertraitName;
      bool hasUnresolvedMembers;
      bool isBuiltin;
    } trait_;
  } as;
};

void *typesAllocRaw(size_t size);
void typesFreeAll(void);

InternedName internTokenName(Token token);

// Not a C string
const char *internedNameChars(InternedName name);

// The identifier token for an interned name. Its text lives in the type arena.
Token internedNameToToken(InternedName name);

bool internedNamesEqual(InternedName a, InternedName b);

bool internedNameEqualsToken(InternedName name, Token token);

// Returns pointer to the `unit` type
Type *typeUnit(void);

// Returns pointer to the `bool` type
Type *typeBool(void);

// Returns pointer to the `string` type
Type *typeString(void);

// Returns pointer to the type of numbers of this kind
Type *typeNumber(NumberKind kind);

Type *typeF64(void);
Type *typeU8(void);

bool typeIsNumber(const Type *type);

// Matches a number of any kind. Only usable as a native parameter type.
Type *typeAnyNumber(void);

// Matches an integer of any kind. Only usable as a native parameter type.
Type *typeAnyInteger(void);

// Whether a value of type `actual` can be given where `expected` is wanted.
// An unknown (NULL) type, or an unknown part of one such as the element type
// of `[]`, accepts and is accepted by anything.
bool typeAccepts(const Type *expected, const Type *actual);

// Returns pointer to the `Self` placeholder type
Type *typeSelfPlaceholder(void);

// Allocates a new struct type
// Returns pointer to the new type in the types arena
Type *typeStruct(Token name, UninternedTypeMember *fields, int fieldCount);

// Allocates a new function type
// Returns pointer to the new type in the types arena
Type *typeFunction(Type **paramTypes, int paramCount, Type *returnType);

// Allocates a new list type, such as `[i64]`
// Returns pointer to the new type in the types arena
Type *typeList(Type *elementType);

// Allocates a new fixed size array type, such as `[i64; 3]`
// A `length` of -1 is the placeholder `N`, -2 is `M`, and so on.
// Returns pointer to the new type in the types arena
Type *typeArray(Type *elementType, int length);

#define ARRAY_LENGTH_PLACEHOLDER(index) (-1 - (index))
#define ARRAY_LENGTH_IS_PLACEHOLDER(length) ((length) < 0)

// Length placeholders share the binding slots of the generic parameters. `T`
// to `W` take the first four, then `N` takes the next, and so on.
#define GENERIC_TYPE_PARAM_SLOTS 4
#define ARRAY_LENGTH_SLOT(length) (GENERIC_TYPE_PARAM_SLOTS - 1 - (length))

// Allocates a new trait type.
// Returns pointer to the new type in the types arena
Type *typeTrait(Token name, UninternedTypeMember *staticMethods,
                int staticMethodCount, UninternedTypeMember *instanceMethods,
                int instanceMethodCount);

void typeStructSetFields(Type *type, UninternedTypeMember *fields,
                         int fieldCount);
void typeStructAddStaticMethod(Type *type, Token name, Type *methodType,
                               bool isPublic);
void typeStructAddInstanceMethod(Type *type, Token name, Type *methodType,
                                 bool isPublic);
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

void typeTraitMarkBuiltin(Type *type);
bool typeTraitIsBuiltin(Type *type);

// Replaces every TYPE_SELF found inside `type` with a `concrete` type.
Type *typeSubstituteSelf(Type *type, Type *concrete);

// A generic parameter. 0 is T, 1 is U, and so on.
Type *typeGenericParam(int index);

// A generic parameter a program declared, such as the `T` in `fun id[T]`.
// Callers fill it in unless `isOpaque`, which is the form it has inside its own
// declaration.
Type *typeDeclaredGenericParam(Token name, int index, bool isOpaque);

// The first generic parameter with this index found inside `type`, or NULL.
Type *typeFindGenericParam(Type *type, int index);

// One more than the largest generic parameter index inside `type`, so 0 when
// it has none. Opaque parameters don't count.
int typeGenericParamCount(Type *type);

// Fills empty `bindings` slots by lining `pattern` up with `actual`: where
// `pattern` has generic parameter i and slot i is empty, slot i becomes the
// matching part of `actual`. Looks inside arrays and functions.
void typeBindGenericParams(Type *pattern, Type *actual, Type **bindings,
                           int count);

// Replaces each generic parameter inside `type` with its slot in `bindings`.
// Parameters with an empty slot are kept.
Type *typeSubstituteGenericParams(Type *type, Type **bindings, int count);

// True if no part of `type` is unknown (NULL).
bool typeIsFullyKnown(const Type *type);

// Replaces each generic parameter left inside `type` with unknown (NULL).
Type *typeEraseGenericParams(Type *type);

// Records how many type parameters a struct declaration has.
void typeStructSetGenericParamCount(Type *type, int count);

// True for a struct declared with type parameters, such as `Box` in
// `struct Box[T]`, and false for an instance such as `Box[f64]`.
bool typeStructIsGeneric(Type *type);

// `Box[f64]`: the declaration `base` with these type arguments.
Type *typeStructInstance(Type *base, Type **typeArgs, int typeArgCount);

// The declaration a struct type comes from: `Box` for `Box[f64]`, and the
// type itself for a declaration or a struct that isn't generic.
Type *typeStructDeclaration(Type *type);

// Marks a struct as having one or more fields/methods that failed to
// resolve for any reason. See the hasUnresolvedMembers field's doc
// comment above for why this exists.
void typeStructMarkUnresolvedMembers(Type *type);
bool typeStructHasUnresolvedMembers(Type *type);

// Returns bool if two types are equal
bool typesEqual(const Type *a, const Type *b);

// Returns NULL if `type` isn't TYPE_STRUCT or has no field with this name.
Type *typeStructFieldLookup(Type *type, Token fieldName);
Type *typeStructInstanceMethodLookup(Type *type, Token methodName);
Type *typeStructStaticMethodLookup(Type *type, Token methodName);
Type *typeStructTraitInstanceMethodLookup(Type *type, Token methodName);
Type *typeStructTraitStaticMethodLookup(Type *type, Token methodName);

bool typeStructInstanceMethodIsPublic(Type *type, Token methodName);
bool typeStructStaticMethodIsPublic(Type *type, Token methodName);

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
