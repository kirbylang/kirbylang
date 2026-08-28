#include <assert.h>
#include <string.h>

#include "../src/token.h"
#include "../src/types.h"

static Token makeToken(const char *text) {
  Token t;
  t.type = TOKEN_IDENTIFIER;
  t.start = text;
  t.length = (int)strlen(text);
  t.line = 1;
  return t;
}

static void test_primitives_are_singletons(void) {
  assert(typeUnit() == typeUnit());
  assert(typeBool() == typeBool());
  assert(typeString() == typeString());
  assert(typeF64() == typeF64());

  assert(typeUnit() != typeBool());
  assert(typeBool() != typeString());
  assert(typeString() != typeF64());
}

static void test_primitive_equality(void) {
  assert(typesEqual(typeUnit(), typeUnit()));
  assert(typesEqual(typeBool(), typeBool()));
  assert(typesEqual(typeString(), typeString()));
  assert(typesEqual(typeF64(), typeF64()));

  assert(!typesEqual(typeBool(), typeF64()));
  assert(!typesEqual(typeString(), typeUnit()));

  assert(!typesEqual(NULL, typeBool()));
  assert(!typesEqual(typeBool(), NULL));
}

static void test_struct_equality_is_nominal(void) {
  Token pointName = makeToken("Point");
  Token otherPointName = makeToken("Point"); // same text, different Token
  Token vectorName = makeToken("Vector");

  TypeMember pointFields[] = {
      {makeToken("x"), typeF64()},
      {makeToken("y"), typeF64()},
  };

  TypeMember vectorLikeFields[] = {
      {makeToken("magnitude"), typeF64()},
  };

  Type *point = typeStruct(pointName, pointFields, 2, NULL, 0, NULL, 0);
  Type *pointAgain =
      typeStruct(otherPointName, vectorLikeFields, 1, NULL, 0, NULL, 0);
  Type *vector = typeStruct(vectorName, pointFields, 2, NULL, 0, NULL, 0);

  assert(typesEqual(point, pointAgain)); // same name -> equal, fields ignored
  assert(!typesEqual(point, vector));    // different name -> not equal
}

static void test_function_equality_is_structural(void) {
  Type *f64ToF64Params[] = {typeF64()};
  Type *f64ToF64 = typeFunction(f64ToF64Params, 1, typeF64());

  Type *f64ToF64AgainParams[] = {typeF64()};
  Type *f64ToF64Again = typeFunction(f64ToF64AgainParams, 1, typeF64());

  Type *f64ToStringParams[] = {typeF64()};
  Type *f64ToString = typeFunction(f64ToStringParams, 1, typeString());

  Type *twoParamsParams[] = {typeF64(), typeF64()};
  Type *twoParams = typeFunction(twoParamsParams, 2, typeF64());

  Type *zeroParams = typeFunction(NULL, 0, typeUnit());
  Type *zeroParamsAgain = typeFunction(NULL, 0, typeUnit());

  assert(typesEqual(f64ToF64, f64ToF64Again));     // same shape, diff pointers
  assert(!typesEqual(f64ToF64, f64ToString));      // different return
  assert(!typesEqual(f64ToF64, twoParams));        // different arity
  assert(typesEqual(zeroParams, zeroParamsAgain)); // both zero-arity
}

static void test_struct_field_lookup(void) {
  TypeMember fields[] = {
      {makeToken("x"), typeF64()},
      {makeToken("label"), typeString()},
  };
  Type *point = typeStruct(makeToken("Point"), fields, 2, NULL, 0, NULL, 0);

  assert(typeStructFieldLookup(point, makeToken("x")) == typeF64());
  assert(typeStructFieldLookup(point, makeToken("label")) == typeString());
  assert(typeStructFieldLookup(point, makeToken("missing")) == NULL);

  assert(typeStructFieldLookup(typeF64(), makeToken("x")) == NULL);
  assert(typeStructFieldLookup(NULL, makeToken("x")) == NULL);
}

static void test_type_to_string(void) {
  assert(strcmp(typeToString(typeUnit()), "unit") == 0);
  assert(strcmp(typeToString(typeBool()), "bool") == 0);
  assert(strcmp(typeToString(typeString()), "string") == 0);
  assert(strcmp(typeToString(typeF64()), "f64") == 0);

  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  assert(strcmp(typeToString(point), "Point") == 0);

  Type *addParams[] = {typeF64(), typeF64()};
  Type *add = typeFunction(addParams, 2, typeF64());
  assert(strcmp(typeToString(add), "fun (f64, f64) => f64") == 0);

  // Nested function type
  Type *callbackParams[] = {typeF64()};
  Type *callback = typeFunction(callbackParams, 1, typeF64());
  Type *higherOrderParams[] = {callback, typeF64()};
  Type *higherOrder = typeFunction(higherOrderParams, 2, typeF64());
  assert(strcmp(typeToString(higherOrder),
                "fun (fun (f64) => f64, f64) => f64") == 0);
}

static void test_array_equality_is_structural(void) {
  Type *f64Array = typeArray(typeF64());
  Type *f64ArrayAgain = typeArray(typeF64());
  Type *stringArray = typeArray(typeString());

  assert(typesEqual(f64Array, f64ArrayAgain)); // same element type
  assert(!typesEqual(f64Array, stringArray));  // different element type
  assert(!typesEqual(f64Array, typeF64()));    // not even the same kind

  // Multidimensional: array-of-array-of-f64 recurses correctly.
  Type *matrix = typeArray(typeArray(typeF64()));
  Type *matrixAgain = typeArray(typeArray(typeF64()));
  Type *notAMatrix = typeArray(typeArray(typeString()));
  assert(typesEqual(matrix, matrixAgain));
  assert(!typesEqual(matrix, notAMatrix));

  // The empty-array case: no element to infer from yet.
  Type *emptyArray = typeArray(NULL);
  Type *emptyArrayAgain = typeArray(NULL);
  assert(typesEqual(emptyArray, emptyArrayAgain));
  assert(!typesEqual(emptyArray, f64Array));
}

static void test_array_to_string(void) {
  assert(strcmp(typeToString(typeArray(typeF64())), "[f64]") == 0);
  assert(strcmp(typeToString(typeArray(typeArray(typeF64()))), "[[f64]]") == 0);
  assert(strcmp(typeToString(typeArray(NULL)), "[<unknown>]") == 0);
}

static void test_struct_method_lookup(void) {
  Type *f64ToF64Params[] = {typeF64()};
  Type *deposit = typeFunction(f64ToF64Params, 1, typeF64());

  Type *accountParams[] = {typeF64()};
  Type *newAccount =
      typeFunction(accountParams, 1,
                   typeStruct(makeToken("Account"), NULL, 0, NULL, 0, NULL, 0));

  TypeMember instanceMethods[] = {{makeToken("deposit"), deposit}};
  TypeMember staticMethods[] = {{makeToken("new"), newAccount}};
  TypeMember fields[] = {{makeToken("balance"), typeF64()}};

  Type *account = typeStruct(makeToken("Account"), fields, 1, staticMethods, 1,
                             instanceMethods, 1);

  // Instance methods and static methods live in separate lookups --
  // a static method isn't found via the instance lookup and vice versa.
  assert(typeStructInstanceMethodLookup(account, makeToken("deposit")) ==
         deposit);
  assert(typeStructInstanceMethodLookup(account, makeToken("new")) == NULL);
  assert(typeStructStaticMethodLookup(account, makeToken("new")) == newAccount);
  assert(typeStructStaticMethodLookup(account, makeToken("deposit")) == NULL);

  // Fields stay in their own lookup, unaffected by methods existing now.
  assert(typeStructFieldLookup(account, makeToken("balance")) == typeF64());
  assert(typeStructFieldLookup(account, makeToken("deposit")) == NULL);

  // Non-struct types and NULL still return NULL, not crash, for both new
  // lookups too.
  assert(typeStructInstanceMethodLookup(typeF64(), makeToken("x")) == NULL);
  assert(typeStructStaticMethodLookup(NULL, makeToken("x")) == NULL);
}

static void test_incremental_struct_construction(void) {
  // The self-referential case: struct Node { var next: Node; } --
  // register the placeholder first, then set fields onto the *same*
  // pointer, so the field's own type (Node) is the real, complete one.
  Type *node = typeStruct(makeToken("Node"), NULL, 0, NULL, 0, NULL, 0);
  TypeMember fields[] = {{makeToken("next"), node}};
  typeStructSetFields(node, fields, 1);

  assert(node->as.struct_.fieldCount == 1);
  assert(typeStructFieldLookup(node, makeToken("next")) == node);
}

static void test_incremental_struct_methods_across_multiple_calls(void) {
  // Simulates multiple impl blocks contributing methods to the same
  // struct one at a time, in any order.
  Type *counter = typeStruct(makeToken("Counter"), NULL, 0, NULL, 0, NULL, 0);

  Type *newType = typeFunction(NULL, 0, counter);
  typeStructAddStaticMethod(counter, makeToken("new"), newType);

  Type *getType = typeFunction(NULL, 0, typeF64());
  typeStructAddInstanceMethod(counter, makeToken("get"), getType);

  Type *incParams[] = {typeF64()};
  Type *incType = typeFunction(incParams, 1, typeUnit());
  typeStructAddInstanceMethod(counter, makeToken("increment"), incType);

  assert(typeStructStaticMethodLookup(counter, makeToken("new")) == newType);
  assert(typeStructInstanceMethodLookup(counter, makeToken("get")) ==
        getType);
  assert(typeStructInstanceMethodLookup(counter, makeToken("increment")) ==
        incType);
  // Adding instance methods didn't disturb the static one, or vice versa.
  assert(typeStructStaticMethodLookup(counter, makeToken("get")) == NULL);
  assert(typeStructInstanceMethodLookup(counter, makeToken("new")) == NULL);
}

int main(void) {
  test_primitives_are_singletons();
  test_primitive_equality();
  test_struct_equality_is_nominal();
  test_function_equality_is_structural();
  test_struct_field_lookup();
  test_struct_method_lookup();
  test_incremental_struct_construction();
  test_incremental_struct_methods_across_multiple_calls();
  test_type_to_string();
  test_array_equality_is_structural();
  test_array_to_string();

  typesFreeAll();

  return 0;
}
