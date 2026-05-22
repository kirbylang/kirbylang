#include "sys/stat.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asserts.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "version.h"
#include "vm.h"

static ObjString *readFile(VM *vm, const char *path) {
  FILE *file = fopen(path, "rb");

  if (!file) {
    runtimeError(vm, "File does not exist: '%s'", path);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    return NULL;
  }

  rewind(file); // Go back to start

  // Allocate buffer (+1 for null terminator)
  char *buffer = (char *)malloc(size + 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  size_t bytesRead = fread(buffer, 1, size, file);
  if (bytesRead != (size_t)size) {
    free(buffer);
    fclose(file);
    return NULL;
  }

  buffer[size] = '\0'; // Null-terminate so it's a valid C string

  fclose(file);

  return takeString(buffer, size);
}

static void writeFile(VM *vm, const char *path, const char *text) {
  FILE *file = fopen(path, "wb");

  if (!file) {
    runtimeError(vm, "File does not exist: '%s'", path);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  fprintf(file, "%s", text);

  return;
}

void defineNative(VM *vm, const char *name, NativeFn function) {
  pushOnStack(OBJ_VAL(copyString(name, (int)strlen(name))));
  pushOnStack(OBJ_VAL(newNative(function)));
  tableSet(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  popFromStack();
  popFromStack();
}

static Value clockNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "clock", 0, argCount);

  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value versionNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "__version__", 0, argCount);

  return OBJ_VAL(copyString(CLOX_VERSION, CLOX_VERSION_len));
}

static Value exitNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "exit", 1, argCount);
  assertArgIsNumber(vm, "exit", args, 0);

  double exitCode = args[0].as.number;

  assertPositiveNumber(vm, "exit", exitCode, 0);

  exit(exitCode);

  return NIL_VAL;
}

static Value randNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "rand", 0, argCount);

  return NUMBER_VAL((double)rand());
}

static Value rand01Native(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "rand01", 0, argCount);

  return NUMBER_VAL((double)rand() / (double)RAND_MAX);
}

static Value randBetweenNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "randBetween", 2, argCount);
  assertArgIsNumber(vm, "randBetween", args, 0);
  assertArgIsNumber(vm, "randBetween", args, 1);

  double min = AS_NUMBER(args[0]);
  double max = AS_NUMBER(args[1]);

  if (min > max) {
    raiseError(vm, "randomRange(min, max) requires min <= max");
  }

  double r = (double)rand() / (double)RAND_MAX; // [0, 1]
  double result = min + r * (max - min);        // [min, max)

  return NUMBER_VAL(ceil(result));
}

static Value ceilNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "ceil", 1, argCount);
  assertArgIsNumber(vm, "ceil", args, 0);

  Value value = args[0];

  return NUMBER_VAL(ceil(AS_NUMBER(value)));
}

Value fileExists(char *filename) {
  struct stat buffer;
  return BOOL_VAL((stat(filename, &buffer) == 0));
}

static Value fileExistsNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "fileExists", 1, argCount);
  assertArgIsString(vm, "fileExists", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);

  return fileExists(path->chars);
}

static Value readFileToStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "readFileToString", 1, argCount);
  assertArgIsString(vm, "readFileToString", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);
  ObjString *contents = readFile(vm, path->chars);

  return OBJ_VAL(contents);
}

static Value writeStringToFileNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "writeStringToFile", 2, argCount);
  assertArgIsString(vm, "writeStringToFile", args, 0);
  assertArgIsString(vm, "writeStringToFile", args, 1);

  Value path_arg = args[0];
  Value text_arg = args[1];

  ObjString *path = AS_STRING(path_arg);
  char *text = AS_CSTRING(text_arg);

  writeFile(vm, path->chars, text);

  return NIL_VAL;
}

static Value getEnvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "getenv", 1, argCount);
  assertArgIsString(vm, "getenv", args, 0);

  Value name = args[0];

  char *name_str = AS_CSTRING(name);

  char *env_value = getenv(name_str);

  if (env_value == NULL) {
    runtimeError(vm, "Environment variable not found: '%s'", name_str);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  return OBJ_VAL(copyString(env_value, (int)strlen(env_value)));
}

static Value setEnvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "setenv", 2, argCount);
  assertArgIsString(vm, "setenv", args, 0);
  assertArgIsString(vm, "setenv", args, 1);

  ObjString *name = AS_STRING(args[0]);
  ObjString *value = AS_STRING(args[1]);

  // overwrite = 1 means always replace existing value
  int result = setenv(name->chars, value->chars, 1);

  if (result != 0) {
    runtimeError(vm, "setenv unable to set environment variable: '%s'",
                 name->chars);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  return NIL_VAL; // or you could return true if you prefer
}

static Value lenNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "len", 1, argCount);

  Value name = args[0];

  if (IS_STRING(name)) {
    int length = AS_STRING(name)->length;

    return NUMBER_VAL(length);
  } else if (IS_ARRAY(name)) {
    ObjArray *array = AS_ARRAY(name);
    int length = array->count;
    return NUMBER_VAL(length);
  } else {
    runtimeError(vm,
                 "function len expects argument 1 to be a string or array.");
    exit(70);
  }
}

static Value typeofNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "typeof", 1, argCount);

  Value value = args[0];

  // Longest value/object type is function (8 chars) + null terminator
  char *buffer = malloc(9);

  if (buffer == NULL) {
    raiseError(vm, "typeof(value) unable to allocate string for type string.");
  }

  valueTypeToString(value, buffer, 9);

  return OBJ_VAL(takeString(buffer, strlen(buffer)));
}

static bool isHelper(Value value, char *type) {
  char *buffer = malloc(9);

  if (buffer == NULL) {
    return false;
  }

  valueTypeToString(value, buffer, 9);

  // free(buffer);

  bool equals = strcmp(type, buffer) == 0;

  return equals;
}

static Value isNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "is", 2, argCount);
  assertArgIsString(vm, "is", args, 1);

  Value value = args[0];
  Value type = args[1];

  bool equals = isHelper(value, AS_STRING(type)->chars);

  return BOOL_VAL(equals);
}

static Value isNumberNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "isNumber", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "number");

  return BOOL_VAL(equals);
}

static Value isFunctionNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "isFunction", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "function");

  return BOOL_VAL(equals);
}

static Value isBoolNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "isBool", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "bool");

  return BOOL_VAL(equals);
}

static Value isStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "isString", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "string");

  return BOOL_VAL(equals);
}

static Value isNilNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "isNil", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "nil");

  return BOOL_VAL(equals);
}

static Value instanceOfNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "instanceOf", 2, argCount);
  assertArgIsClass(vm, "instanceOf", args, 1);

  Value value = args[0];

  if (!IS_INSTANCE(value)) {
    return BOOL_VAL(false);
  }

  Value klass = args[1];

  ObjInstance *instance = AS_INSTANCE(value);
  ObjClass *klass2 = AS_CLASS(klass);

  return BOOL_VAL(instance->klass == klass2);
}

static Value arrPushNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrPush", 2, argCount);
  assertArgIsArray(vm, "arrPush", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);
  Value value = args[1];

  writeValueToArrayObj(array, value);

  return NIL_VAL;
}

static Value arrPopNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrPop", 1, argCount);
  assertArgIsArray(vm, "arrPop", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  if (array->count == 0) {
    runtimeError(vm, "Cannot pop empty array.");
    exit(70);
  }

  Value value = array->values[array->count - 1];

  array->count--;

  return value;
}

static Value arrInsertNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrInsert", 3, argCount);
  assertArgIsArray(vm, "arrInsert", args, 0);
  assertArgIsNumber(vm, "arrInsert", args, 1);

  ObjArray *array = AS_ARRAY(args[0]);
  int index = (int)AS_NUMBER(args[1]);
  Value value = args[2];

  assertIsInArrayBounds(vm, array, index);

  writeValueToArrayObj(array, NIL_VAL);

  for (int i = array->count - 1; i > index; i--) {
    array->values[i] = array->values[i - 1];
  }

  array->values[index] = value;

  return NIL_VAL;
}

static Value arrRemoveNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrRemove", 2, argCount);
  assertArgIsArray(vm, "arrRemove", args, 0);
  assertArgIsNumber(vm, "arrRemove", args, 1);

  ObjArray *array = AS_ARRAY(args[0]);
  int index = (int)AS_NUMBER(args[1]);

  assertIsInArrayBounds(vm, array, index);

  Value removed = array->values[index];

  for (int i = index; i < array->count - 1; i++) {
    array->values[i] = array->values[i + 1];
  }

  array->count--;

  return removed;
}

static Value arrClearNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrClear", 1, argCount);
  assertArgIsArray(vm, "arrClear", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  array->count = 0;

  return NIL_VAL;
}

static Value arrContainsNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrContains", 2, argCount);
  assertArgIsArray(vm, "arrContains", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);
  Value value = args[1];

  for (int i = 0; i < array->count; i++) {
    if (valuesEqual(array->values[i], value)) {
      return BOOL_VAL(true);
    }
  }

  return BOOL_VAL(false);
}

static Value arrCopyNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrayCopy", 1, argCount);
  assertArgIsArray(vm, "arrayCopy", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);
  ObjArray *result = newArray();

  pushOnStack(OBJ_VAL(result)); // Protect from GC

  for (int i = 0; i < array->count; i++) {
    writeValueToArrayObj(result, array->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(result);
}

static Value arrIsEmptyNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrIsEmpty", 1, argCount);
  assertArgIsArray(vm, "arrIsEmpty", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  bool isEmpty = array->count == 0;

  return BOOL_VAL(isEmpty);
}

static Value strIsEmptyNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "strIsEmpty", 1, argCount);
  assertArgIsString(vm, "strIsEmpty", args, 0);

  ObjString *string = AS_STRING(args[0]);

  bool isEmpty = string->length == 0;

  return BOOL_VAL(isEmpty);
}

static Value arrEqualNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrEqual", 2, argCount);
  assertArgIsArray(vm, "arrEqual", args, 0);
  assertArgIsArray(vm, "arrEqual", args, 1);

  ObjArray *array_a = AS_ARRAY(args[0]);
  ObjArray *array_b = AS_ARRAY(args[1]);

  if (array_a->count != array_b->count) {
    return BOOL_VAL(false);
  }

  bool areEqual = true;

  for (int i = 0; i < array_a->count; i++) {
    bool i_equal = valuesEqual(array_a->values[i], array_b->values[i]);

    if (!i_equal) {
      areEqual = false;
      break;
    }
  }

  return BOOL_VAL(areEqual);
}

static Value arrSliceNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrSlice", 3, argCount);
  assertArgIsArray(vm, "arrSlice", args, 0);
  assertArgIsNumber(vm, "arrSlice", args, 1);
  assertArgIsNumber(vm, "arrSlice", args, 2);

  ObjArray *array = AS_ARRAY(args[0]);

  double start = AS_NUMBER(args[1]);
  assertPositiveNumber(vm, "arrSlice", start, 1);
  assertIsInArrayBounds(vm, array, start);

  double end = AS_NUMBER(args[2]);
  assertPositiveNumber(vm, "arrSlice", end, 2);
  assertIsInArrayBounds(vm, array, end);

  if (start >= end) {
    runtimeError(
        vm, "function arrSlice expects argument 1 to be less than argument 2.");
    exit(70);
  }

  ObjArray *result = newArray();

  pushOnStack(OBJ_VAL(result));

  for (int i = start; i < end; i++) {
    writeValueToArrayObj(result, array->values[i]);
  }

  popFromStack();

  return OBJ_VAL(result);
}

static Value arrConcatNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrConcat", 2, argCount);
  assertArgIsArray(vm, "arrConcat", args, 0);
  assertArgIsArray(vm, "arrConcat", args, 1);

  ObjArray *array_a = AS_ARRAY(args[0]);
  ObjArray *array_b = AS_ARRAY(args[1]);

  ObjArray *new_array = newArray();

  pushOnStack(OBJ_VAL(new_array)); // Protect from GC

  for (int i = 0; i < array_a->count; i++) {
    writeValueToArrayObj(new_array, array_a->values[i]);
  }

  for (int i = 0; i < array_b->count; i++) {
    writeValueToArrayObj(new_array, array_b->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(new_array);
}

static Value arrReverseNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "arrReverse", 1, argCount);
  assertArgIsArray(vm, "arrReverse", args, 0);

  ObjArray *array_a = AS_ARRAY(args[0]);

  ObjArray *new_array = newArray();

  pushOnStack(OBJ_VAL(new_array)); // Protect from GC

  for (int i = array_a->count - 1; i >= 0; i--) {
    writeValueToArrayObj(new_array, array_a->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(new_array);
}

static Value stdinNative(VM *vm, int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(vm, "stdin", 1, argCount);
  }

  if (argCount == 1) {
    if (!IS_STRING(args[0])) {
      runtimeError(vm, "input() argument must be a string.");
      return NIL_VAL;
    }
    ObjString *prompt = AS_STRING(args[0]);
    fwrite(prompt->chars, sizeof(char), prompt->length, stdout);
    fflush(stdout);
  }

  const size_t MAX_INPUT = 1024 * 1024 * 5; // 5 MB cap

  size_t capacity = 64;
  size_t length = 0;
  char *buffer = ALLOCATE(char, capacity);

  int c;
  while ((c = getchar()) != EOF) {
    // Handle Windows CRLF: skip '\r'
    if (c == '\r')
      continue;

    if (length + 1 >= capacity) {
      size_t newCapacity = capacity * 2;

      if (newCapacity > MAX_INPUT) {
        newCapacity = MAX_INPUT;
      }

      if (capacity == MAX_INPUT) {
        FREE_ARRAY(char, buffer, capacity);
        runtimeError(vm, "stdin() exceeded maximum length.");
        return NIL_VAL;
      }

      buffer = GROW_ARRAY(char, buffer, capacity, newCapacity);
      capacity = newCapacity;
    }

    buffer[length++] = (char)c;
  }

  if (c == EOF && length == 0) {
    FREE_ARRAY(char, buffer, capacity);
    return NIL_VAL;
  }

  buffer[length] = '\0';

  return OBJ_VAL(takeString(buffer, length));
}

static Value promptNative(VM *vm, int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(vm, "prompt", 1, argCount);
    assertArgIsString(vm, "prompt", args, 0);
  }

  if (argCount == 1) {
    if (!IS_STRING(args[0])) {
      runtimeError(vm, "prompt() argument must be a string.");
      return NIL_VAL;
    }
    ObjString *prompt = AS_STRING(args[0]);
    fwrite(prompt->chars, sizeof(char), prompt->length, stdout);
    fflush(stdout);
  }

  const size_t MAX_INPUT = 1024 * 1024; // 1 MB cap

  size_t capacity = 64;
  size_t length = 0;
  char *buffer = ALLOCATE(char, capacity);

  int c;
  while ((c = getchar()) != '\n' && c != EOF) {
    // Handle Windows CRLF: skip '\r'
    if (c == '\r')
      continue;

    if (length + 1 >= capacity) {
      size_t newCapacity = capacity * 2;

      if (newCapacity > MAX_INPUT) {
        newCapacity = MAX_INPUT;
      }

      if (capacity == MAX_INPUT) {
        FREE_ARRAY(char, buffer, capacity);
        runtimeError(vm, "input() exceeded maximum length.");
        return NIL_VAL;
      }

      buffer = GROW_ARRAY(char, buffer, capacity, newCapacity);
      capacity = newCapacity;
    }

    buffer[length++] = (char)c;
  }

  if (c == EOF && length == 0) {
    FREE_ARRAY(char, buffer, capacity);
    return NIL_VAL;
  }

  buffer[length] = '\0';

  return OBJ_VAL(takeString(buffer, length));
}

static Value argvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "argv", 1, argCount);
  assertArgIsNumber(vm, "argv", args, 0);

  Value index = args[0];

  int idx = (int)AS_NUMBER(index);
  if (idx < 0 || idx >= vm->argc) {
    return NIL_VAL;
  }

  const char *c_arg = vm->argv[idx];
  ObjString *cloxStr = copyString(c_arg, (int)strlen(c_arg));

  return OBJ_VAL(cloxStr);
}

static Value argcNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "argc", 0, argCount);

  (void)args;

  int argc = vm->argc;

  return NUMBER_VAL(argc);
}

static Value parseNumberNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "parseNumber", 1, argCount);
  assertArgIsString(vm, "parseNumber", args, 0);

  Value value = args[0];

  char *valueString = AS_CSTRING(value);
  int valueInt = strtol(valueString, NULL, 10);

  return NUMBER_VAL(valueInt);
}

static Value numberToStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "numberToString", 1, argCount);
  assertArgIsNumber(vm, "numberToString", args, 0);

  Value value = args[0];

  char buffer[32];

  int length = snprintf(buffer, sizeof(buffer), "%.15g", value.as.number);

  return OBJ_VAL(copyString(buffer, length));
}

void defineAllNatives(VM *vm) {
  defineNative(vm, "clock", clockNative);
  defineNative(vm, "__version__", versionNative);
  defineNative(vm, "exit", exitNative);
  defineNative(vm, "rand", randNative);
  defineNative(vm, "rand01", rand01Native);
  defineNative(vm, "randBetween", randBetweenNative);
  defineNative(vm, "ceil", ceilNative);
  defineNative(vm, "readFileToString", readFileToStringNative);
  defineNative(vm, "writeStringToFile", writeStringToFileNative);
  defineNative(vm, "numberToString", numberToStringNative);
  defineNative(vm, "fileExists", fileExistsNative);
  defineNative(vm, "getenv", getEnvNative);
  defineNative(vm, "setenv", setEnvNative);
  defineNative(vm, "len", lenNative);
  defineNative(vm, "typeof", typeofNative);
  defineNative(vm, "argv", argvNative);
  defineNative(vm, "argc", argcNative);
  defineNative(vm, "parseNumber", parseNumberNative);
  defineNative(vm, "instanceOf", instanceOfNative);
  defineNative(vm, "prompt", promptNative);
  defineNative(vm, "stdin", stdinNative);
  defineNative(vm, "arrPush", arrPushNative);
  defineNative(vm, "arrPop", arrPopNative);
  defineNative(vm, "arrInsert", arrInsertNative);
  defineNative(vm, "arrRemove", arrRemoveNative);
  defineNative(vm, "arrClear", arrClearNative);
  defineNative(vm, "arrContains", arrContainsNative);
  defineNative(vm, "arrCopy", arrCopyNative);
  defineNative(vm, "arrIsEmpty", arrIsEmptyNative);
  defineNative(vm, "arrEqual", arrEqualNative);
  defineNative(vm, "arrSlice", arrSliceNative);
  defineNative(vm, "arrConcat", arrConcatNative);
  defineNative(vm, "arrReverse", arrReverseNative);
  defineNative(vm, "is", isNative);
  defineNative(vm, "isNumber", isNumberNative);
  defineNative(vm, "isFunction", isFunctionNative);
  defineNative(vm, "isBool", isBoolNative);
  defineNative(vm, "isString", isStringNative);
  defineNative(vm, "isNil", isNilNative);
  defineNative(vm, "strIsEmpty", strIsEmptyNative);
}
