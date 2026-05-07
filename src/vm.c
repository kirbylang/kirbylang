#include "sys/stat.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "assert.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "string.h"
#include "version.h"
#include "vm.h"

static ObjString *readFile(const char *path);
static void resetStack();
void runtimeError(VM *vm, const char *format, ...);
static Value peek(int distance);
static InterpretResult run();
static void concatenate();
static bool call(ObjClosure *function, int argCount);
Value fileExists(char *filename);
static void writeFile(const char *path, const char *text);

VM vm;

static Value clockNative(int argCount, Value *args) {
  assertArgCount(&vm, "clock", 0, argCount);

  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value versionNative(int argCount, Value *args) {
  assertArgCount(&vm, "__version__", 0, argCount);

  return OBJ_VAL(copyString(CLOX_VERSION, CLOX_VERSION_len));
}

static Value exitNative(int argCount, Value *args) {
  assertArgCount(&vm, "exit", 1, argCount);
  assertArgIsNumber(&vm, "exit", args, 0);

  double exitCode = args[0].as.number;

  assertPositiveNumber(&vm, "exit", exitCode, 0);

  exit(exitCode);

  return NIL_VAL;
}

static Value randNative(int argCount, Value *args) {
  assertArgCount(&vm, "rand", 0, argCount);

  return NUMBER_VAL((double)rand());
}

static Value rand01Native(int argCount, Value *args) {
  assertArgCount(&vm, "rand01", 0, argCount);

  return NUMBER_VAL((double)rand() / (double)RAND_MAX);
}

static Value randBetweenNative(int argCount, Value *args) {
  assertArgCount(&vm, "randBetween", 2, argCount);
  assertArgIsNumber(&vm, "randBetween", args, 0);
  assertArgIsNumber(&vm, "randBetween", args, 1);

  double min = AS_NUMBER(args[0]);
  double max = AS_NUMBER(args[1]);

  if (min > max) {
    raiseError(&vm, "randomRange(min, max) requires min <= max");
  }

  double r = (double)rand() / (double)RAND_MAX; // [0, 1]
  double result = min + r * (max - min);        // [min, max)

  return NUMBER_VAL(ceil(result));
}

static Value ceilNative(int argCount, Value *args) {
  assertArgCount(&vm, "ceil", 1, argCount);
  assertArgIsNumber(&vm, "ceil", args, 0);

  Value value = args[0];

  return NUMBER_VAL(ceil(AS_NUMBER(value)));
}

static Value fileExistsNative(int argCount, Value *args) {
  assertArgCount(&vm, "fileExists", 1, argCount);
  assertArgIsString(&vm, "fileExists", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);

  return fileExists(path->chars);
}

static Value readFileToStringNative(int argCount, Value *args) {
  assertArgCount(&vm, "readFileToString", 1, argCount);
  assertArgIsString(&vm, "readFileToString", args, 0);

  Value value = args[0];

  ObjString *path = AS_STRING(value);
  ObjString *contents = readFile(path->chars);

  return OBJ_VAL(contents);
}

static Value writeStringToFileNative(int argCount, Value *args) {
  assertArgCount(&vm, "writeStringToFile", 2, argCount);
  assertArgIsString(&vm, "writeStringToFile", args, 0);
  assertArgIsString(&vm, "writeStringToFile", args, 1);

  Value path_arg = args[0];
  Value text_arg = args[1];

  ObjString *path = AS_STRING(path_arg);
  char *text = AS_CSTRING(text_arg);

  writeFile(path->chars, text);

  return NIL_VAL;
}

static Value getEnvNative(int argCount, Value *args) {
  assertArgCount(&vm, "getenv", 1, argCount);
  assertArgIsString(&vm, "getenv", args, 0);

  Value name = args[0];

  char *name_str = AS_CSTRING(name);

  char *env_value = getenv(name_str);

  if (env_value == NULL) {
    runtimeError(&vm, "Environment variable not found: '%s'", name_str);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  return OBJ_VAL(copyString(env_value, (int)strlen(env_value)));
}

static Value setEnvNative(int argCount, Value *args) {
  assertArgCount(&vm, "setenv", 2, argCount);
  assertArgIsString(&vm, "setenv", args, 0);
  assertArgIsString(&vm, "setenv", args, 1);

  ObjString *name = AS_STRING(args[0]);
  ObjString *value = AS_STRING(args[1]);

  // overwrite = 1 means always replace existing value
  int result = setenv(name->chars, value->chars, 1);

  if (result != 0) {
    runtimeError(&vm, "setenv unable to set environment variable: '%s'",
                 name->chars);
    exit(70); // INTERPRET_RUNTIME_ERROR
  }

  return NIL_VAL; // or you could return true if you prefer
}

static Value lenNative(int argCount, Value *args) {
  assertArgCount(&vm, "len", 1, argCount);
  assertArgIsString(&vm, "len", args, 0);

  Value name = args[0];

  int length = AS_STRING(name)->length;

  return NUMBER_VAL(length);
}

static Value typeofNative(int argCount, Value *args) {
  assertArgCount(&vm, "typeof", 1, argCount);

  Value value = args[0];

  // Longest value/object type is function (8 chars) + null terminator
  char *buffer = malloc(9);

  if (buffer == NULL) {
    raiseError(&vm, "typeof(value) unable to allocate string for type string.");
  }

  valueTypeToString(value, buffer, 9);

  return OBJ_VAL(takeString(buffer, strlen(buffer)));
}

static Value instanceOfNative(int argCount, Value *args) {
  assertArgCount(&vm, "instanceOf", 2, argCount);
  assertArgIsClass(&vm, "instanceOf", args, 1);

  Value value = args[0];

  if (!IS_INSTANCE(value)) {
    return BOOL_VAL(false);
  }

  Value klass = args[1];

  ObjInstance *instance = AS_INSTANCE(value);
  ObjClass *klass2 = AS_CLASS(klass);

  return BOOL_VAL(instance->klass == klass2);
}

static Value stdinNative(int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(&vm, "stdin", 1, argCount);
  }

  if (argCount == 1) {
    if (!IS_STRING(args[0])) {
      runtimeError(&vm, "input() argument must be a string.");
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
        runtimeError(&vm, "stdin() exceeded maximum length.");
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

static Value promptNative(int argCount, Value *args) {
  if (argCount > 1) {
    assertArgCount(&vm, "prompt", 1, argCount);
    assertArgIsString(&vm, "prompt", args, 0);
  }

  if (argCount == 1) {
    if (!IS_STRING(args[0])) {
      runtimeError(&vm, "prompt() argument must be a string.");
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
        runtimeError(&vm, "input() exceeded maximum length.");
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

static Value argvNative(int argCount, Value *args) {
  assertArgCount(&vm, "argv", 1, argCount);
  assertArgIsNumber(&vm, "argv", args, 0);

  Value index = args[0];

  int idx = (int)AS_NUMBER(index);
  if (idx < 0 || idx >= vm.argc) {
    return NIL_VAL;
  }

  const char *c_arg = vm.argv[idx];
  ObjString *cloxStr = copyString(c_arg, (int)strlen(c_arg));

  return OBJ_VAL(cloxStr);
}

static Value argcNative(int argCount, Value *args) {
  assertArgCount(&vm, "argc", 0, argCount);

  int argc = vm.argc;

  return NUMBER_VAL(argc);
}

static Value parseNumberNative(int argCount, Value *args) {
  assertArgCount(&vm, "parseNumber", 1, argCount);
  assertArgIsString(&vm, "parseNumber", args, 0);

  Value value = args[0];

  char *valueString = AS_CSTRING(value);
  int valueInt = strtol(valueString, NULL, 10);

  return NUMBER_VAL(valueInt);
}

InterpretResult interpretFunction(ObjFunction *function) {
  TRACELN("vm.interpretFunction()");

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  ObjClosure *closure = newClosure(function);
  pop();

  push(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}

InterpretResult interpret(const char *source) {
  TRACELN("vm.interpret()");

  Scanner scanner;
  initScanner(&scanner, source);

  ObjFunction *function = compile(&scanner, source);

  return interpretFunction(function);
}

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm->frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm->frames[i];
    ObjFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code - 1;

    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static void defineNative(const char *name, NativeFn function) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function)));
  tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM(int argc, char *argv[]) {
  TRACELN("vm.initVM()");
  srand(time(NULL));
  resetStack();
  vm.argc = argc;
  vm.argv = argv;
  vm.objects = NULL;
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;

  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  initTable(&vm.globals);
  initTable(&vm.strings);

  vm.initString = NULL;
  vm.initString = copyString("init", 4);

  defineNative("clock", clockNative);
  defineNative("__version__", versionNative);
  defineNative("exit", exitNative);
  defineNative("rand", randNative);
  defineNative("rand01", rand01Native);
  defineNative("randBetween", randBetweenNative);
  defineNative("ceil", ceilNative);
  defineNative("readFileToString", readFileToStringNative);
  defineNative("writeStringToFile", writeStringToFileNative);
  defineNative("fileExists", fileExistsNative);
  defineNative("getenv", getEnvNative);
  defineNative("setenv", setEnvNative);
  defineNative("len", lenNative);
  defineNative("typeof", typeofNative);
  defineNative("argv", argvNative);
  defineNative("argc", argcNative);
  defineNative("parseNumber", parseNumberNative);
  defineNative("instanceOf", instanceOfNative);
  defineNative("prompt", promptNative);
  defineNative("stdin", stdinNative);
}

void freeVM() {
  TRACELN("vm.freeVM()");
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) { return vm.stackTop[-1 - distance]; }

static bool call(ObjClosure *closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError(&vm, "Expected %d arguments but got %d.",
                 closure->function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError(&vm, "Stack overflow.");
    return false;
  }

  CallFrame *frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLASS: {
      ObjClass *klass = AS_CLASS(callee);
      vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));

      Value initializer;
      if (tableGet(&klass->methods, vm.initString, &initializer)) {
        return call(AS_CLOSURE(initializer), argCount);
      } else if (argCount != 0) {
        runtimeError(&vm, "Expected 0 arguments but got %d.", argCount);
        return false;
      }

      return true;
    }
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
      vm.stackTop[-argCount - 1] = bound->receiver;
      return call(bound->method, argCount);
    }
    case OBJ_CLOSURE:
      return call(AS_CLOSURE(callee), argCount);
    case OBJ_NATIVE: {
      NativeFn native = AS_NATIVE(callee);
      Value result = native(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    }
  }
  runtimeError(&vm, "Can only call functions and classes.");
  return false;
}

static bool invokeFromClass(ObjClass *klass, ObjString *name, int argCount) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError(&vm, "Undefined property '%s'.", name->chars);
    return false;
  }
  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString *name, int argCount) {
  Value receiver = peek(argCount);

  if (!IS_INSTANCE(receiver)) {
    runtimeError(&vm, "Only instances have methods.");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(receiver);

  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callValue(value, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass *klass, ObjString *name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    runtimeError(&vm, "Undefined property '%s'.", name->chars);
    return false;
  }

  ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpvalue *captureUpvalue(Value *local) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue *createdUpvalue = newUpvalue(local);

  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value *last) {
  while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static void defineMethod(ObjString *name) {
  Value method = peek(0);
  ObjClass *klass = AS_CLASS(peek(1));
  tableSet(&klass->methods, name, method);
  pop();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString *b = AS_STRING(peek(0));
  ObjString *a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static ObjString *readFile(const char *path) {
  FILE *file = fopen(path, "rb"); // "rb" = binary mode (safe for all files)
  if (!file)
    return NULL;

  // Go to end to get file size
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

static void writeFile(const char *path, const char *text) {
  FILE *file = fopen(path, "wb"); // "rb" = binary mode (safe for all files)
  if (!file)
    return;

  fprintf(file, "%s", text);

  return;
}

Value fileExists(char *filename) {
  struct stat buffer;
  return BOOL_VAL((stat(filename, &buffer) == 0));
}

static InterpretResult run() {
  CallFrame *f = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*f->ip++)
#define READ_CONSTANT()                                                        \
  (f->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (f->ip += 2, (uint16_t)((f->ip[-2] << 8) | f->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError(&vm, "Operands must be numbers.");                          \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop());                                               \
    double a = AS_NUMBER(pop());                                               \
    push(valueType(a op b));                                                   \
  } while (false)

  TRACELN("vm.run()");

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    fprintf(stderr, "          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      fprintf(stderr, "[ ");
      printValueToErr(*slot);
      fprintf(stderr, " ]");
    }

    fprintf(stderr, "\n");

    disassembleInstruction(&f->closure->function->chunk,
                           (int)(f->ip - f->closure->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError(&vm, "Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE: {
      assertNonZero(&vm, "/", peek(0).as.number, 1);
      assertNonZero(&vm, "/", peek(1).as.number, 0);
      BINARY_OP(NUMBER_VAL, /);
      break;
    }
    case OP_MODULO: {
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
        runtimeError(&vm, "Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }

      double b = AS_NUMBER(pop());
      double a = AS_NUMBER(pop());

      push(NUMBER_VAL(fmod(a, b)));
      break;
    }
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0))) {
        runtimeError(&vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
      break;
    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_RETURN: {
      Value result = pop();
      closeUpvalues(f->slots);
      vm.frameCount--;
      if (vm.frameCount == 0) {
        pop();
        return INTERPRET_OK;
      }

      vm.stackTop = f->slots;
      push(result);
      f = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_POP: {
      pop();
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(f->slots[slot]);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      f->slots[slot] = peek(0);
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError(&vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();

      if (tableSet(&vm.globals, name, peek(0))) {
        tableDelete(&vm.globals, name);
        runtimeError(&vm, "Undefined variable %s", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (isFalsey(peek(0)))
        f->ip += offset;
      break;
    }
    case OP_JUMP_IF_NOT_NIL: {
      uint16_t offset = READ_SHORT();
      if (!IS_NIL(peek(0)))
        f->ip += offset;
      break;
    }
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      f->ip += offset;
      break;
    }
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      f->ip -= offset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount), argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      f = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function);
      push(OBJ_VAL(closure));

      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(f->slots + index);
        } else {
          closure->upvalues[i] = f->closure->upvalues[index];
        }
      }

      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(*f->closure->upvalues[slot]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *f->closure->upvalues[slot]->location = peek(0);
      break;
    }
    case OP_CLOSE_UPVALUE:
      closeUpvalues(vm.stackTop - 1);
      pop();
      break;
    case OP_CLASS:
      push(OBJ_VAL(newClass(READ_STRING())));
      break;
    case OP_GET_FIELD: {
      uint8_t slot = READ_BYTE();
      if (!IS_INSTANCE(peek(0))) {
        runtimeError(&vm, "Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(0));
      pop();
      push(instance->fields[slot]);
      break;
    }
    case OP_SET_FIELD: {
      uint8_t slot = READ_BYTE();
      Value value = peek(0);
      if (!IS_INSTANCE(peek(1))) {
        runtimeError(&vm, "Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjInstance *instance = AS_INSTANCE(peek(1));
      instance->fields[slot] = value;
      pop();
      pop();
      push(value);
      break;
    }
    case OP_GET_PROPERTY: {
      if (!IS_INSTANCE(peek(0))) {
        runtimeError(&vm, "Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peek(0));
      ObjString *name = READ_STRING();

      Value value;
      if (tableGet(&instance->fields, name, &value)) {
        pop(); // Instance.
        push(value);
        break;
      }

      if (!bindMethod(instance->klass, name)) {
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_SET_PROPERTY: {
      if (!IS_INSTANCE(peek(1))) {
        runtimeError(&vm, "Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peek(1));
      tableSet(&instance->fields, READ_STRING(), peek(0));
      Value value = pop();
      pop();
      push(value);
      break;
    }
    case OP_METHOD:
      defineMethod(READ_STRING());
      break;
    case OP_INVOKE: {
      ObjString *method = READ_STRING();
      int argCount = READ_BYTE();
      if (!invoke(method, argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      f = &vm.frames[vm.frameCount - 1];
      break;
    }
    }
  }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}
