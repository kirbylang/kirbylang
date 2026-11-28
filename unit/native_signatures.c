#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/native.h"
#include "../src/parser.h"
#include "../src/stdlib_source.h"
#include "../src/typecheck.h"
#include "../src/types.h"

// Natives that take a value of any type. A generic parameter can describe
// them, but they aren't signed yet. The print natives also have their own
// Display check.
static const char *awaitingAnyTypeSignature[] = {
    "@typeof", "@is",    "@isNumber", "@isFunction", "@isBool",   "@isString",
    "@isNil",  "@print", "@println",  "@eprint",     "@eprintln",
};

// Natives that return nothing on some paths, so their type needs Option[T].
// Phase 6. prompt and stdin also take an optional argument, which the
// language has no way to spell.
static const char *awaitingOptionType[] = {"@argv", "@prompt", "@stdin"};

static bool listContains(const char *const *names, int count,
                         const char *name) {
  for (int i = 0; i < count; i++) {
    if (strcmp(names[i], name) == 0)
      return true;
  }
  return false;
}

static bool isDeferred(const char *name) {
  int anyTypeCount = (int)(sizeof(awaitingAnyTypeSignature) /
                           sizeof(*awaitingAnyTypeSignature));
  int optionCount =
      (int)(sizeof(awaitingOptionType) / sizeof(*awaitingOptionType));

  return listContains(awaitingAnyTypeSignature, anyTypeCount, name) ||
         listContains(awaitingOptionType, optionCount, name);
}

// The names the standard library declares with `native`
static const char *declared[256];
static int declaredCount = 0;

static void readDeclaredNatives(void) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parseStdlib(KIRBY_STDLIB, &count, &hadError, &endLine);
  assert(!hadError);

  for (int i = 0; i < count; i++) {
    if (ast[i]->kind != NODE_FUNCTION || !ast[i]->as.function.isNative)
      continue;

    Token name = ast[i]->as.function.name;
    assert(declaredCount < 256);
    char *copy = (char *)calloc((size_t)name.length + 1, 1);
    memcpy(copy, name.start, (size_t)name.length);
    declared[declaredCount++] = copy;
  }

  free(ast);
}

static bool isDeclared(const char *name) {
  return listContains(declared, declaredCount, name);
}

static bool isDefinedNative(const char *name) {
  for (int i = 0; i < nativeDefinitionCount; i++) {
    if (strcmp(nativeDefinitions[i].name, name) == 0)
      return true;
  }
  return false;
}

// A native with neither a declaration nor a place on a deferred list is an
// undecided native. Failing here is the prompt to decide.
static void test_every_native_is_declared_or_deliberately_deferred(void) {
  for (int i = 0; i < nativeDefinitionCount; i++) {
    const char *name = nativeDefinitions[i].name;

    if (isDeclared(name) == isDeferred(name)) {
      fprintf(stderr, "native '%s' needs a declaration or a deferred list\n",
              name);
      assert(false);
    }
  }
}

// A declaration for a name the VM never defines would type-check calls to a
// global that doesn't exist.
static void test_every_declaration_names_a_real_native_once(void) {
  for (int i = 0; i < declaredCount; i++) {
    if (!isDefinedNative(declared[i]) ||
        listContains(declared, i, declared[i])) {
      fprintf(stderr, "declaration '%s' has no matching native, or repeats\n",
              declared[i]);
      assert(false);
    }
  }
}

static void checkStdlibIntoSession(void) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parseStdlib(KIRBY_STDLIB, &count, &hadError, &endLine);
  assert(!hadError);
  assert(typchkCheckStdlib(ast, count));
  free(ast);
}

static void expectNativeType(const char *name, const char *expected) {
  Type *type =
      typchkTypeEnvLookupFunction(typchkSessionEnv(), tokenFromCString(name));
  assert(type != NULL && type->kind == TYPE_FN);
  assert(type->as.function.isNative);

  if (strcmp(typeToString(type), expected) != 0) {
    fprintf(stderr, "type of '%s' is %s\n", name, typeToString(type));
    assert(false);
  }
}

static void test_declared_natives_have_their_types(void) {
  typchkSessionBegin();
  checkStdlibIntoSession();

  expectNativeType("@ceil", "fun (f64) => f64");
  expectNativeType("@clock", "fun () => f64");
  expectNativeType("@strLen", "fun (string) => i64");
  expectNativeType("@listLen", "fun ([T]) => i64");
  expectNativeType("@listPop", "fun ([T]) => T");
  expectNativeType("@strSlice", "fun (string, integer, integer) => string");
  expectNativeType("@arrConcat", "fun ([T; N], [T; M]) => [T]");

  assert(typchkTypeEnvLookupFunction(typchkSessionEnv(),
                                     tokenFromCString("@len")) == NULL);
  assert(typchkTypeEnvLookupFunction(typchkSessionEnv(),
                                     tokenFromCString("@typeof")) == NULL);

  typchkSessionEnd();
}

// A user function named "ceil" (no '@') is a completely different global
// from the native "@ceil" -- the parser rejects '@' in any name a user
// declares, so the two can never collide. Registering one leaves the
// other's type untouched.
static void test_a_user_function_does_not_collide_with_a_native(void) {
  typchkSessionBegin();
  checkStdlibIntoSession();
  TypeEnv *env = typchkSessionEnv();

  Type *userCeil = typeFunction(NULL, 0, typeString());
  typchkTypeEnvRegisterFunction(env, tokenFromCString("ceil"), userCeil);

  assert(typchkTypeEnvLookupFunction(env, tokenFromCString("ceil")) ==
         userCeil);

  Type *nativeCeil =
      typchkTypeEnvLookupFunction(env, tokenFromCString("@ceil"));
  assert(nativeCeil != NULL);
  assert(nativeCeil != userCeil);
  assert(nativeCeil->as.function.paramCount == 1);
  assert(nativeCeil->as.function.paramTypes[0] == typeF64());
  assert(nativeCeil->as.function.returnType == typeF64());

  typchkSessionEnd();
}

int main(void) {
  readDeclaredNatives();

  test_every_native_is_declared_or_deliberately_deferred();
  test_every_declaration_names_a_real_native_once();
  test_declared_natives_have_their_types();
  test_a_user_function_does_not_collide_with_a_native();

  typesFreeAll();
  astFreeAll();

  printf("native signature tests passed\n");
  return 0;
}
