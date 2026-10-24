#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_signatures.h"
#include "parser.h"
#include "token.h"

const NativeSignature nativeSignatures[] = {
    {"@clock", "fun () => f64"},
    {"@version", "fun () => string"},
    {"@exit", "fun (i64) => unit"},
    {"@rand", "fun () => f64"},
    {"@rand01", "fun () => f64"},
    {"@randBetween", "fun (i64, i64) => i64"},
    {"@ceil", "fun (f64) => f64"},
    {"@readFileToString", "fun (string) => string"},
    {"@writeStringToFile", "fun (string, string) => unit"},
    {"@numberToString", "fun (number) => string"},
    {"@boolToString", "fun (bool) => string"},
    {"@fileExists", "fun (string) => bool"},
    {"@getenv", "fun (string) => string"},
    {"@setenv", "fun (string, string) => unit"},
    {"@argc", "fun () => i64"},
    {"@parseInt", "fun (string) => i64"},
    {"@parseFloat", "fun (string) => f64"},
    {"@strIsEmpty", "fun (string) => bool"},
    {"@floor", "fun (f64) => f64"},
    {"@round", "fun (f64) => f64"},
    {"@trunc", "fun (f64) => f64"},
    {"@abs", "fun (f64) => f64"},
    {"@sqrt", "fun (f64) => f64"},
    {"@pow", "fun (f64, f64) => f64"},
    {"@min", "fun (f64, f64) => f64"},
    {"@max", "fun (f64, f64) => f64"},
    {"@assert", "fun (bool, string) => unit"},
    {"@panic", "fun (string) => unit"},
    {"@strContains", "fun (string, string) => bool"},
    {"@strStartsWith", "fun (string, string) => bool"},
    {"@strEndsWith", "fun (string, string) => bool"},
    {"@strTrim", "fun (string) => string"},
    {"@strToUpper", "fun (string) => string"},
    {"@strToLower", "fun (string) => string"},
    {"@strRepeat", "fun (string, i64) => string"},
    {"@strLen", "fun (string) => i64"},
    {"@strIndexOf", "fun (string, string) => i64"},
    {"@strSlice", "fun (string, integer, integer) => string"},
    {"@strReplace", "fun (string, string, string) => string"},
    {"@strReplaceAll", "fun (string, string, string) => string"},
    {"@strSplit", "fun (string, string) => [string]"},
    {"@strToBytes", "fun (string) => [u8]"},
    {"@strConcat", "fun ([string]) => string"},
    {"@now", "fun () => i64"},
    {"@listJoin", "fun ([string], string) => string"},
    {"@listLen", "fun ([T]) => i64"},
    {"@listIsEmpty", "fun ([T]) => bool"},
    {"@listPush", "fun ([T], T) => unit"},
    {"@listPop", "fun ([T]) => T"},
    {"@listInsert", "fun ([T], integer, T) => unit"},
    {"@listRemove", "fun ([T], integer) => T"},
    {"@listClear", "fun ([T]) => unit"},
    {"@listContains", "fun ([T], T) => bool"},
    {"@listEqual", "fun ([T], [T]) => bool"},
    {"@listCopy", "fun ([T]) => [T]"},
    {"@listReverse", "fun ([T]) => [T]"},
    {"@listSlice", "fun ([T], integer, integer) => [T]"},
    {"@listConcat", "fun ([T], [T]) => [T]"},
    {"@arrLen", "fun ([T; N]) => i64"},
    {"@arrContains", "fun ([T; N], T) => bool"},
    {"@arrEqual", "fun ([T; N], [T; N]) => bool"},
    {"@arrCopy", "fun ([T; N]) => [T; N]"},
    {"@arrReverse", "fun ([T; N]) => [T; N]"},
    {"@arrJoin", "fun ([string; N], string) => string"},
    {"@arrSlice", "fun ([T; N], integer, integer) => [T]"},
    {"@arrConcat", "fun ([T; N], [T; M]) => [T]"},
    {"@arrToList", "fun ([T; N]) => [T]"},
};

const int nativeSignatureCount =
    (int)(sizeof(nativeSignatures) / sizeof(nativeSignatures[0]));

static bool _isNonNegative(double value) { return value >= 0; }
static bool _isLessThanOrEqualTo255(double value) { return value <= 255; }

const NativeArgConstraint nativeArgConstraints[] = {
    {"@sqrt", 0, _isNonNegative, "a non-negative number"},
    {"@exit", 0, _isNonNegative, "a non-negative number"},
    {"@exit", 0, _isLessThanOrEqualTo255, "a number less than or equal to 255"},
};

const int nativeArgConstraintCount =
    (int)(sizeof(nativeArgConstraints) / sizeof(nativeArgConstraints[0]));

Type *parseNativeSignature(TypeEnv *env, const char *signature) {
  bool hadError = false;
  AstNode *node = parseTypeText(signature, &hadError);
  if (node == NULL)
    return NULL;

  return typchkResolveSignatureType(env, node);
}

// Natives are global functions as far as the checker is concerned. They are
// registered before any user code, so a user declaration of the same name
// shadows them.
void defineAllNativeSignatures(TypeEnv *env) {
  for (int i = 0; i < nativeSignatureCount; i++) {
    const NativeSignature *native = &nativeSignatures[i];
    Type *type = parseNativeSignature(env, native->signature);

    if (type == NULL) {
      fprintf(stderr, "Invalid signature for native '%s': %s\n", native->name,
              native->signature);
      exit(1);
    }

    typchkTypeEnvRegisterFunction(env, tokenFromCString(native->name), type);
  }
}
