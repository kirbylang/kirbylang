#include <stdbool.h>

#include "native_signatures.h"
#include "token.h"

const NativeSignature nativeSignatures[] = {
    {"@clock", {0}, 0, NATIVE_F64},
    {"@version", {0}, 0, NATIVE_STRING},
    {"@exit", {NATIVE_F64}, 1, NATIVE_UNIT},
    {"@rand", {0}, 0, NATIVE_F64},
    {"@rand01", {0}, 0, NATIVE_F64},
    {"@randBetween", {NATIVE_F64, NATIVE_F64}, 2, NATIVE_F64},
    {"@ceil", {NATIVE_F64}, 1, NATIVE_F64},
    {"@readFileToString", {NATIVE_STRING}, 1, NATIVE_STRING},
    {"@writeStringToFile", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_UNIT},
    {"@numberToString", {NATIVE_F64}, 1, NATIVE_STRING},
    {"@boolToString", {NATIVE_BOOL}, 1, NATIVE_STRING},
    {"@fileExists", {NATIVE_STRING}, 1, NATIVE_BOOL},
    {"@getenv", {NATIVE_STRING}, 1, NATIVE_STRING},
    {"@setenv", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_UNIT},
    {"@argc", {0}, 0, NATIVE_F64},
    {"@parseNumber", {NATIVE_STRING}, 1, NATIVE_F64},
    {"@strIsEmpty", {NATIVE_STRING}, 1, NATIVE_BOOL},
    {"@floor", {NATIVE_F64}, 1, NATIVE_F64},
    {"@round", {NATIVE_F64}, 1, NATIVE_F64},
    {"@trunc", {NATIVE_F64}, 1, NATIVE_F64},
    {"@abs", {NATIVE_F64}, 1, NATIVE_F64},
    {"@sqrt", {NATIVE_F64}, 1, NATIVE_F64},
    {"@pow", {NATIVE_F64, NATIVE_F64}, 2, NATIVE_F64},
    {"@min", {NATIVE_F64, NATIVE_F64}, 2, NATIVE_F64},
    {"@max", {NATIVE_F64, NATIVE_F64}, 2, NATIVE_F64},
    {"@assert", {NATIVE_BOOL, NATIVE_STRING}, 2, NATIVE_UNIT},
    {"@panic", {NATIVE_STRING}, 1, NATIVE_UNIT},
    {"@strContains", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_BOOL},
    {"@strStartsWith", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_BOOL},
    {"@strEndsWith", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_BOOL},
    {"@strTrim", {NATIVE_STRING}, 1, NATIVE_STRING},
    {"@strToUpper", {NATIVE_STRING}, 1, NATIVE_STRING},
    {"@strToLower", {NATIVE_STRING}, 1, NATIVE_STRING},
    {"@strRepeat", {NATIVE_STRING, NATIVE_F64}, 2, NATIVE_STRING},
    {"@strSplit", {NATIVE_STRING, NATIVE_STRING}, 2, NATIVE_LIST},
    {"@now", {0}, 0, NATIVE_F64},
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

static Type *primitiveType(NativePrimitive primitive) {
  switch (primitive) {
  case NATIVE_UNIT:
    return typeUnit();
  case NATIVE_BOOL:
    return typeBool();
  case NATIVE_STRING:
    return typeString();
  case NATIVE_F64:
    return typeF64();
  case NATIVE_LIST:
    return typeArray(NULL);
  }

  return typeUnit();
}

// Natives are global functions as far as the checker is concerned. They are
// registered before any user code, so a user declaration of the same name
// shadows them.
void defineAllNativeSignatures(TypeEnv *env) {
  for (int i = 0; i < nativeSignatureCount; i++) {
    const NativeSignature *signature = &nativeSignatures[i];

    Type **paramTypes = NULL;

    if (signature->paramCount > 0) {
      paramTypes =
          (Type **)typesAllocRaw(signature->paramCount * sizeof(Type *));

      for (int param = 0; param < signature->paramCount; param++) {
        paramTypes[param] = primitiveType(signature->paramTypes[param]);
      }
    }

    Type *type = typeFunction(paramTypes, signature->paramCount,
                              primitiveType(signature->returnType));

    typchkTypeEnvRegisterFunction(env, tokenFromCString(signature->name), type);
  }
}
