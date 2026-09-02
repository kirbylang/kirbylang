#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/typecheck.h"
#include "../src/types.h"

static Token makeToken(const char *text) {
  Token t;
  t.type = TOKEN_IDENTIFIER;
  t.start = text;
  t.length = (int)strlen(text);
  t.line = 1;
  return t;
}

// Parses `source` and returns the declaredType/paramTypes[0]/returnType
// node from its first declaration -- whichever call site below actually
// needs, they each just want "the NODE_TYPE this snippet produces".
static AstNode *parseFirstVarType(const char *source) {
  int outCount = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &outCount, &hadError, &endLine);
  assert(!hadError);
  assert(outCount >= 1);
  assert(ast[0]->kind == NODE_VAR_DECL);
  AstNode *declaredType = ast[0]->as.varDecl.declaredType;
  assert(declaredType != NULL);
  return declaredType;
}

static void test_scope_declare_and_lookup(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);

  typchkTypeEnvDeclare(env, makeToken("x"), typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("missing")) == NULL);

  typchkTypeEnvEndScope(env);
  typchkTypeEnvDestroy(env);
}

static void test_scope_shadowing(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env); // outer
  typchkTypeEnvDeclare(env, makeToken("x"), typeF64());

  typchkTypeEnvBeginScope(env); // inner
  typchkTypeEnvDeclare(env, makeToken("x"), typeString());
  assert(typchkTypeEnvLookup(env, makeToken("x")) ==
         typeString()); // inner wins
  typchkTypeEnvEndScope(env);

  // Back in the outer scope -- inner's shadow is gone.
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeF64());

  typchkTypeEnvEndScope(env);
  typchkTypeEnvDestroy(env);
}

static void test_struct_and_function_registries(void) {
  TypeEnv *env = typchkTypeEnvCreate();

  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), point);
  assert(typchkTypeEnvLookupStruct(env, makeToken("Point")) == point);
  assert(typchkTypeEnvLookupStruct(env, makeToken("Missing")) == NULL);

  Type *addParams[] = {typeF64(), typeF64()};
  Type *add = typeFunction(addParams, 2, typeF64());
  typchkTypeEnvRegisterFunction(env, makeToken("add"), add);
  assert(typchkTypeEnvLookupFunction(env, makeToken("add")) == add);
  assert(typchkTypeEnvLookupFunction(env, makeToken("missing")) == NULL);

  typchkTypeEnvDestroy(env);
}

static void test_resolve_primitives(void) {
  TypeEnv *env = typchkTypeEnvCreate();

  assert(typchkResolveType(env, parseFirstVarType("var x: unit;")) ==
         typeUnit());
  assert(typchkResolveType(env, parseFirstVarType("var x: bool;")) ==
         typeBool());
  assert(typchkResolveType(env, parseFirstVarType("var x: string;")) ==
         typeString());
  assert(typchkResolveType(env, parseFirstVarType("var x: f64;")) == typeF64());

  typchkTypeEnvDestroy(env);
}

static void test_resolve_registered_struct(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), point);

  assert(typchkResolveType(env, parseFirstVarType("var x: Point;")) == point);

  typchkTypeEnvDestroy(env);
}

static void test_resolve_unknown_name_errors(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  typchkResetError();

  Type *result = typchkResolveType(env, parseFirstVarType("var x: Bogus;"));
  assert(result == NULL);
  assert(typchkHadError());

  typchkResetError();
  typchkTypeEnvDestroy(env);
}

static void test_resolve_generic_type_errors(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  typchkResetError();

  Type *result = typchkResolveType(env, parseFirstVarType("var x: List[f64];"));
  assert(result == NULL);
  assert(typchkHadError());

  typchkResetError();
  typchkTypeEnvDestroy(env);
}

static void test_resolve_function_type(void) {
  TypeEnv *env = typchkTypeEnvCreate();

  Type *result = typchkResolveType(
      env, parseFirstVarType("var x: fun (f64, f64) => f64;"));
  assert(result != NULL);
  assert(result->kind == TYPE_FN);
  assert(result->as.function.paramCount == 2);
  assert(result->as.function.paramTypes[0] == typeF64());
  assert(result->as.function.paramTypes[1] == typeF64());
  assert(result->as.function.returnType == typeF64());

  // Zero-param case, and a nested function type in the param list.
  Type *nested = typchkResolveType(
      env, parseFirstVarType("var x: fun (fun () => bool) => unit;"));
  assert(nested != NULL);
  assert(nested->as.function.paramCount == 1);
  Type *innerParam = nested->as.function.paramTypes[0];
  assert(innerParam->kind == TYPE_FN);
  assert(innerParam->as.function.paramCount == 0);
  assert(innerParam->as.function.returnType == typeBool());
  assert(nested->as.function.returnType == typeUnit());

  typchkTypeEnvDestroy(env);
}

static void test_resolve_function_type_propagates_inner_error(void) {
  TypeEnv *env = typchkTypeEnvCreate();
  typchkResetError();

  // The unknown type is buried inside a function-type parameter --
  // typchkResolveType() must still catch it, not just check the top level.
  Type *result =
      typchkResolveType(env, parseFirstVarType("var x: fun (Bogus) => unit;"));
  assert(result == NULL);
  assert(typchkHadError());

  typchkResetError();
  typchkTypeEnvDestroy(env);
}

// Parses `source` and runs typchkCheckStmt() over every top-level declaration
// in order -- doesn't populate struct/function registries the way
// typchkCheckProgram() does (see typchkCheckStmt's NODE_STRUCT/NODE_IMPL case);
// tests below that need a struct register it directly via
// typchkTypeEnvRegisterStruct().
static TypeEnv *checkProgram(const char *source) {
  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &outCount, &hadParseError, &endLine);
  assert(!hadParseError);

  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  for (int i = 0; i < outCount; i++) {
    typchkCheckStmt(env, ast[i]);
  }
  return env;
}

static void test_literals(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var a = 5; var b = \"hi\"; var c = true; "
                              "var d = nil;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("a")) == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("b")) == typeString());
  assert(typchkTypeEnvLookup(env, makeToken("c")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("d")) == typeUnit());
  typchkTypeEnvDestroy(env);
}

static void test_binary_arithmetic_and_concat(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "var sum = 1 + 2; var product = 3 * 4; var greeting = \"a\" + \"b\";");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("sum")) == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("product")) == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("greeting")) == typeString());
  typchkTypeEnvDestroy(env);
}

static void test_binary_plus_mismatch_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = 1 + \"two\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_binary_arithmetic_requires_f64(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = \"a\" - \"b\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_comparisons(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("var a = 1 < 2; var b = 1 == 1; var c = true == false;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("a")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("b")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("c")) == typeBool());
  typchkTypeEnvDestroy(env);
}

static void test_equality_requires_same_type(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = 1 == \"one\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_ordering_requires_f64(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = \"a\" < \"b\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_unary(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("var a = !5; var b = !\"\"; var c = !!5; var d = -5;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("a")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("b")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("c")) == typeBool());
  assert(typchkTypeEnvLookup(env, makeToken("d")) == typeF64());
  typchkTypeEnvDestroy(env);
}

static void test_negate_requires_f64(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = -\"a\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_and_or_produce_bool(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var flag = true or false;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("flag")) == typeBool());
  typchkTypeEnvDestroy(env);
}

static void test_and_or_non_bool_operand_errors(void) {
  typchkResetError();
  // Only nil and false are falsey, so this yields "" rather than the
  // default it looks like it picks. `??` is the operator for that.
  TypeEnv *env = checkProgram("var name = \"\" or \"default\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_and_or_mismatched_type_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = true and \"oops\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_nullish_result_comes_from_fallback(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = nil ?? \"fallback\";");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeString());
  typchkTypeEnvDestroy(env);
}

static void test_function_call_checked(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("fun add(a: f64, b: f64): f64 = a + b; var x = add(1, 2);");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeF64());
  typchkTypeEnvDestroy(env);
}

static void test_function_call_wrong_arg_type_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("fun add(a: f64, b: f64): f64 = a + b; var x = "
                              "add(1, \"two\");");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_function_call_wrong_arity_errors(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("fun add(a: f64, b: f64): f64 = a + b; var x = add(1);");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_native_call_is_unchecked(void) {
  typchkResetError();
  // `clock` isn't declared anywhere Kirby-visible -- presumed native,
  // not an error, and the whole call infers as "no opinion."
  TypeEnv *env = checkProgram("var x = clock();");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == NULL);
  typchkTypeEnvDestroy(env);
}

static void test_struct_instance_field_and_method_access(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);

  TypeMember fields[] = {{makeToken("balance"), typeF64()}};
  Type *f64ToF64Params[] = {typeF64()};
  Type *depositType = typeFunction(f64ToF64Params, 1, typeF64());
  TypeMember instanceMethods[] = {{makeToken("deposit"), depositType}};
  Type *account =
      typeStruct(makeToken("Account"), fields, 1, NULL, 0, instanceMethods, 1);
  typchkTypeEnvRegisterStruct(env, makeToken("Account"), account);
  typchkTypeEnvDeclare(env, makeToken("a"), account);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse("var balance = a.balance; var result = a.deposit(50);",
                        &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  for (int i = 0; i < outCount; i++)
    typchkCheckStmt(env, ast[i]);

  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("balance")) == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("result")) == typeF64());

  typchkTypeEnvDestroy(env);
}

static void test_struct_static_method_access(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);

  Type *pointType = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  Type *newParams[] = {typeF64(), typeF64()};
  Type *newType = typeFunction(newParams, 2, pointType);
  TypeMember staticMethods[] = {{makeToken("new"), newType}};
  Type *point =
      typeStruct(makeToken("Point"), NULL, 0, staticMethods, 1, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), point);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast =
      parse("var p = Point.new(1, 2);", &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  for (int i = 0; i < outCount; i++)
    typchkCheckStmt(env, ast[i]);

  assert(!typchkHadError());
  // p's type is a *different* Type* instance than `point` (nominal
  // equality, not pointer identity) -- typesEqual is the right check.
  assert(typesEqual(typchkTypeEnvLookup(env, makeToken("p")), point));

  typchkTypeEnvDestroy(env);
}

static void test_local_variable_shadows_struct_name_for_get(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);

  // Register a real struct "Point" with a static method "origin" -- then
  // declare a *local variable*, also named "Point", holding an unrelated
  // struct with a field "x". Point.x should resolve through the local
  // shadowing the struct name (matching how a local shadows a global of
  // the same name at the bytecode level today), not accidentally hit
  // static-method lookup against the real Point struct.
  Type *originType = typeFunction(
      NULL, 0, typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0));
  TypeMember staticMethods[] = {{makeToken("origin"), originType}};
  Type *pointStructType =
      typeStruct(makeToken("Point"), NULL, 0, staticMethods, 1, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), pointStructType);

  TypeMember otherFields[] = {{makeToken("x"), typeF64()}};
  Type *otherType =
      typeStruct(makeToken("Other"), otherFields, 1, NULL, 0, NULL, 0);
  typchkTypeEnvDeclare(env, makeToken("Point"),
                       otherType); // shadows the struct

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast =
      parse("var result = Point.x;", &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("result")) == typeF64());

  typchkTypeEnvDestroy(env);
}

static void test_struct_unknown_field_errors(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  typchkTypeEnvDeclare(env, makeToken("p"), point);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast =
      parse("var x = p.bogus;", &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_struct_init(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  TypeMember fields[] = {{makeToken("x"), typeF64()},
                         {makeToken("y"), typeF64()}};
  Type *point = typeStruct(makeToken("Point"), fields, 2, NULL, 0, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), point);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse("var p = Point { x: 1, y: 2 };", &outCount,
                        &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(!typchkHadError());
  assert(typesEqual(typchkTypeEnvLookup(env, makeToken("p")), point));
  typchkTypeEnvDestroy(env);
}

static void test_struct_init_wrong_field_type_errors(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  TypeMember fields[] = {{makeToken("x"), typeF64()}};
  Type *point = typeStruct(makeToken("Point"), fields, 1, NULL, 0, NULL, 0);
  typchkTypeEnvRegisterStruct(env, makeToken("Point"), point);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse("var p = Point { x: \"wrong\" };", &outCount,
                        &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_self_type(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  Type *point = typeStruct(makeToken("Point"), NULL, 0, NULL, 0, NULL, 0);
  // Sets self-type directly to test typchkInferSelf() in isolation, rather
  // than going through a whole method body via typchkCheckFunctionBody().
  typchkTypeEnvSetSelfType(env, point);

  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse("var x = self;", &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == point);

  typchkTypeEnvDestroy(env);
}

static void test_self_outside_method_errors(void) {
  typchkResetError();
  TypeEnv *env = typchkTypeEnvCreate();
  typchkTypeEnvBeginScope(env);
  // No typchkTypeEnvSetSelfType call -- stays NULL, matching "not currently
  // checking a method body." The parser itself allows bare `self`
  // anywhere (the compiler's own "self outside a method" rejection is a
  // separate, later, compile-time check, not a parse-time one) -- this
  // confirms the checker reports its own diagnostic if that compiler
  // typchkCheck somehow didn't already catch it first.
  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse("var x = self;", &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  typchkCheckStmt(env, ast[0]);

  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_array_literal_and_index(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "var nums = [1, 2, 3]; var first = nums[0]; var empty = [];");
  assert(!typchkHadError());
  Type *numsType = typchkTypeEnvLookup(env, makeToken("nums"));
  assert(numsType != NULL && numsType->kind == TYPE_ARRAY);
  assert(numsType->as.array.elementType == typeF64());
  assert(typchkTypeEnvLookup(env, makeToken("first")) == typeF64());
  Type *emptyType = typchkTypeEnvLookup(env, makeToken("empty"));
  assert(emptyType != NULL && emptyType->kind == TYPE_ARRAY);
  assert(emptyType->as.array.elementType == NULL);
  typchkTypeEnvDestroy(env);
}

static void test_array_heterogeneous_elements_error(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = [1, \"two\"];");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_index_non_array_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x = 5; var y = x[0];");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_if_expression(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("fun test(a: bool): string = if (a) \"yes\" else \"no\";");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_if_expression_missing_else_with_non_unit_branch_errors(void) {
  typchkResetError();
  // No else -- implicit else is unit, "yes" is string, mismatch.
  TypeEnv *env = checkProgram("fun test(a: bool): string = if (a) \"yes\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_if_statement_with_unit_branches_is_fine(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("fun test(a: bool): unit { if (a) { print \"hi\"; } }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_block_expression(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var result = { var a = 1; var b = 2; a + b };");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("result")) == typeF64());
  typchkTypeEnvDestroy(env);
}

static void test_function_implicit_return_checked_against_declared_type(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("fun sum(a: f64, b: f64): f64 { a + b }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_function_wrong_implicit_return_type_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("fun sum(a: f64, b: f64): string { a + b }");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_return_statement_checked(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("fun sum(a: f64, b: f64): f64 { return a + b; }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_return_wrong_type_errors(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("fun sum(a: f64, b: f64): f64 { return \"oops\"; }");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_recursive_function(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "fun fib(n: f64): f64 { if (n < 2) return n; return fib(n - 1) + "
      "fib(n - 2); }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_lambda_with_explicit_types(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "var add = fun (a: f64, b: f64) { a + b }; var x = add(1, 2);");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeF64());
  typchkTypeEnvDestroy(env);
}

static void test_lambda_contextual_inference(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("var handler: fun (f64) => f64 = fun (x) { x + 1 };");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_lambda_untyped_without_context_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var f = fun (x) { x };");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_while_loop(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "fun countdown(n: f64): unit { while (n > 0) { n = n - 1; } }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_while_condition_not_bool_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("while (\"x\") { print 1; }");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_for_loop_scopes_its_variable(void) {
  typchkResetError();
  TypeEnv *env =
      checkProgram("for (var i = 0; i < 10; i = i + 1) { print i; }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static void test_var_with_annotation_checks_initializer(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x: f64 = \"wrong\";");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_uninitialized_var_with_type_is_fine(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x: f64;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == typeF64());
  typchkTypeEnvDestroy(env);
}

static void test_uninitialized_var_without_type_errors(void) {
  typchkResetError();
  TypeEnv *env = checkProgram("var x;");
  assert(typchkHadError());
  typchkTypeEnvDestroy(env);
  typchkResetError();
}

static void test_unresolved_variable_is_presumed_native_not_an_error(void) {
  typchkResetError();
  // No such Kirby-level declaration anywhere -- presumed native, per
  // typchkInferVariable()'s documented design. A real typo still surfaces, just
  // at runtime ("Undefined variable"), not statically -- there's no way
  // to distinguish the two cases without a native signature to check
  // against.
  TypeEnv *env = checkProgram("var x = bogus;");
  assert(!typchkHadError());
  assert(typchkTypeEnvLookup(env, makeToken("x")) == NULL);
  typchkTypeEnvDestroy(env);
}

static void test_nested_function_and_closure(void) {
  typchkResetError();
  TypeEnv *env = checkProgram(
      "fun outer(): fun () => f64 { var a = 123; fun inner(): f64 { "
      "return a; } return inner; }");
  assert(!typchkHadError());
  typchkTypeEnvDestroy(env);
}

static bool typecheckSource(const char *source) {
  int outCount = 0;
  bool hadParseError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &outCount, &hadParseError, &endLine);
  assert(!hadParseError);
  return typchkCheckProgram(ast, outCount);
}

static void test_program_fully_typed_struct_and_methods(void) {
  typchkResetError();
  bool ok = typecheckSource(
      "struct Point {\n"
      "  pub var x: f64;\n"
      "  pub var y: f64;\n"
      "}\n"
      "impl Point {\n"
      "  pub fun new(x: f64, y: f64): Point = Point { x: x, y: y };\n"
      "  pub fun sum(self): f64 = self.x + self.y;\n"
      "}\n"
      "var p = Point.new(1, 2);\n"
      "print p.sum();\n");
  assert(ok);
}

static void test_program_missing_param_type_fails(void) {
  typchkResetError();
  bool ok = typecheckSource("fun add(a: f64, b): f64 = a + b;");
  assert(!ok);
}

static void test_program_missing_return_type_fails(void) {
  typchkResetError();
  bool ok = typecheckSource("fun add(a: f64, b: f64) = a + b;");
  assert(!ok);
}

static void test_program_missing_struct_field_type_fails(void) {
  typchkResetError();
  bool ok = typecheckSource("struct Point { pub var x: f64; pub var y; }");
  assert(!ok);
}

static void test_program_self_referential_struct(void) {
  typchkResetError();
  bool ok = typecheckSource("struct Node {\n"
                            "  var value: f64;\n"
                            "  var next: Node;\n"
                            "}\n");
  assert(ok);
}

static void test_program_forward_referencing_struct_field(void) {
  typchkResetError();
  bool ok = typecheckSource("struct B { var value: f64; }\n"
                            "struct A { var b: B; }\n");
  assert(ok);
}

static void test_program_multiple_impl_blocks(void) {
  typchkResetError();
  bool ok = typecheckSource("struct Counter {\n"
                            "  var count: f64;\n"
                            "}\n"
                            "impl Counter {\n"
                            "  pub fun new(): Counter = Counter { count: 0 };\n"
                            "}\n"
                            "impl Counter {\n"
                            "  pub fun get(self): f64 = self.count;\n"
                            "}\n"
                            "var c = Counter.new();\n"
                            "print c.get();\n");
  assert(ok);
}

static void test_program_impl_before_struct_declaration(void) {
  typchkResetError();
  bool ok = typecheckSource("impl Point {\n"
                            "  pub fun origin(): Point = Point { x: 0 };\n"
                            "}\n"
                            "struct Point {\n"
                            "  pub var x: f64;\n"
                            "}\n"
                            "var p = Point.origin();\n"
                            "print p.x;\n");
  assert(ok);
}

static void test_program_mutually_recursive_functions(void) {
  typchkResetError();
  bool ok = typecheckSource(
      "fun isEven(n: f64): bool { if (n == 0) return true; return "
      "isOdd(n - 1); }\n"
      "fun isOdd(n: f64): bool { if (n == 0) return false; return "
      "isEven(n - 1); }\n"
      "print isEven(10);\n");
  assert(ok);
}

static void test_program_method_body_type_error_caught(void) {
  typchkResetError();
  bool ok = typecheckSource("struct Point { pub var x: f64; }\n"
                            "impl Point {\n"
                            "  pub fun bad(self): string = self.x;\n"
                            "}\n");
  assert(!ok);
}

static void test_program_body_falling_off_the_end_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(): f64 { print 1; }\n");
  assert(!ok);
}

static void test_program_return_on_only_one_branch_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(c: bool): f64 { if (c) { return 1; } }\n");
  assert(!ok);
}

static void test_program_return_on_both_branches_is_fine(void) {
  typchkResetError();
  bool ok = typecheckSource(
      "fun f(c: bool): f64 { if (c) { return 1; } else { return 2; } }\n");
  assert(ok);
}

static void test_program_unit_body_needs_no_return(void) {
  typchkResetError();
  bool ok = typecheckSource("fun f(): unit { print 1; }\n");
  assert(ok);
}

static void test_program_lambda_falling_off_the_end_errors(void) {
  typchkResetError();
  bool ok =
      typecheckSource("let f: fun () => f64 = fun (): f64 { print 1; };\n");
  assert(!ok);
}

static void test_program_nested_closure_captures_self(void) {
  typchkResetError();
  // A nested *function* (not a lambda) declared inside a method,
  // referencing self.
  bool ok =
      typecheckSource("struct Struct { var value: f64; }\n"
                      "impl Struct {\n"
                      "  pub fun printSelf(self): fun () => f64 {\n"
                      "    fun innerClosure(): f64 { return self.value; }\n"
                      "    return innerClosure;\n"
                      "  }\n"
                      "}\n");
  assert(ok);
}

static void test_type_alias_chained(void) {
  typchkResetError();
  bool ok = typecheckSource("type A = B;\n"
                            "type B = f64\n;"
                            "let value: A = 7;\n"
                            "print value;\n");
  assert(ok);
}

static void test_type_alias_cycle_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("type A = B;\n"
                            "type B = A;\n"
                            "let value: A = 1;\n");
  assert(!ok);
}

static void test_type_alias_to_struct(void) {
  typchkResetError();
  bool ok = typecheckSource("type Coord = Point;\n"
                            "struct Point {\n"
                            "  pub var x: f64;\n"
                            "}\n"
                            "impl Point {\n"
                            "  pub fun new(x: f64): Coord = Point { x: x };\n"
                            "}\n"
                            "let p: Coord = Point.new(3);\n"
                            "print p.x;\n");
  assert(ok);
}

static void test_type_alias_used_as_type(void) {
  typchkResetError();
  bool ok = typecheckSource("type Number = f64;\n"
                            "type Text = string;\n"
                            "let count: Number = 42;\n"
                            "let name: Text = \"Kirby\";\n"
                            "fun add(a: Number, b: Number): Number = a + b;\n"
                            "print count;\n"
                            "print name;\n"
                            "print add(1, 2);\n");
  assert(ok);
}

static void test_type_alias_wrong_type_errors(void) {
  typchkResetError();
  bool ok = typecheckSource("type Number = f64;\n"
                            "let count: Number = \"not a number\";\n");
  assert(!ok);
}

int main(void) {
  test_scope_declare_and_lookup();
  test_scope_shadowing();
  test_struct_and_function_registries();
  test_resolve_primitives();
  test_resolve_registered_struct();
  test_resolve_unknown_name_errors();
  test_resolve_generic_type_errors();
  test_resolve_function_type();
  test_resolve_function_type_propagates_inner_error();

  test_literals();
  test_binary_arithmetic_and_concat();
  test_binary_plus_mismatch_errors();
  test_binary_arithmetic_requires_f64();
  test_comparisons();
  test_equality_requires_same_type();
  test_ordering_requires_f64();
  test_unary();
  test_negate_requires_f64();
  test_and_or_produce_bool();
  test_and_or_non_bool_operand_errors();
  test_and_or_mismatched_type_errors();
  test_nullish_result_comes_from_fallback();
  test_function_call_checked();
  test_function_call_wrong_arg_type_errors();
  test_function_call_wrong_arity_errors();
  test_native_call_is_unchecked();
  test_struct_instance_field_and_method_access();
  test_struct_static_method_access();
  test_local_variable_shadows_struct_name_for_get();
  test_struct_unknown_field_errors();
  test_struct_init();
  test_struct_init_wrong_field_type_errors();
  test_self_type();
  test_self_outside_method_errors();
  test_array_literal_and_index();
  test_array_heterogeneous_elements_error();
  test_index_non_array_errors();
  test_if_expression();
  test_if_expression_missing_else_with_non_unit_branch_errors();
  test_if_statement_with_unit_branches_is_fine();
  test_block_expression();
  test_function_implicit_return_checked_against_declared_type();
  test_function_wrong_implicit_return_type_errors();
  test_return_statement_checked();
  test_return_wrong_type_errors();
  test_recursive_function();
  test_lambda_with_explicit_types();
  test_lambda_contextual_inference();
  test_lambda_untyped_without_context_errors();
  test_while_loop();
  test_while_condition_not_bool_errors();
  test_for_loop_scopes_its_variable();
  test_var_with_annotation_checks_initializer();
  test_uninitialized_var_with_type_is_fine();
  test_uninitialized_var_without_type_errors();
  test_unresolved_variable_is_presumed_native_not_an_error();
  test_nested_function_and_closure();

  test_program_fully_typed_struct_and_methods();
  test_program_missing_param_type_fails();
  test_program_missing_return_type_fails();
  test_program_missing_struct_field_type_fails();
  test_program_self_referential_struct();
  test_program_forward_referencing_struct_field();
  test_program_multiple_impl_blocks();
  test_program_impl_before_struct_declaration();
  test_program_mutually_recursive_functions();
  test_program_method_body_type_error_caught();
  test_program_nested_closure_captures_self();

  test_type_alias_chained();
  test_type_alias_cycle_errors();
  test_type_alias_to_struct();
  test_type_alias_used_as_type();
  test_type_alias_wrong_type_errors();
  test_program_body_falling_off_the_end_errors();
  test_program_return_on_only_one_branch_errors();
  test_program_return_on_both_branches_is_fine();
  test_program_unit_body_needs_no_return();
  test_program_lambda_falling_off_the_end_errors();

  typesFreeAll();
  astFreeAll();

  return 0;
}
