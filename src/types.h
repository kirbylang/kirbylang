#ifndef kirby_types_h
#define kirby_types_h

#include <stdbool.h>
#include <stddef.h>

#include "token.h"

typedef enum {
  TYPE_UNIT,
  TYPE_BOOL,
  TYPE_STRING,
  TYPE_F64,
  TYPE_STRUCT,
  TYPE_FN,
  TYPE_ARRAY,
} TypeKind;

typedef struct Type Type;

typedef struct {
  Token name;
  Type *type;
} TypeMember;

struct Type {
  TypeKind kind;
  union {
    // Nominal Equality
    struct {
      Token name;
      TypeMember *fields;
      int fieldCount;
      TypeMember *staticMethods;
      int staticMethodCount;
      TypeMember *instanceMethods;
      int instanceMethodCount;
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
  } as;
};

void *typesAllocRaw(size_t size);
void typesFreeAll(void);

// Copies a token's lexeme into the types arena.
//
// Tokens point into the source buffer of the unit that produced them.
// Anything reachable from a Type outlives that buffer, so names are
// interned on the way in.
Token typesInternToken(Token token);

// Returns pointer to the `unit` type
Type *typeUnit(void);

// Returns pointer to the `bool` type
Type *typeBool(void);

// Returns pointer to the `string` type
Type *typeString(void);

// Returns pointer to the `f64` type
Type *typeF64(void);

// Allocates a new struct type
// Returns pointer to the new type in the types arena
Type *typeStruct(Token name, TypeMember *fields, int fieldCount,
                 TypeMember *staticMethods, int staticMethodCount,
                 TypeMember *instanceMethods, int instanceMethodCount);

// Allocates a new function type
// Returns pointer to the new type in the types arena
Type *typeFunction(Type **paramTypes, int paramCount, Type *returnType);

// Allocates a new array type
// Returns pointer to the new type in the types arena
Type *typeArray(Type *elementType);

void typeStructSetFields(Type *type, TypeMember *fields, int fieldCount);
void typeStructAddStaticMethod(Type *type, Token name, Type *methodType);
void typeStructAddInstanceMethod(Type *type, Token name, Type *methodType);

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

// A short, human-readable name for error messages, e.g. "f64", "bool",
// "Point", "fun (f64) => f64".
const char *typeToString(Type *type);

#endif
