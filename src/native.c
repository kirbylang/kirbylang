#include "sys/stat.h"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asserts.h"
#include "gc.h"
#include "native.h"
#include "native_signatures.h"
#include "object.h"
#include "version.h"
#include "vm.h"

static ObjString *readFile(VM *vm, const char *path) {
  FILE *file = fopen(path, "rb");

  if (!file) {
    runtimeError(vm, "File does not exist: '%s'", path);
    exit(EXIT_CODE_RUNTIME_ERR); // INTERPRET_RUNTIME_ERROR
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

  return takeString(vm->gc, buffer, size);
}

static void writeFile(VM *vm, const char *path, const char *text) {
  FILE *file = fopen(path, "wb");

  if (!file) {
    runtimeError(vm, "File does not exist: '%s'", path);
    exit(EXIT_CODE_RUNTIME_ERR); // INTERPRET_RUNTIME_ERROR
  }

  fprintf(file, "%s", text);

  return;
}

void defineNative(VM *vm, const char *name, NativeFn function) {
  pushOnStack(OBJ_VAL(copyString(vm->gc, name, (int)strlen(name))));
  pushOnStack(OBJ_VAL(newNative(vm->gc, function)));
  tableSet(vm->gc, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  popFromStack();
  popFromStack();
}

static Value clockNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "@clock", 0, argCount);

  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value versionNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "@version", 0, argCount);

  return OBJ_VAL(copyString(vm->gc, KIRBY_VERSION, KIRBY_VERSION_len));
}

static Value exitNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@exit", 1, argCount);
  assertArgIsNumber(vm, "@exit", args, 0);

  double exitCode = args[0].as.number;

  assertGtEq(vm, "@exit", exitCode, 0, 0);

  exit(exitCode);

  return NIL_VAL;
}

static Value randNative(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "@rand", 0, argCount);

  return NUMBER_VAL((double)rand());
}

static Value rand01Native(VM *vm, int argCount, Value *args) {
  (void)args;

  assertArgCount(vm, "@rand01", 0, argCount);

  return NUMBER_VAL((double)rand() / (double)RAND_MAX);
}

static Value randBetweenNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@randBetween", 2, argCount);
  assertArgIsNumber(vm, "@randBetween", args, 0);
  assertArgIsNumber(vm, "@randBetween", args, 1);

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
  assertArgCount(vm, "@ceil", 1, argCount);
  assertArgIsNumber(vm, "@ceil", args, 0);

  Value value = args[0];

  return NUMBER_VAL(ceil(AS_NUMBER(value)));
}

Value fileExists(char *filename) {
  struct stat buffer;
  return BOOL_VAL((stat(filename, &buffer) == 0));
}

static Value fileExistsNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@fileExists", 1, argCount);
  assertArgIsString(vm, "@fileExists", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);

  return fileExists(path->chars);
}

static Value readFileToStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@readFileToString", 1, argCount);
  assertArgIsString(vm, "@readFileToString", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);
  ObjString *contents = readFile(vm, path->chars);

  return OBJ_VAL(contents);
}

static Value writeStringToFileNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@writeStringToFile", 2, argCount);
  assertArgIsString(vm, "@writeStringToFile", args, 0);
  assertArgIsString(vm, "@writeStringToFile", args, 1);

  Value path_arg = args[0];
  Value text_arg = args[1];

  ObjString *path = AS_STRING(path_arg);
  char *text = AS_CSTRING(text_arg);

  writeFile(vm, path->chars, text);

  return NIL_VAL;
}

static Value getEnvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@getenv", 1, argCount);
  assertArgIsString(vm, "@getenv", args, 0);

  Value name = args[0];

  char *name_str = AS_CSTRING(name);

  char *env_value = getenv(name_str);

  if (env_value == NULL) {
    runtimeError(vm, "Environment variable not found: '%s'", name_str);
    exit(EXIT_CODE_RUNTIME_ERR); // INTERPRET_RUNTIME_ERROR
  }

  return OBJ_VAL(copyString(vm->gc, env_value, (int)strlen(env_value)));
}

static Value setEnvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@setenv", 2, argCount);
  assertArgIsString(vm, "@setenv", args, 0);
  assertArgIsString(vm, "@setenv", args, 1);

  ObjString *name = AS_STRING(args[0]);
  ObjString *value = AS_STRING(args[1]);

  // overwrite = 1 means always replace existing value
  int result = setenv(name->chars, value->chars, 1);

  if (result != 0) {
    runtimeError(vm, "setenv unable to set environment variable: '%s'",
                 name->chars);
    exit(EXIT_CODE_RUNTIME_ERR); // INTERPRET_RUNTIME_ERROR
  }

  return NIL_VAL; // or you could return true if you prefer
}

static Value lenNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@len", 1, argCount);

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
    exit(EXIT_CODE_RUNTIME_ERR);
  }
}

static Value typeofNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@typeof", 1, argCount);

  Value value = args[0];

  // Longest value/object type is function (8 chars) + null terminator
  char *buffer = malloc(9);

  if (buffer == NULL) {
    raiseError(vm, "typeof(value) unable to allocate string for type string.");
  }

  valueTypeToString(value, buffer, 9);

  return OBJ_VAL(takeString(vm->gc, buffer, strlen(buffer)));
}

static bool isHelper(Value value, char *type) {
  char *buffer = malloc(9);

  if (buffer == NULL) {
    return false;
  }

  valueTypeToString(value, buffer, 9);

  bool equals = strcmp(type, buffer) == 0;

  free(buffer);

  return equals;
}

static Value isNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@is", 2, argCount);
  assertArgIsString(vm, "@is", args, 1);

  Value value = args[0];
  Value type = args[1];

  bool equals = isHelper(value, AS_STRING(type)->chars);

  return BOOL_VAL(equals);
}

static Value isNumberNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@isNumber", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "number");

  return BOOL_VAL(equals);
}

static Value isFunctionNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@isFunction", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "function");

  return BOOL_VAL(equals);
}

static Value isBoolNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@isBool", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "bool");

  return BOOL_VAL(equals);
}

static Value isStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@isString", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "string");

  return BOOL_VAL(equals);
}

static Value isNilNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@isNil", 1, argCount);

  Value value = args[0];

  bool equals = isHelper(value, "nil");

  return BOOL_VAL(equals);
}

static Value instanceOfNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@instanceOf", 2, argCount);
  assertArgIsStruct(vm, "@instanceOf", args, 1);

  Value value = args[0];

  if (!IS_INSTANCE(value)) {
    return BOOL_VAL(false);
  }

  Value struct1 = args[1];

  ObjInstance *instance = AS_INSTANCE(value);
  ObjStruct *struct2 = AS_STRUCT(struct1);

  return BOOL_VAL(instance->struct_ == struct2);
}

static Value arrPushNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrPush", 2, argCount);
  assertArgIsArray(vm, "@arrPush", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);
  Value value = args[1];

  writeValueToArrayObj(vm->gc, array, value);

  return NIL_VAL;
}

static Value arrPopNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrPop", 1, argCount);
  assertArgIsArray(vm, "@arrPop", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  if (array->count == 0) {
    runtimeError(vm, "Cannot pop empty array.");
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  Value value = array->values[array->count - 1];

  array->count--;

  return value;
}

static Value arrInsertNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrInsert", 3, argCount);
  assertArgIsArray(vm, "@arrInsert", args, 0);
  assertArgIsNumber(vm, "@arrInsert", args, 1);

  ObjArray *array = AS_ARRAY(args[0]);
  int index = (int)AS_NUMBER(args[1]);
  Value value = args[2];

  assertIsInArrayBounds(vm, array, index);

  writeValueToArrayObj(vm->gc, array, NIL_VAL);

  for (int i = array->count - 1; i > index; i--) {
    array->values[i] = array->values[i - 1];
  }

  array->values[index] = value;

  return NIL_VAL;
}

static Value arrRemoveNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrRemove", 2, argCount);
  assertArgIsArray(vm, "@arrRemove", args, 0);
  assertArgIsNumber(vm, "@arrRemove", args, 1);

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
  assertArgCount(vm, "@arrClear", 1, argCount);
  assertArgIsArray(vm, "@arrClear", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  array->count = 0;

  return NIL_VAL;
}

static Value arrContainsNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrContains", 2, argCount);
  assertArgIsArray(vm, "@arrContains", args, 0);

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
  assertArgCount(vm, "@arrCopy", 1, argCount);
  assertArgIsArray(vm, "@arrCopy", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);
  ObjArray *result = newArray(vm->gc);

  pushOnStack(OBJ_VAL(result)); // Protect from GC

  for (int i = 0; i < array->count; i++) {
    writeValueToArrayObj(vm->gc, result, array->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(result);
}

static Value arrIsEmptyNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrIsEmpty", 1, argCount);
  assertArgIsArray(vm, "@arrIsEmpty", args, 0);

  ObjArray *array = AS_ARRAY(args[0]);

  bool isEmpty = array->count == 0;

  return BOOL_VAL(isEmpty);
}

static Value strIsEmptyNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strIsEmpty", 1, argCount);
  assertArgIsString(vm, "@strIsEmpty", args, 0);

  ObjString *string = AS_STRING(args[0]);

  bool isEmpty = string->length == 0;

  return BOOL_VAL(isEmpty);
}

static Value arrEqualNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrEqual", 2, argCount);
  assertArgIsArray(vm, "@arrEqual", args, 0);
  assertArgIsArray(vm, "@arrEqual", args, 1);

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
  assertArgCount(vm, "@arrSlice", 3, argCount);
  assertArgIsArray(vm, "@arrSlice", args, 0);
  assertArgIsNumber(vm, "@arrSlice", args, 1);
  assertArgIsNumber(vm, "@arrSlice", args, 2);

  ObjArray *array = AS_ARRAY(args[0]);

  double start = AS_NUMBER(args[1]);
  assertPositiveNumber(vm, "@arrSlice", start, 1);
  assertIsInArrayBounds(vm, array, start);

  double end = AS_NUMBER(args[2]);
  assertPositiveNumber(vm, "@arrSlice", end, 2);
  assertIsInArrayBounds(vm, array, end);

  if (start >= end) {
    runtimeError(
        vm, "function arrSlice expects argument 1 to be less than argument 2.");
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  ObjArray *result = newArray(vm->gc);

  pushOnStack(OBJ_VAL(result));

  for (int i = start; i < end; i++) {
    writeValueToArrayObj(vm->gc, result, array->values[i]);
  }

  popFromStack();

  return OBJ_VAL(result);
}

static Value arrConcatNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrConcat", 2, argCount);
  assertArgIsArray(vm, "@arrConcat", args, 0);
  assertArgIsArray(vm, "@arrConcat", args, 1);

  ObjArray *array_a = AS_ARRAY(args[0]);
  ObjArray *array_b = AS_ARRAY(args[1]);

  ObjArray *new_array = newArray(vm->gc);

  pushOnStack(OBJ_VAL(new_array)); // Protect from GC

  for (int i = 0; i < array_a->count; i++) {
    writeValueToArrayObj(vm->gc, new_array, array_a->values[i]);
  }

  for (int i = 0; i < array_b->count; i++) {
    writeValueToArrayObj(vm->gc, new_array, array_b->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(new_array);
}

static Value arrReverseNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrReverse", 1, argCount);
  assertArgIsArray(vm, "@arrReverse", args, 0);

  ObjArray *array_a = AS_ARRAY(args[0]);

  ObjArray *new_array = newArray(vm->gc);

  pushOnStack(OBJ_VAL(new_array)); // Protect from GC

  for (int i = array_a->count - 1; i >= 0; i--) {
    writeValueToArrayObj(vm->gc, new_array, array_a->values[i]);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(new_array);
}

/**
 * Concatenate every element of an array into one string, separated by a
 * separator string. Elements must already be strings; the language has no
 * implicit conversion to string.
 */
static Value arrJoinNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@arrJoin", 2, argCount);
  assertArgIsArray(vm, "@arrJoin", args, 0);
  assertArgIsString(vm, "@arrJoin", args, 1);

  ObjArray *array = AS_ARRAY(args[0]);
  ObjString *separator = AS_STRING(args[1]);

  int length = 0;

  for (int i = 0; i < array->count; i++) {
    if (!IS_STRING(array->values[i])) {
      char type[VALUE_STRING_MAX];

      valueTypeToString(array->values[i], type, sizeof(type));

      runtimeError(vm,
                   "function arrJoin expects every element of argument 1 to be "
                   "a string but element %d is a %s.",
                   i, type);
      exit(EXIT_CODE_RUNTIME_ERR);
    }

    length += AS_STRING(array->values[i])->length;
  }

  if (array->count > 1) {
    length += separator->length * (array->count - 1);
  }

  // args stay on the VM stack for the duration of a native call, so the array
  // and its strings are reachable if this allocation triggers a collection.
  char *chars = ALLOCATE(vm->gc, char, length + 1);
  int offset = 0;

  for (int i = 0; i < array->count; i++) {
    if (i > 0) {
      memcpy(chars + offset, separator->chars, separator->length);
      offset += separator->length;
    }

    ObjString *element = AS_STRING(array->values[i]);

    memcpy(chars + offset, element->chars, element->length);
    offset += element->length;
  }

  chars[length] = '\0';

  return OBJ_VAL(takeString(vm->gc, chars, length));
}

static Value stdinNative(VM *vm, int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(vm, "@stdin", 1, argCount);
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

  const size_t MAX_INPUT_BYTES = 1024 * 1024 * 5; // 5 MB cap

  size_t capacity = 64;
  size_t length = 0;
  char *buffer = ALLOCATE(vm->gc, char, capacity);

  int c;
  while ((c = getchar()) != EOF) {
    // Handle Windows CRLF: skip '\r'
    if (c == '\r')
      continue;

    if (length + 1 >= capacity) {
      size_t newCapacity = capacity * 2;

      if (newCapacity > MAX_INPUT_BYTES) {
        newCapacity = MAX_INPUT_BYTES;
      }

      if (capacity == MAX_INPUT_BYTES) {
        FREE_ARRAY(vm->gc, char, buffer, capacity);
        runtimeError(vm, "stdin() exceeded maximum length.");
        return NIL_VAL;
      }

      buffer = GROW_ARRAY(vm->gc, char, buffer, capacity, newCapacity);
      capacity = newCapacity;
    }

    buffer[length++] = (char)c;
  }

  if (c == EOF && length == 0) {
    FREE_ARRAY(vm->gc, char, buffer, capacity);
    return NIL_VAL;
  }

  buffer[length] = '\0';

  return OBJ_VAL(takeString(vm->gc, buffer, length));
}

static Value promptNative(VM *vm, int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(vm, "@prompt", 1, argCount);
    assertArgIsString(vm, "@prompt", args, 0);
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

  const size_t MAX_INPUT_BYTES = 1024 * 1024; // 1 MB cap

  size_t capacity = 64;
  size_t length = 0;
  char *buffer = ALLOCATE(vm->gc, char, capacity);

  int c;
  while ((c = getchar()) != '\n' && c != EOF) {
    // Handle Windows CRLF: skip '\r'
    if (c == '\r')
      continue;

    if (length + 1 >= capacity) {
      size_t newCapacity = capacity * 2;

      if (newCapacity > MAX_INPUT_BYTES) {
        newCapacity = MAX_INPUT_BYTES;
      }

      if (capacity == MAX_INPUT_BYTES) {
        FREE_ARRAY(vm->gc, char, buffer, capacity);
        runtimeError(vm, "input() exceeded maximum length.");
        return NIL_VAL;
      }

      buffer = GROW_ARRAY(vm->gc, char, buffer, capacity, newCapacity);
      capacity = newCapacity;
    }

    buffer[length++] = (char)c;
  }

  if (c == EOF && length == 0) {
    FREE_ARRAY(vm->gc, char, buffer, capacity);
    return NIL_VAL;
  }

  buffer[length] = '\0';

  return OBJ_VAL(takeString(vm->gc, buffer, length));
}

static Value argvNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@argv", 1, argCount);
  assertArgIsNumber(vm, "@argv", args, 0);

  Value index = args[0];

  int idx = (int)AS_NUMBER(index);
  if (idx < 0 || idx >= vm->argc) {
    return NIL_VAL;
  }

  const char *c_arg = vm->argv[idx];
  ObjString *kirbyStr = copyString(vm->gc, c_arg, (int)strlen(c_arg));

  return OBJ_VAL(kirbyStr);
}

static Value argcNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@argc", 0, argCount);

  (void)args;

  int argc = vm->argc;

  return NUMBER_VAL(argc);
}

static Value parseNumberNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@parseNumber", 1, argCount);
  assertArgIsString(vm, "@parseNumber", args, 0);

  Value value = args[0];

  char *valueString = AS_CSTRING(value);
  int valueInt = strtol(valueString, NULL, 10);

  return NUMBER_VAL(valueInt);
}

static Value numberToStringNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@numberToString", 1, argCount);
  assertArgIsNumber(vm, "@numberToString", args, 0);

  Value value = args[0];

  char buffer[32];

  int length = snprintf(buffer, sizeof(buffer), "%.15g", value.as.number);

  return OBJ_VAL(copyString(vm->gc, buffer, length));
}

static Value floorNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@floor", 1, argCount);
  assertArgIsNumber(vm, "@floor", args, 0);

  return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

// Rounds halves away from zero: round(2.5) is 3, round(-2.5) is -3.
static Value roundNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@round", 1, argCount);
  assertArgIsNumber(vm, "@round", args, 0);

  return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

// Drops the fractional part, moving toward zero: trunc(-2.7) is -2.
static Value truncNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@trunc", 1, argCount);
  assertArgIsNumber(vm, "@trunc", args, 0);

  return NUMBER_VAL(trunc(AS_NUMBER(args[0])));
}

static Value absNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@abs", 1, argCount);
  assertArgIsNumber(vm, "@abs", args, 0);

  return NUMBER_VAL(fabs(AS_NUMBER(args[0])));
}

static Value sqrtNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@sqrt", 1, argCount);
  assertArgIsNumber(vm, "@sqrt", args, 0);

  double number = AS_NUMBER(args[0]);

  assertNonNegativeNumber(vm, "@sqrt", number, 0);

  return NUMBER_VAL(sqrt(number));
}

// Raising 0 to a negative power divides by zero, and a negative number to a
// fractional power has no real result. Both raise an error, like `/` with a
// zero divisor and `@sqrt` of a negative number.
static Value powNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@pow", 2, argCount);
  assertArgIsNumber(vm, "@pow", args, 0);
  assertArgIsNumber(vm, "@pow", args, 1);

  double base = AS_NUMBER(args[0]);
  double exponent = AS_NUMBER(args[1]);

  if (base == 0 && exponent < 0) {
    runtimeError(vm, "function @pow can't raise 0 to a negative power (%g).",
                 exponent);
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  if (base < 0 && exponent != floor(exponent)) {
    runtimeError(vm,
                 "function @pow can't raise a negative number (%g) to a "
                 "fractional power (%g).",
                 base, exponent);
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  return NUMBER_VAL(pow(base, exponent));
}

static Value minNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@min", 2, argCount);
  assertArgIsNumber(vm, "@min", args, 0);
  assertArgIsNumber(vm, "@min", args, 1);

  return NUMBER_VAL(fmin(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static Value maxNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@max", 2, argCount);
  assertArgIsNumber(vm, "@max", args, 0);
  assertArgIsNumber(vm, "@max", args, 1);

  return NUMBER_VAL(fmax(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

// Stops the program with a message written by the script, after a fixed
// prefix. The message is printed as is, never used as a format string.
static void raiseScriptMessage(VM *vm, const char *prefix, ObjString *message) {
  runtimeError(vm, "%s%.*s", prefix, message->length, message->chars);
  exit(EXIT_CODE_RUNTIME_ERR);
}

static Value assertNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@assert", 2, argCount);
  assertArgIsBool(vm, "@assert", args, 0);
  assertArgIsString(vm, "@assert", args, 1);

  if (!AS_BOOL(args[0])) {
    raiseScriptMessage(vm, "assertion failed: ", AS_STRING(args[1]));
  }

  return NIL_VAL;
}

static Value panicNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@panic", 1, argCount);
  assertArgIsString(vm, "@panic", args, 0);

  raiseScriptMessage(vm, "panic: ", AS_STRING(args[0]));

  return NIL_VAL;
}

// Finds the first place `needle` appears in `haystack` at or after `start`.
// Returns its byte offset, or -1 if it doesn't appear. An empty needle is
// found at `start`.
static int findBytes(const char *haystack, int haystackLength, int start,
                     const char *needle, int needleLength) {
  for (int i = start; i + needleLength <= haystackLength; i++) {
    if (memcmp(haystack + i, needle, needleLength) == 0) {
      return i;
    }
  }

  return -1;
}

static Value strContainsNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strContains", 2, argCount);
  assertArgIsString(vm, "@strContains", args, 0);
  assertArgIsString(vm, "@strContains", args, 1);

  ObjString *string = AS_STRING(args[0]);
  ObjString *sub = AS_STRING(args[1]);

  int index =
      findBytes(string->chars, string->length, 0, sub->chars, sub->length);

  return BOOL_VAL(index >= 0);
}

static Value strStartsWithNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strStartsWith", 2, argCount);
  assertArgIsString(vm, "@strStartsWith", args, 0);
  assertArgIsString(vm, "@strStartsWith", args, 1);

  ObjString *string = AS_STRING(args[0]);
  ObjString *prefix = AS_STRING(args[1]);

  bool startsWith = prefix->length <= string->length &&
                    memcmp(string->chars, prefix->chars, prefix->length) == 0;

  return BOOL_VAL(startsWith);
}

static Value strEndsWithNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strEndsWith", 2, argCount);
  assertArgIsString(vm, "@strEndsWith", args, 0);
  assertArgIsString(vm, "@strEndsWith", args, 1);

  ObjString *string = AS_STRING(args[0]);
  ObjString *suffix = AS_STRING(args[1]);

  bool endsWith = suffix->length <= string->length &&
                  memcmp(string->chars + string->length - suffix->length,
                         suffix->chars, suffix->length) == 0;

  return BOOL_VAL(endsWith);
}

// The whitespace strTrim removes. Not isspace(), which depends on the locale.
static bool isTrimmedChar(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static Value strTrimNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strTrim", 1, argCount);
  assertArgIsString(vm, "@strTrim", args, 0);

  ObjString *string = AS_STRING(args[0]);

  int start = 0;
  int end = string->length;

  while (start < end && isTrimmedChar(string->chars[start])) {
    start++;
  }

  while (end > start && isTrimmedChar(string->chars[end - 1])) {
    end--;
  }

  return OBJ_VAL(copyString(vm->gc, string->chars + start, end - start));
}

// Copies a string, changing only the ASCII letters 'a'-'z' (or 'A'-'Z'). Every
// other byte, including the bytes of multi-byte characters, is kept as is.
static Value changeAsciiCase(VM *vm, ObjString *string, bool toUpper) {
  // The string is still an argument on the VM stack, so it stays reachable if
  // this allocation triggers a collection.
  char *chars = ALLOCATE(vm->gc, char, string->length + 1);

  for (int i = 0; i < string->length; i++) {
    char c = string->chars[i];

    if (toUpper && c >= 'a' && c <= 'z') {
      c = (char)(c - 'a' + 'A');
    } else if (!toUpper && c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }

    chars[i] = c;
  }

  chars[string->length] = '\0';

  return OBJ_VAL(takeString(vm->gc, chars, string->length));
}

static Value strToUpperNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strToUpper", 1, argCount);
  assertArgIsString(vm, "@strToUpper", args, 0);

  return changeAsciiCase(vm, AS_STRING(args[0]), true);
}

static Value strToLowerNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strToLower", 1, argCount);
  assertArgIsString(vm, "@strToLower", args, 0);

  return changeAsciiCase(vm, AS_STRING(args[0]), false);
}

static Value strRepeatNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strRepeat", 2, argCount);
  assertArgIsString(vm, "@strRepeat", args, 0);
  assertArgIsNumber(vm, "@strRepeat", args, 1);

  ObjString *string = AS_STRING(args[0]);
  double number = AS_NUMBER(args[1]);

  assertNonNegativeNumber(vm, "@strRepeat", number, 1);
  assertWholeNumber(vm, "@strRepeat", number, 1);

  // A count of 0 repeats nothing
  if (string->length == 0 || number < 1) {
    return OBJ_VAL(copyString(vm->gc, "", 0));
  }

  // Written as a negated comparison so NaN is rejected too. Checking before
  // converting to int keeps the conversion and the multiplication from
  // overflowing.
  if (!(number <= (double)(INT_MAX - 1) / string->length)) {
    raiseError(vm, "function @strRepeat result is too large.");
  }

  int count = (int)number;
  int length = string->length * count;

  char *chars = ALLOCATE(vm->gc, char, length + 1);

  for (int i = 0; i < count; i++) {
    memcpy(chars + i * string->length, string->chars, string->length);
  }

  chars[length] = '\0';

  return OBJ_VAL(takeString(vm->gc, chars, length));
}

// Appends a copy of `length` bytes to an array as a new string. The new string
// is held on the VM stack while the array grows, since growing the array can
// start a collection before the string is reachable from anywhere else.
static void appendStringCopy(VM *vm, ObjArray *array, const char *chars,
                             int length) {
  ObjString *piece = copyString(vm->gc, chars, length);

  pushOnStack(OBJ_VAL(piece));
  writeValueToArrayObj(vm->gc, array, OBJ_VAL(piece));
  popFromStack();
}

static Value strSplitNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strSplit", 2, argCount);
  assertArgIsString(vm, "@strSplit", args, 0);
  assertArgIsString(vm, "@strSplit", args, 1);

  ObjString *string = AS_STRING(args[0]);
  ObjString *separator = AS_STRING(args[1]);

  ObjArray *result = newArray(vm->gc);

  pushOnStack(OBJ_VAL(result)); // Protect from GC

  if (separator->length == 0) {
    // With no separator every byte becomes its own string
    for (int i = 0; i < string->length; i++) {
      appendStringCopy(vm, result, string->chars + i, 1);
    }
  } else {
    int start = 0;
    int found;

    while ((found = findBytes(string->chars, string->length, start,
                              separator->chars, separator->length)) >= 0) {
      appendStringCopy(vm, result, string->chars + start, found - start);
      start = found + separator->length;
    }

    appendStringCopy(vm, result, string->chars + start, string->length - start);
  }

  popFromStack(); // Clean up after GC protection

  return OBJ_VAL(result);
}

static Value strIndexOfNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strIndexOf", 2, argCount);
  assertArgIsString(vm, "@strIndexOf", args, 0);
  assertArgIsString(vm, "@strIndexOf", args, 1);

  ObjString *string = AS_STRING(args[0]);
  ObjString *sub = AS_STRING(args[1]);

  int index =
      findBytes(string->chars, string->length, 0, sub->chars, sub->length);

  if (index < 0) {
    return NIL_VAL;
  }

  return NUMBER_VAL(index);
}

// Asserts a number is a whole position in the string: 0 up to and including
// its length. Returns it as an int.
static int assertStringPosition(VM *vm, const char *function, ObjString *string,
                                double number, int index) {
  assertWholeNumber(vm, function, number, index);

  // Written as a negated comparison so NaN is rejected too
  if (!(number >= 0 && number <= string->length)) {
    runtimeError(
        vm,
        "function %s expects argument %d to be between 0 and %d but got %g.",
        function, index + 1, string->length, number);
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  return (int)number;
}

static Value strSliceNative(VM *vm, int argCount, Value *args) {
  assertArgCount(vm, "@strSlice", 3, argCount);
  assertArgIsString(vm, "@strSlice", args, 0);
  assertArgIsNumber(vm, "@strSlice", args, 1);
  assertArgIsNumber(vm, "@strSlice", args, 2);

  ObjString *string = AS_STRING(args[0]);

  int start =
      assertStringPosition(vm, "@strSlice", string, AS_NUMBER(args[1]), 1);
  int end =
      assertStringPosition(vm, "@strSlice", string, AS_NUMBER(args[2]), 2);

  if (start > end) {
    runtimeError(vm, "function @strSlice expects argument 2 to be less than or "
                     "equal to argument 3.");
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  return OBJ_VAL(copyString(vm->gc, string->chars + start, end - start));
}

// Replaces the first match of `old` in a string, or every match when `all` is
// true. Matches are found left to right and never overlap. A string with no
// matches is returned as is. An empty `old` never matches.
static Value replaceMatches(VM *vm, const char *function, int argCount,
                            Value *args, bool all) {
  assertArgCount(vm, function, 3, argCount);
  assertArgIsString(vm, function, args, 0);
  assertArgIsString(vm, function, args, 1);
  assertArgIsString(vm, function, args, 2);

  ObjString *string = AS_STRING(args[0]);
  ObjString *old = AS_STRING(args[1]);
  ObjString *replacement = AS_STRING(args[2]);

  if (old->length == 0) {
    return args[0];
  }

  int matches = 0;
  int at = findBytes(string->chars, string->length, 0, old->chars, old->length);

  while (at >= 0) {
    matches++;

    if (!all) {
      break;
    }

    at = findBytes(string->chars, string->length, at + old->length, old->chars,
                   old->length);
  }

  if (matches == 0) {
    return args[0];
  }

  // Done in long long so a large replacement can't overflow before the check
  long long length =
      (long long)string->length +
      (long long)matches * ((long long)replacement->length - old->length);

  if (length >= INT_MAX) {
    runtimeError(vm, "function %s result is too large.", function);
    exit(EXIT_CODE_RUNTIME_ERR);
  }

  // The string is still an argument on the VM stack, so it stays reachable if
  // this allocation triggers a collection.
  char *chars = ALLOCATE(vm->gc, char, (int)length + 1);
  int read = 0;
  int write = 0;

  for (int i = 0; i < matches; i++) {
    int next =
        findBytes(string->chars, string->length, read, old->chars, old->length);

    memcpy(chars + write, string->chars + read, next - read);
    write += next - read;

    memcpy(chars + write, replacement->chars, replacement->length);
    write += replacement->length;

    read = next + old->length;
  }

  memcpy(chars + write, string->chars + read, string->length - read);
  chars[length] = '\0';

  return OBJ_VAL(takeString(vm->gc, chars, (int)length));
}

static Value strReplaceNative(VM *vm, int argCount, Value *args) {
  return replaceMatches(vm, "@strReplace", argCount, args, false);
}

static Value strReplaceAllNative(VM *vm, int argCount, Value *args) {
  return replaceMatches(vm, "@strReplaceAll", argCount, args, true);
}

const NativeDefinition nativeDefinitions[] = {
    {"@clock", clockNative},
    {"@version", versionNative},
    {"@exit", exitNative},
    {"@rand", randNative},
    {"@rand01", rand01Native},
    {"@randBetween", randBetweenNative},
    {"@ceil", ceilNative},
    {"@readFileToString", readFileToStringNative},
    {"@writeStringToFile", writeStringToFileNative},
    {"@numberToString", numberToStringNative},
    {"@fileExists", fileExistsNative},
    {"@getenv", getEnvNative},
    {"@setenv", setEnvNative},
    {"@len", lenNative},
    {"@typeof", typeofNative},
    {"@argv", argvNative},
    {"@argc", argcNative},
    {"@parseNumber", parseNumberNative},
    {"@instanceOf", instanceOfNative},
    {"@prompt", promptNative},
    {"@stdin", stdinNative},
    {"@arrPush", arrPushNative},
    {"@arrPop", arrPopNative},
    {"@arrInsert", arrInsertNative},
    {"@arrRemove", arrRemoveNative},
    {"@arrClear", arrClearNative},
    {"@arrContains", arrContainsNative},
    {"@arrCopy", arrCopyNative},
    {"@arrIsEmpty", arrIsEmptyNative},
    {"@arrEqual", arrEqualNative},
    {"@arrSlice", arrSliceNative},
    {"@arrConcat", arrConcatNative},
    {"@arrReverse", arrReverseNative},
    {"@arrJoin", arrJoinNative},
    {"@is", isNative},
    {"@isNumber", isNumberNative},
    {"@isFunction", isFunctionNative},
    {"@isBool", isBoolNative},
    {"@isString", isStringNative},
    {"@isNil", isNilNative},
    {"@strIsEmpty", strIsEmptyNative},
    {"@floor", floorNative},
    {"@round", roundNative},
    {"@trunc", truncNative},
    {"@abs", absNative},
    {"@sqrt", sqrtNative},
    {"@pow", powNative},
    {"@min", minNative},
    {"@max", maxNative},
    {"@assert", assertNative},
    {"@panic", panicNative},
    {"@strContains", strContainsNative},
    {"@strStartsWith", strStartsWithNative},
    {"@strEndsWith", strEndsWithNative},
    {"@strTrim", strTrimNative},
    {"@strToUpper", strToUpperNative},
    {"@strToLower", strToLowerNative},
    {"@strRepeat", strRepeatNative},
    {"@strSplit", strSplitNative},
    {"@strIndexOf", strIndexOfNative},
    {"@strSlice", strSliceNative},
    {"@strReplace", strReplaceNative},
    {"@strReplaceAll", strReplaceAllNative},
};

const int nativeDefinitionCount =
    (int)(sizeof(nativeDefinitions) / sizeof(nativeDefinitions[0]));

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
};

const int nativeSignatureCount =
    (int)(sizeof(nativeSignatures) / sizeof(nativeSignatures[0]));

static bool _isNonNegative(double value) { return value >= 0; }

const NativeArgConstraint nativeArgConstraints[] = {
    {"@sqrt", 0, _isNonNegative, "a non-negative number"},
    {"@exit", 0, _isNonNegative, "a non-negative number"},
};

const int nativeArgConstraintCount =
    (int)(sizeof(nativeArgConstraints) / sizeof(nativeArgConstraints[0]));

void defineAllNatives(VM *vm) {
  for (int i = 0; i < nativeDefinitionCount; i++) {
    defineNative(vm, nativeDefinitions[i].name, nativeDefinitions[i].function);
  }
}

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
