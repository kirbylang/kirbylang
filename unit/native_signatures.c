#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/native.h"
#include "../src/native_signatures.h"
#include "../src/typecheck.h"
#include "../src/types.h"

// Natives whose type needs generics. Phase 5.
static const char *awaitingGenerics[] = {
    "len",        "typeof",     "is",        "isNumber",   "isFunction",
    "isBool",     "isString",   "isNil",     "instanceOf", "arrPush",
    "arrPop",     "arrInsert",  "arrRemove", "arrClear",   "arrContains",
    "arrCopy",    "arrIsEmpty", "arrEqual",  "arrSlice",   "arrConcat",
    "arrReverse",
};

// Natives that return nothing on some paths, so their type needs Option[T].
// Phase 6. prompt and stdin also take an optional argument, which the
// language has no way to spell.
static const char *awaitingOptionType[] = {"argv", "prompt", "stdin"};

static bool listContains(const char *const *names, int count,
                         const char *name) {
  for (int i = 0; i < count; i++) {
    if (strcmp(names[i], name) == 0)
      return true;
  }
  return false;
}

static bool isDeferred(const char *name) {
  int genericCount =
      (int)(sizeof(awaitingGenerics) / sizeof(*awaitingGenerics));
  int optionCount =
      (int)(sizeof(awaitingOptionType) / sizeof(*awaitingOptionType));

  return listContains(awaitingGenerics, genericCount, name) ||
         listContains(awaitingOptionType, optionCount, name);
}

static bool hasSignature(const char *name) {
  for (int i = 0; i < nativeSignatureCount; i++) {
    if (strcmp(nativeSignatures[i].name, name) == 0)
      return true;
  }
  return false;
}

static bool isDefinedNative(const char *name) {
  for (int i = 0; i < nativeDefinitionCount; i++) {
    if (strcmp(nativeDefinitions[i].name, name) == 0)
      return true;
  }
  return false;
}

// A native with neither a signature nor a place on a deferred list is an
// undecided native. Failing here is the prompt to decide.
static void test_every_native_is_signed_or_deliberately_deferred(void) {
  for (int i = 0; i < nativeDefinitionCount; i++) {
    const char *name = nativeDefinitions[i].name;

    if (hasSignature(name) == isDeferred(name)) {
      fprintf(stderr, "native '%s' needs a signature or a deferred list\n",
              name);
      assert(false);
    }
  }
}

// A signature for a name the VM never defines would type-check calls to a
// global that doesn't exist.
static void test_every_signature_names_a_real_native(void) {
  for (int i = 0; i < nativeSignatureCount; i++) {
    const char *name = nativeSignatures[i].name;

    if (!isDefinedNative(name)) {
      fprintf(stderr, "signature '%s' has no matching native\n", name);
      assert(false);
    }
  }
}

static Token makeToken(const char *text) {
  Token t;
  t.type = TOKEN_IDENTIFIER;
  t.start = text;
  t.length = (int)strlen(text);
  t.line = 1;
  return t;
}

static void test_signed_natives_resolve_in_a_new_env(void) {
  TypeEnv *env = typchkTypeEnvCreate();

  Type *ceil = typchkTypeEnvLookupFunction(env, makeToken("ceil"));
  assert(ceil != NULL);
  assert(ceil->kind == TYPE_FN);
  assert(ceil->as.function.paramCount == 1);
  assert(ceil->as.function.paramTypes[0] == typeF64());
  assert(ceil->as.function.returnType == typeF64());

  Type *clock = typchkTypeEnvLookupFunction(env, makeToken("clock"));
  assert(clock != NULL);
  assert(clock->as.function.paramCount == 0);
  assert(clock->as.function.returnType == typeF64());

  assert(typchkTypeEnvLookupFunction(env, makeToken("len")) == NULL);

  typchkTypeEnvDestroy(env);
}

static void test_a_user_function_shadows_a_native(void) {
  TypeEnv *env = typchkTypeEnvCreate();

  Type *userCeil = typeFunction(NULL, 0, typeString());
  typchkTypeEnvRegisterFunction(env, makeToken("ceil"), userCeil);

  assert(typchkTypeEnvLookupFunction(env, makeToken("ceil")) == userCeil);

  typchkTypeEnvDestroy(env);
}

int main(void) {
  test_every_native_is_signed_or_deliberately_deferred();
  test_every_signature_names_a_real_native();
  test_signed_natives_resolve_in_a_new_env();
  test_a_user_function_shadows_a_native();

  typesFreeAll();

  printf("native signature tests passed\n");
  return 0;
}
