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

  UninternedTypeMember pointFields[] = {
      {makeToken("x"), typeF64()},
      {makeToken("y"), typeF64()},
  };

  UninternedTypeMember vectorLikeFields[] = {
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
  UninternedTypeMember fields[] = {
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

  UninternedTypeMember instanceMethods[] = {{makeToken("deposit"), deposit}};
  UninternedTypeMember staticMethods[] = {{makeToken("new"), newAccount}};
  UninternedTypeMember fields[] = {{makeToken("balance"), typeF64()}};

  Type *accountStruct = typeStruct(makeToken("Account"), fields, 1,
                                   staticMethods, 1, instanceMethods, 1);

  // Instance methods and static methods live in separate lookups --
  // a static method isn't found via the instance lookup and vice versa.
  assert(typeStructInstanceMethodLookup(accountStruct, makeToken("deposit")) ==
         deposit);
  assert(typeStructInstanceMethodLookup(accountStruct, makeToken("new")) ==
         NULL);
  assert(typeStructStaticMethodLookup(accountStruct, makeToken("new")) ==
         newAccount);
  assert(typeStructStaticMethodLookup(accountStruct, makeToken("deposit")) ==
         NULL);

  // Fields stay in their own lookup, unaffected by methods existing now.
  assert(typeStructFieldLookup(accountStruct, makeToken("balance")) ==
         typeF64());
  assert(typeStructFieldLookup(accountStruct, makeToken("deposit")) == NULL);

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
  UninternedTypeMember fields[] = {{makeToken("next"), node}};
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
  assert(typeStructInstanceMethodLookup(counter, makeToken("get")) == getType);
  assert(typeStructInstanceMethodLookup(counter, makeToken("increment")) ==
         incType);
  // Adding instance methods didn't disturb the static one, or vice versa.
  assert(typeStructStaticMethodLookup(counter, makeToken("get")) == NULL);
  assert(typeStructInstanceMethodLookup(counter, makeToken("new")) == NULL);
}

static void test_struct_trait_method_lookup_is_separate_from_instance(void) {
  // Methods from `impl Trait for X` land in a different array than
  // `impl X` methods, so coherence checking can tell which impl block a
  // method came from -- see typeStructAddTraitMethod's doc comment.
  // Also split by hasSelf, same as the struct's own static/instance
  // methods, so a static trait method (e.g. Default.default()) can't be
  // called as if it were an instance method or vice versa.
  Type *accountStruct =
      typeStruct(makeToken("Account"), NULL, 0, NULL, 0, NULL, 0);

  Type *toStringType = typeFunction(NULL, 0, typeString());
  typeStructAddTraitMethod(accountStruct, makeToken("toString"), toStringType,
                           /*hasSelf=*/true);

  Type *defaultType = typeFunction(NULL, 0, typeF64());
  typeStructAddTraitMethod(accountStruct, makeToken("default"), defaultType,
                           /*hasSelf=*/false);

  assert(typeStructTraitInstanceMethodLookup(
             accountStruct, makeToken("toString")) == toStringType);
  assert(typeStructTraitStaticMethodLookup(
             accountStruct, makeToken("default")) == defaultType);

  // Not visible via the plain instance-method lookup.
  assert(typeStructInstanceMethodLookup(accountStruct, makeToken("toString")) ==
         NULL);

  // Not visible via the other trait-method category.
  assert(typeStructTraitStaticMethodLookup(accountStruct,
                                           makeToken("toString")) == NULL);

  assert(typeStructTraitInstanceMethodLookup(accountStruct,
                                             makeToken("default")) == NULL);
  assert(typeStructTraitInstanceMethodLookup(typeF64(),
                                             makeToken("toString")) == NULL);
  assert(typeStructTraitInstanceMethodLookup(NULL, makeToken("toString")) ==
         NULL);
}

static void test_struct_trait_coherence_bookkeeping(void) {
  Token displayName = makeToken("Display");
  Token eqName = makeToken("Eq");
  InternedName display = internTokenName(displayName);
  InternedName eq = internTokenName(eqName);

  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);

  assert(!typeStructImplementsTrait(point, display));
  assert(!typeStructImplementsTrait(point, eq));

  typeStructMarkTraitImplemented(point, display);

  assert(typeStructImplementsTrait(point, display));
  assert(!typeStructImplementsTrait(point, eq)); // only Display so far

  typeStructMarkTraitImplemented(point, eq);

  assert(typeStructImplementsTrait(point, display));
  assert(typeStructImplementsTrait(point, eq));

  assert(!typeStructImplementsTrait(NULL, display));
  assert(!typeStructImplementsTrait(typeF64(), display));
}

static void test_trait_construction_and_method_lookup(void) {
  UninternedTypeMember instanceMethods[] = {
      {makeToken("toString"), typeFunction(NULL, 0, typeString())},
  };
  UninternedTypeMember staticMethods[] = {
      {makeToken("default"), typeFunction(NULL, 0, typeSelfPlaceholder())},
  };
  Type *display =
      typeTrait(makeToken("Display"), staticMethods, 1, instanceMethods, 1);

  assert(display->kind == TYPE_TRAIT);
  assert(strcmp(typeToString(display), "Display") == 0);

  Type *found = typeTraitInstanceMethodLookup(display, makeToken("toString"));
  assert(found != NULL);
  assert(typesEqual(found, typeFunction(NULL, 0, typeString())));
  assert(typeTraitInstanceMethodLookup(display, makeToken("missing")) == NULL);
  // Static and instance methods live in separate lookups, same as structs.
  assert(typeTraitInstanceMethodLookup(display, makeToken("default")) == NULL);
  assert(typeTraitStaticMethodLookup(display, makeToken("default")) != NULL);
  assert(typeTraitStaticMethodLookup(display, makeToken("toString")) == NULL);

  assert(typeTraitInstanceMethodCount(display) == 1);
  TypeMember first = typeTraitInstanceMethodAt(display, 0);
  assert(internedNameEqualsToken(first.name, makeToken("toString")));
  assert(typeTraitStaticMethodCount(display) == 1);

  // Non-trait types and NULL are safe no-ops, same as the struct lookups.
  assert(typeTraitInstanceMethodLookup(typeF64(), makeToken("toString")) ==
         NULL);
  assert(typeTraitInstanceMethodLookup(NULL, makeToken("toString")) == NULL);
}

static void test_trait_equality_is_nominal(void) {
  Type *displayA = typeTrait(makeToken("Display"), NULL, 0, NULL, 0);
  Type *displayB = typeTrait(makeToken("Display"), NULL, 0, NULL, 0);
  Type *eq = typeTrait(makeToken("Eq"), NULL, 0, NULL, 0);

  assert(typesEqual(displayA, displayB)); // same name -> equal
  assert(!typesEqual(displayA, eq));
  assert(!typesEqual(displayA, typeF64())); // different kind entirely
}

static void test_trait_supertrait_and_unresolved_flag(void) {
  Type *eq = typeTrait(makeToken("Eq"), NULL, 0, NULL, 0);
  Type *ord = typeTrait(makeToken("Ord"), NULL, 0, NULL, 0);

  assert(!ord->as.trait_.hasSupertrait);

  typeTraitSetSupertrait(ord, eq->as.trait_.name);

  assert(ord->as.trait_.hasSupertrait);
  assert(internedNamesEqual(ord->as.trait_.supertraitName, eq->as.trait_.name));

  assert(!typeTraitHasUnresolvedMembers(ord));
  typeTraitMarkUnresolvedMembers(ord);
  assert(typeTraitHasUnresolvedMembers(ord));

  assert(!typeTraitHasUnresolvedMembers(NULL));
  assert(!typeTraitHasUnresolvedMembers(typeF64()));
}

static void test_self_placeholder_is_singleton_and_always_equal(void) {
  assert(typeSelfPlaceholder() == typeSelfPlaceholder());
  assert(typeSelfPlaceholder()->kind == TYPE_SELF);
  assert(strcmp(typeToString(typeSelfPlaceholder()), "Self") == 0);
  // Self is a placeholder, not a concrete type -- typesEqual treats every
  // occurrence of it as interchangeable, same as the other primitives.
  assert(typesEqual(typeSelfPlaceholder(), typeSelfPlaceholder()));
}

static void test_substitute_self_replaces_placeholder(void) {
  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);

  // Bare Self.
  assert(typeSubstituteSelf(typeSelfPlaceholder(), point) == point);

  // Self doesn't appear -- the exact same pointer comes back, unchanged.
  assert(typeSubstituteSelf(typeF64(), point) == typeF64());
  assert(typeSubstituteSelf(point, point) == point);

  // Self as a parameter: fun (Self) => bool
  Type *selfParams[] = {typeSelfPlaceholder()};
  Type *equalsSig = typeFunction(selfParams, 1, typeBool());
  Type *substituted = typeSubstituteSelf(equalsSig, point);
  assert(substituted != equalsSig); // rebuilt, since it did contain Self
  assert(substituted->kind == TYPE_FN);
  assert(substituted->as.function.paramTypes[0] == point);
  assert(substituted->as.function.returnType == typeBool());

  // Self as a return type: fun () => Self
  Type *defaultSig = typeFunction(NULL, 0, typeSelfPlaceholder());
  Type *substitutedDefault = typeSubstituteSelf(defaultSig, point);
  assert(substitutedDefault->as.function.returnType == point);

  // Self nested inside an array element type: [Self]
  Type *selfArray = typeArray(typeSelfPlaceholder());
  Type *substitutedArray = typeSubstituteSelf(selfArray, point);
  assert(substitutedArray->as.array.elementType == point);

  // A signature with no Self anywhere comes back as the identical pointer,
  // not a needless rebuild.
  Type *plainParams[] = {typeF64()};
  Type *plainSig = typeFunction(plainParams, 1, typeBool());
  assert(typeSubstituteSelf(plainSig, point) == plainSig);
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
  test_struct_trait_method_lookup_is_separate_from_instance();
  test_struct_trait_coherence_bookkeeping();
  test_trait_construction_and_method_lookup();
  test_trait_equality_is_nominal();
  test_trait_supertrait_and_unresolved_flag();
  test_self_placeholder_is_singleton_and_always_equal();
  test_substitute_self_replaces_placeholder();

  typesFreeAll();

  return 0;
}
