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
  TYPE_FUNCTION,
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
      // No self -- accessed on the struct name itself, e.g. Point.new(...).
      TypeMember *staticMethods;
      int staticMethodCount;
      // self excluded from each Type*'s param list, same as FunctionNode's
      // params/paramTypes already exclude it -- self's type is always
      // just "this struct," never worth spelling out per-method.
      TypeMember *instanceMethods;
      int instanceMethodCount;
    } struct_;
    // Structural Equality
    struct {
      Type **paramTypes;
      int paramCount;
      Type *returnType;
    } function;
    // Structural, single element type -- always the result of inference
    // (array literal, or indexing into an already-known array), never of
    // resolveType(). There's no `: Type` annotation syntax that produces
    // this yet -- `List[T]`/`[T; N]` aren't supported. Recursion handles
    // multidimensional arrays ([[1, 2], [3, 4]]) for free.
    struct {
      Type *elementType;
    } array;
  } as;
};

void *typesAllocRaw(size_t size);
void typesFreeAll(void);

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
