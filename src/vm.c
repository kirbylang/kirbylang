#include "sys/stat.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "asserts.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "parser.h"
#include "string.h"
#include "version.h"
#include "vm.h"

static void resetStack(void);
void runtimeError(VM *vm, const char *format, ...);
static Value peekStack(int distance);
static InterpretResult run(void);
static void concatenate(void);
static bool call(ObjClosure *function, int argCount);
static InterpretResult run(void);

VM vm;

InterpretResult interpretFunction(ObjFunction *function) {
  TRACELN("vm.interpretFunction()");

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  pushOnStack(OBJ_VAL(function));
  ObjClosure *closure = newClosure(function);
  popFromStack();

  pushOnStack(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}

InterpretResult interpret(const char *source) {
  TRACELN("vm.interpret()");

  int count = 0;
  bool parseHadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &parseHadError, &endLine);

  if (parseHadError) {
    astFreeAll();
    free(ast);
    return INTERPRET_COMPILE_ERROR;
  }

  ObjFunction *function = compile(ast, count, endLine);

  astFreeAll();
  free(ast);

  return interpretFunction(function);
}

static void resetStack(void) {
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

  defineAllNatives(&vm);
}

void freeVM(void) {
  TRACELN("vm.freeVM()");
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjects();
}

void pushOnStack(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value popFromStack(void) {
  vm.stackTop--;
  return *vm.stackTop;
}

void popNFromStack(int count) { vm.stackTop -= count; }

static Value peekStack(int distance) { return vm.stackTop[-1 - distance]; }

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
      Value result = native(&vm, argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      pushOnStack(result);
      return true;
    }
    default: {
      // Handled below
      break;
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
  Value caller = peekStack(argCount);

  if (!IS_INSTANCE(caller)) {
    runtimeError(&vm, "Only instances have methods.");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(caller);

  int slot;

  if (classFieldSlot(instance->klass, name, &slot)) {
    Value fieldValue = instance->fields[slot];

    vm.stackTop[argCount - 1] = fieldValue;
    return callValue(fieldValue, argCount);
  }

  return invokeFromClass(instance->klass, name, argCount);
}

static bool bindMethod(ObjClass *klass, ObjString *name) {
  Value method;
  if (!tableGet(&klass->methods, name, &method)) {
    return false;
  }

  ObjBoundMethod *bound = newBoundMethod(peekStack(0), AS_CLOSURE(method));
  popFromStack();
  pushOnStack(OBJ_VAL(bound));
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
  Value method = peekStack(0);
  ObjClass *klass = AS_CLASS(peekStack(1));
  tableSet(&klass->methods, name, method);
  popFromStack();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
  ObjString *b = AS_STRING(peekStack(0));
  ObjString *a = AS_STRING(peekStack(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length);
  popFromStack();
  popFromStack();
  pushOnStack(OBJ_VAL(result));
}

static InterpretResult run(void) {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peekStack(0)) || !IS_NUMBER(peekStack(1))) {                \
      runtimeError(&vm, "Operands must be numbers.");                          \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(popFromStack());                                      \
    double a = AS_NUMBER(popFromStack());                                      \
    pushOnStack(valueType(a op b));                                            \
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

    disassembleInstruction(
        &frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      // Reads then next byte operand as an index into the constant table
      // Looks up constant Value by index
      Value constant = READ_CONSTANT();
      pushOnStack(constant);
      break;
    }
    case OP_NIL:
      pushOnStack(NIL_VAL);
      break;
    case OP_TRUE:
      pushOnStack(BOOL_VAL(true));
      break;
    case OP_FALSE:
      pushOnStack(BOOL_VAL(false));
      break;
    case OP_ADD: {
      Value operand_b = peekStack(0);
      Value operand_a = peekStack(1);

      if (IS_STRING(operand_b) && IS_STRING(operand_a)) {
        concatenate();
      } else if (IS_NUMBER(operand_b) && IS_NUMBER(operand_a)) {
        double b = AS_NUMBER(popFromStack());
        double a = AS_NUMBER(popFromStack());

        Value result = NUMBER_VAL(a + b);

        pushOnStack(result);
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
      assertNonZero(&vm, "/", peekStack(0).as.number, 1);
      assertNonZero(&vm, "/", peekStack(1).as.number, 0);
      BINARY_OP(NUMBER_VAL, /);
      break;
    }
    case OP_MODULO: {
      if (!IS_NUMBER(peekStack(0)) || !IS_NUMBER(peekStack(1))) {
        runtimeError(&vm, "Operands must be numbers.");
        return INTERPRET_RUNTIME_ERROR;
      }

      double b = AS_NUMBER(popFromStack());
      double a = AS_NUMBER(popFromStack());

      Value result = NUMBER_VAL(fmod(a, b));

      pushOnStack(result);
      break;
    }
    case OP_NOT: {
      Value result = BOOL_VAL(isFalsey(popFromStack()));
      pushOnStack(result);
      break;
    }
    case OP_NEGATE: {
      Value value = peekStack(0);

      if (!IS_NUMBER(value)) {
        runtimeError(&vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      Value negated_value = NUMBER_VAL(-AS_NUMBER(popFromStack()));

      pushOnStack(negated_value);

      break;
    }
    case OP_PRINT: {
      Value value = popFromStack();

      printValue(value);
      printf("\n");
      break;
    }
    case OP_RETURN: {
      Value result = popFromStack();
      closeUpvalues(frame->slots);

      vm.frameCount--;
      if (vm.frameCount == 0) {
        popFromStack();
        return INTERPRET_OK;
      }

      vm.stackTop = frame->slots;
      pushOnStack(result);
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_EQUAL: {
      Value b = popFromStack();
      Value a = popFromStack();
      pushOnStack(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_POP: {
      popFromStack();
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      Value value = frame->slots[slot];

      pushOnStack(value);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      Value value = peekStack(0);

      frame->slots[slot] = value;
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;

      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError(&vm, "Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      pushOnStack(value);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value = peekStack(0);

      bool isNewKey = tableSet(&vm.globals, name, value);

      if (isNewKey) {
        tableDelete(&vm.globals, name);
        runtimeError(&vm, "Undefined variable %s", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value = peekStack(0);
      tableSet(&vm.globals, name, value);
      popFromStack();
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t jumpOffset = READ_SHORT();
      if (isFalsey(peekStack(0)))
        frame->ip += jumpOffset;
      break;
    }
    case OP_JUMP_IF_NOT_NIL: {
      uint16_t jumpOffset = READ_SHORT();
      if (!IS_NIL(peekStack(0)))
        frame->ip += jumpOffset;
      break;
    }
    case OP_JUMP: {
      uint16_t jumpOffset = READ_SHORT();
      frame->ip += jumpOffset;
      break;
    }
    case OP_LOOP: {
      uint16_t jumpOffset = READ_SHORT();
      frame->ip -= jumpOffset;
      break;
    }
    case OP_CALL: {
      int argCount = READ_BYTE();
      Value callee = peekStack(argCount);

      if (!callValue(callee, argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }

      frame = &vm.frames[vm.frameCount - 1];

      break;
    }
    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function);
      pushOnStack(OBJ_VAL(closure));

      for (int i = 0; i < closure->upvalueCount; i++) {
        uint8_t isLocal = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (isLocal) {
          closure->upvalues[i] = captureUpvalue(frame->slots + index);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }

      break;
    }
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      pushOnStack(*frame->closure->upvalues[slot]->location);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peekStack(0);
      break;
    }
    case OP_CLOSE_UPVALUE:
      closeUpvalues(vm.stackTop - 1);
      popFromStack();
      break;
    case OP_CLASS:
      pushOnStack(OBJ_VAL(newClass(READ_STRING())));
      break;
    case OP_GET_PROPERTY: {
      if (!IS_INSTANCE(peekStack(0))) {
        runtimeError(&vm, "Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peekStack(0));
      ObjString *name = READ_STRING();

      int slot;

      if (classFieldSlot(instance->klass, name, &slot)) {
        Value value = instance->fields[slot];

        popFromStack(); // instance
        pushOnStack(value);
        break;
      }

      if (bindMethod(instance->klass, name)) {
        break;
      }

      runtimeError(&vm, "Undefined property '%s'.", name->chars);
      return INTERPRET_RUNTIME_ERROR;
    }
    case OP_SET_PROPERTY: {
      if (!IS_INSTANCE(peekStack(1))) {
        runtimeError(&vm, "Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peekStack(1));
      ObjString *name = READ_STRING();

      int slot;

      if (!classFieldSlot(instance->klass, name, &slot)) {
        runtimeError(&vm, "Undefined field '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      instance->fields[slot] = peekStack(0);

      Value value = popFromStack(); // value
      popFromStack();               // instance
      pushOnStack(value);
      break;
    }
    case OP_FIELD: {
      ObjString *field_name = READ_STRING();

      Value value = peekStack(1);

      if (!IS_CLASS(value)) {
        runtimeError(&vm, "Field declaration outside class.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjClass *klass = AS_CLASS(value);

      Value existing;

      if (tableGet(&klass->fields, field_name, &existing)) {
        runtimeError(&vm, "Duplicate field '%s'.", field_name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      tableSet(&klass->fields, field_name, NUMBER_VAL(klass->fieldCount));

      Value initializer = peekStack(0);

      if (klass->fieldCount >= 256) {
        runtimeError(&vm, "A class can't have more than 256 fields");
      }

      klass->fieldDefaults[klass->fieldCount] = initializer;

      klass->fieldCount++;

      popFromStack(); // initializer

      break;
    }
    case OP_METHOD: {
      // Reads next byte operand as an index in to the constants table
      // Read constant value as a string
      ObjString *method_name = READ_STRING();

      defineMethod(method_name);
      break;
    }
    case OP_INVOKE: {
      ObjString *method = READ_STRING();
      int argCount = READ_BYTE();
      if (!invoke(method, argCount)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = &vm.frames[vm.frameCount - 1];
      break;
    }
    case OP_ARRAY: {
      int count = READ_BYTE();

      ObjArray *array = newArray();

      for (int i = count - 1; i >= 0; i--) {
        writeValueToArrayObj(array, peekStack(i));
      }

      popNFromStack(count);

      pushOnStack(OBJ_VAL(array));

      break;
    }
    case OP_GET_INDEX: {
      Value index = popFromStack();
      Value arrayVal = popFromStack();

      if (!IS_ARRAY(arrayVal)) {
        runtimeError(&vm, "Can only index arrays.");
        return INTERPRET_RUNTIME_ERROR;
      }

      if (!IS_NUMBER(index)) {
        runtimeError(&vm, "Array index must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjArray *array = AS_ARRAY(arrayVal);

      int i = AS_NUMBER(index);

      if (i < 0 || i >= array->count) {
        runtimeError(&vm, "Array index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      pushOnStack(array->values[i]);

      break;
    }
    case OP_SET_INDEX: {
      Value value = popFromStack();
      Value index = popFromStack();
      Value arrayValue = popFromStack();

      if (!IS_ARRAY(arrayValue)) {
        runtimeError(&vm, "Can only index arrays.");
        return INTERPRET_RUNTIME_ERROR;
      }

      if (!IS_NUMBER(index)) {
        runtimeError(&vm, "Array index must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjArray *array = AS_ARRAY(arrayValue);

      int i = AS_NUMBER(index);

      if (i < 0 || i >= array->count) {
        runtimeError(&vm, "Array index out of bounds.");
        return INTERPRET_RUNTIME_ERROR;
      }

      array->values[i] = value;

      pushOnStack(value);

      break;
    }
    case OP_CLOSE_BLOCK_EXPR: {
      int locals = READ_BYTE();

      Value result = popFromStack();

      closeUpvalues(vm.stackTop - locals);

      vm.stackTop -= locals;

      pushOnStack(result);

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
