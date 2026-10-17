#include "sys/stat.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ast.h"
#include "asserts.h"
#include "common.h"
#include "debug.h"
#include "gc.h"
#include "loader.h"
#include "native.h"
#include "object.h"
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
GC gcInstance;

/**
 * Mark the VM's roots in the garbage collector
 */
static void markVMRoots(GC *gc, void *ctx) {
  (void)ctx;

  for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(gc, *slot);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    markObject(gc, (Obj *)vm.frames[i].closure);
  }

  for (ObjUpvalue *upvalue = vm.openUpvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    markObject(gc, (Obj *)upvalue);
  }

  markTable(gc, &vm.globals);
}

InterpretResult interpretFunction(ObjFunction *function) {
  TRACELN("vm.interpretFunction()");

  if (function == NULL)
    return INTERPRET_COMPILE_ERROR;

  pushOnStack(OBJ_VAL(function));
  ObjClosure *closure = newClosure(vm.gc, function);
  popFromStack();

  pushOnStack(OBJ_VAL(closure));
  call(closure, 0);

  return run();
}

InterpretResult interpret(CompiledUnit *unit, bool disassembleCode) {
  TRACELN("vm.interpret()");

  ObjFunction *function = loadUnit(&vm, unit, disassembleCode);

  freeCompiledUnit(unit);
  free(unit);

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

  vm.gc = &gcInstance;
  vm.gc->objects = NULL;
  vm.gc->bytesAllocated = 0;
  vm.gc->nextGC = 1024 * 1024;

  vm.gc->grayCount = 0;
  vm.gc->grayCapacity = 0;
  vm.gc->grayStack = NULL;

  initTable(&vm.gc->strings);

  vm.gc->rootMarkerCallback = markVMRoots;
  vm.gc->rootMarkerCtx = NULL;

  initTable(&vm.globals);

  defineAllNatives(&vm);
}

void freeVM(void) {
  TRACELN("vm.freeVM()");
  freeTable(vm.gc, &vm.globals);
  freeTable(vm.gc, &vm.gc->strings);
  freeObjects(vm.gc);
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
  runtimeError(&vm, "Can only call functions and structs.");
  return false;
}

// The struct whose private members the currently executing code may touch, or
// NULL at the top level / inside a plain function.
static ObjStruct *currentOwner(void) {
  if (vm.frameCount == 0)
    return NULL;

  return vm.frames[vm.frameCount - 1].closure->owner;
}

// Visibility is per-struct, not per-instance: a method of struct T may read the
// private fields of any T struct, not only its own `self`.
static bool canAccess(ObjStruct *struct_, bool isPublic) {
  return isPublic || currentOwner() == struct_;
}

static bool invokeFromStruct(ObjStruct *struct_, ObjString *name,
                             int argCount) {
  Value method;
  if (!tableGet(&struct_->methods, name, &method)) {
    runtimeError(&vm, "Undefined property '%s'.", name->chars);
    return false;
  }

  if (AS_CLOSURE(method)->function->isStatic) {
    runtimeError(&vm,
                 "'%s' is a static method. Call it on '%s' instead of on an "
                 "instance.",
                 name->chars, struct_->name->chars);
    return false;
  }

  if (!canAccess(struct_, AS_CLOSURE(method)->function->isPublic)) {
    runtimeError(&vm, "Method '%s' is private to '%s'.", name->chars,
                 struct_->name->chars);
    return false;
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool invokeStatic(ObjStruct *struct_, ObjString *name, int argCount) {
  Value method;
  if (!tableGet(&struct_->methods, name, &method)) {
    runtimeError(&vm, "Undefined property '%s'.", name->chars);
    return false;
  }

  if (!AS_CLOSURE(method)->function->isStatic) {
    runtimeError(&vm,
                 "'%s' is an instance method and needs an instance of '%s' to "
                 "be called on.",
                 name->chars, struct_->name->chars);
    return false;
  }

  if (!canAccess(struct_, AS_CLOSURE(method)->function->isPublic)) {
    runtimeError(&vm, "Method '%s' is private to '%s'.", name->chars,
                 struct_->name->chars);
    return false;
  }

  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString *name, int argCount) {
  Value caller = peekStack(argCount);

  if (IS_STRUCT(caller)) {
    return invokeStatic(AS_STRUCT(caller), name, argCount);
  }

  if (!IS_INSTANCE(caller)) {
    runtimeError(&vm, "Only instances have methods.");
    return false;
  }

  ObjInstance *instance = AS_INSTANCE(caller);

  int slot;

  if (structFieldSlot(instance->struct_, name, &slot)) {
    if (!canAccess(instance->struct_, instance->struct_->fieldPublic[slot])) {
      Value method;
      if (tableGet(&instance->struct_->methods, name, &method))
        return invokeFromStruct(instance->struct_, name, argCount);

      runtimeError(&vm, "Field '%s' is private to '%s'.", name->chars,
                   instance->struct_->name->chars);
      return false;
    }

    Value fieldValue = instance->fields[slot];

    // Swap the receiver with the field value where the field value is a
    // function
    //
    // | argCount |          -argCount - 1 |
    // |----------|------------------------|
    // |        0 |                     -1 |
    // |        1 |                     -2 |
    // |        2 |                     -3 |
    // |        3 |                     -4 |
    vm.stackTop[-argCount - 1] = fieldValue;

    return callValue(fieldValue, argCount);
  }

  return invokeFromStruct(instance->struct_, name, argCount);
}

typedef enum {
  // Method is not defined
  BIND_NOT_FOUND,
  BIND_OK,
  // Indicates the method was accessed incorrectly (static methods called on
  // instance)
  BIND_ERROR,
} BindResult;

static BindResult bindMethod(ObjStruct *struct_, ObjString *name) {
  Value method;
  if (!tableGet(&struct_->methods, name, &method)) {
    return BIND_NOT_FOUND;
  }

  if (AS_CLOSURE(method)->function->isStatic) {
    runtimeError(&vm,
                 "'%s' is a static method. Access it on '%s' instead of on an "
                 "instance.",
                 name->chars, struct_->name->chars);
    return BIND_ERROR;
  }

  if (!canAccess(struct_, AS_CLOSURE(method)->function->isPublic)) {
    runtimeError(&vm, "Method '%s' is private to '%s'.", name->chars,
                 struct_->name->chars);
    return BIND_ERROR;
  }

  ObjBoundMethod *bound =
      newBoundMethod(vm.gc, peekStack(0), AS_CLOSURE(method));
  popFromStack();
  pushOnStack(OBJ_VAL(bound));
  return BIND_OK;
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

  ObjUpvalue *createdUpvalue = newUpvalue(vm.gc, local);

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

static bool defineMethod(ObjString *name) {
  Value method = peekStack(0);
  Value target = peekStack(1);

  if (!IS_STRUCT(target)) {
    runtimeError(&vm, "Only structs can have implementations");
    return false;
  }

  ObjStruct *struct_ = AS_STRUCT(target);

  // Membership is recorded per closure rather than per function so that a
  // struct declared inside a function (a fresh ObjStruct on every call) still
  // pairs with the closures created alongside it.
  AS_CLOSURE(method)->owner = struct_;

  tableSet(vm.gc, &struct_->methods, name, method);
  popFromStack();
  return true;
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
  ObjString *b = AS_STRING(peekStack(0));
  ObjString *a = AS_STRING(peekStack(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(vm.gc, char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(vm.gc, chars, length);
  popFromStack();
  popFromStack();
  pushOnStack(OBJ_VAL(result));
}

static const char *checkArrayIndex(double index, int count) {
  if (index != floor(index))
    return "Array index must be a whole number.";
  if (index < 0 || index >= count)
    return "Array index out of bounds.";
  return NULL;
}

static InterpretResult run(void) {
  CallFrame *frame = &vm.frames[vm.frameCount - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT()                                                        \
  (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT()                                                           \
  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(f64ValueType, u8ValueType, op)                               \
  do {                                                                         \
    if (IS_NUMBER_F64(peekStack(0)) && IS_NUMBER_F64(peekStack(1))) {          \
      double b = AS_NUMBER_F64(popFromStack());                                \
      double a = AS_NUMBER_F64(popFromStack());                                \
      pushOnStack(f64ValueType(a op b));                                       \
    } else if (IS_NUMBER_U8(peekStack(0)) && IS_NUMBER_U8(peekStack(1))) {     \
      /* Integer promotion keeps a op b in int range; NUMBER_U8_VAL truncates  \
         it to eight bits, so u8 arithmetic wraps mod 256. Comparisons keep    \
         their unsigned semantics under promotion. */                          \
      uint8_t b = AS_NUMBER_U8(popFromStack());                                \
      uint8_t a = AS_NUMBER_U8(popFromStack());                                \
      pushOnStack(u8ValueType(a op b));                                        \
    } else {                                                                   \
      runtimeError(&vm, "Operands must be two numbers of the same type.");     \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
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
      } else if (IS_NUMBER_F64(operand_b) && IS_NUMBER_F64(operand_a)) {
        double b = AS_NUMBER_F64(popFromStack());
        double a = AS_NUMBER_F64(popFromStack());

        Value result = NUMBER_F64_VAL(a + b);

        pushOnStack(result);
      } else if (IS_NUMBER_U8(operand_b) && IS_NUMBER_U8(operand_a)) {
        // Integer promotion makes a + b an int; NUMBER_U8_VAL truncates it
        // to eight bits, so u8 addition wraps mod 256.
        uint8_t b = AS_NUMBER_U8(popFromStack());
        uint8_t a = AS_NUMBER_U8(popFromStack());

        Value result = NUMBER_U8_VAL(a + b);

        pushOnStack(result);
      } else {
        runtimeError(&vm, "Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_F64_VAL, NUMBER_U8_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_F64_VAL, NUMBER_U8_VAL, *);
      break;
    case OP_DIVIDE: {
      if (IS_NUMBER_U8(peekStack(0))) {
        assertNonZeroU8(&vm, "/", peekStack(0).as.number.as.u8, 1);
      } else {
        assertNonZero(&vm, "/", peekStack(0).as.number.as.f64, 1);
      }
      BINARY_OP(NUMBER_F64_VAL, NUMBER_U8_VAL, /);
      break;
    }
    case OP_MODULO: {
      if (IS_NUMBER_U8(peekStack(0))) {
        // A zero divisor is undefined behavior for C integer modulo, so
        // reject it before the op runs. f64 fmod(, 0) returns NaN instead.
        assertNonZeroU8(&vm, "%", peekStack(0).as.number.as.u8, 1);
      }

      if (IS_NUMBER_F64(peekStack(0)) && IS_NUMBER_F64(peekStack(1))) {
        double b = AS_NUMBER_F64(popFromStack());
        double a = AS_NUMBER_F64(popFromStack());

        Value result = NUMBER_F64_VAL(fmod(a, b));

        pushOnStack(result);
      } else if (IS_NUMBER_U8(peekStack(0)) && IS_NUMBER_U8(peekStack(1))) {
        uint8_t b = AS_NUMBER_U8(popFromStack());
        uint8_t a = AS_NUMBER_U8(popFromStack());

        Value result = NUMBER_U8_VAL(a % b);

        pushOnStack(result);
      } else {
        runtimeError(&vm, "Operands must be two numbers of the same type.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_NOT: {
      Value result = BOOL_VAL(isFalsey(popFromStack()));
      pushOnStack(result);
      break;
    }
    case OP_NEGATE: {
      Value value = peekStack(0);

      if (!IS_NUMBER_F64(value)) {
        runtimeError(&vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      Value negated_value = NUMBER_F64_VAL(-AS_NUMBER_F64(popFromStack()));

      pushOnStack(negated_value);

      break;
    }
    case OP_CAST: {
      uint8_t target = READ_BYTE();
      Value value = popFromStack();

      if (target == NUMBER_U8) {
        // Narrowing conversion. A u8 holds only the integers 0 through 255,
        // so an f64 is range-checked here and rejected if it can't be
        // represented exactly.
        if (IS_NUMBER_U8(value)) {
          pushOnStack(value);
        } else if (IS_NUMBER_F64(value)) {
          double d = value.as.number.as.f64;
          if (d != floor(d) || d < 0.0 || d > 255.0) {
            runtimeError(
                &vm,
                "can't convert f64 %g to u8, expected an integer from 0 to "
                "255.",
                d);
            return INTERPRET_RUNTIME_ERROR;
          }
          pushOnStack(NUMBER_U8_VAL((uint8_t)d));
        } else {
          runtimeError(&vm, "can't cast to u8.");
          return INTERPRET_RUNTIME_ERROR;
        }
      } else if (target == NUMBER_F64) {
        // Widening conversion. Every u8 fits exactly in an f64, so no check
        // is needed.
        if (IS_NUMBER_F64(value)) {
          pushOnStack(value);
        } else if (IS_NUMBER_U8(value)) {
          pushOnStack(NUMBER_F64_VAL(value.as.number.as.u8));
        } else {
          runtimeError(&vm, "can't cast to f64.");
          return INTERPRET_RUNTIME_ERROR;
        }
      } else {
        runtimeError(&vm, "Internal: unknown cast target %d.", target);
        return INTERPRET_RUNTIME_ERROR;
      }
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
      BINARY_OP(BOOL_VAL, BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, BOOL_VAL, <);
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

      bool isNewKey = tableSet(vm.gc, &vm.globals, name, value);

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
      tableSet(vm.gc, &vm.globals, name, value);
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
      ObjClosure *closure = newClosure(vm.gc, function);

      // A lambda or nested function written inside a method keeps that
      // method's access. OP_METHOD overwrites this for the method itself.
      closure->owner = frame->closure->owner;

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
    case OP_STRUCT:
      pushOnStack(OBJ_VAL(newStruct(vm.gc, READ_STRING())));
      break;
    case OP_STRUCT_INIT: {
      int fieldCount = READ_BYTE();
      // [ Point ][ y ][ 20.000000 ][ x ][ 10.000000 ]
      //          ^ pairs
      // ^ pairs - 1
      Value *pairs = vm.stackTop - 2 * fieldCount;
      Value structValue = pairs[-1];

      if (!IS_STRUCT(structValue)) {
        runtimeError(&vm, "Only structs can be initialized with '{}'.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjStruct *struct_ = AS_STRUCT(structValue);

      bool provided[256];
      for (int i = 0; i < struct_->fieldCount; i++) {
        provided[i] = false;
      }

      ObjInstance *instance = newInstance(vm.gc, struct_);

      // Handle any `field: value,` passed
      //
      // i=0:  pairs[2*0]   = pairs[0] = 'y'      pairs[2*0+1] = pairs[1] = 10
      // i=1:  pairs[2*1]   = pairs[2] = 'x'      pairs[2*1+1] = pairs[3] = 30
      // i=2:  pairs[2*2]   = pairs[0] = 'a'      pairs[2*2+1] = pairs[5] = 50
      // i=3:  pairs[2*3]   = pairs[2] = 'b'      pairs[2*3+1] = pairs[7] = 70
      for (int i = 0; i < fieldCount; i++) {

        ObjString *field = AS_STRING(pairs[2 * i]);

        int slot;

        if (!structFieldSlot(struct_, field, &slot)) {
          runtimeError(&vm, "Undefined field '%s'.", field->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!canAccess(struct_, struct_->fieldPublic[slot])) {
          runtimeError(&vm, "Field '%s' is private to '%s'.", field->chars,
                       struct_->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        provided[slot] = true;
        instance->fields[slot] = pairs[2 * i + 1];
      }

      // Validate all fields without default values were passed
      for (int i = 0; i < struct_->fieldCount; i++) {
        if (provided[i])
          continue;

        ObjString *missing = structFieldName(struct_, i);

        runtimeError(&vm, "Missing required field '%s'.",
                     missing == NULL ? "?" : missing->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      // Swap the struct for the instance on the stack
      vm.stackTop = pairs - 1;
      pushOnStack(OBJ_VAL(instance));

      break;
    }
    case OP_GET_PROPERTY: {
      if (IS_STRUCT(peekStack(0))) {
        ObjStruct *struct_ = AS_STRUCT(peekStack(0));
        ObjString *name = READ_STRING();

        Value method;

        if (!tableGet(&struct_->methods, name, &method)) {
          runtimeError(&vm, "Undefined property '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!AS_CLOSURE(method)->function->isStatic) {
          runtimeError(&vm,
                       "'%s' is an instance method and needs an instance of "
                       "'%s' to be called on.",
                       name->chars, struct_->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (!canAccess(struct_, AS_CLOSURE(method)->function->isPublic)) {
          runtimeError(&vm, "Method '%s' is private to '%s'.", name->chars,
                       struct_->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        popFromStack(); // struct
        pushOnStack(method);
        break;
      }

      if (!IS_INSTANCE(peekStack(0))) {
        runtimeError(&vm, "Only instances have properties.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peekStack(0));
      ObjString *name = READ_STRING();

      int slot;

      if (structFieldSlot(instance->struct_, name, &slot)) {
        Value shadowed;

        if (!canAccess(instance->struct_,
                       instance->struct_->fieldPublic[slot]) &&
            !tableGet(&instance->struct_->methods, name, &shadowed)) {
          runtimeError(&vm, "Field '%s' is private to '%s'.", name->chars,
                       instance->struct_->name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        if (canAccess(instance->struct_,
                      instance->struct_->fieldPublic[slot])) {
          Value value = instance->fields[slot];

          popFromStack(); // instance
          pushOnStack(value);
          break;
        }
      }

      switch (bindMethod(instance->struct_, name)) {
      case BIND_OK:
        break;
      case BIND_ERROR:
        return INTERPRET_RUNTIME_ERROR;
      case BIND_NOT_FOUND:
        runtimeError(&vm, "Undefined property '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
    case OP_SET_PROPERTY: {
      if (!IS_INSTANCE(peekStack(1))) {
        runtimeError(&vm, "Only instances have fields.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjInstance *instance = AS_INSTANCE(peekStack(1));
      ObjString *name = READ_STRING();

      int slot;

      if (!structFieldSlot(instance->struct_, name, &slot)) {
        runtimeError(&vm, "Undefined field '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      if (!canAccess(instance->struct_, instance->struct_->fieldPublic[slot])) {
        runtimeError(&vm, "Field '%s' is private to '%s'.", name->chars,
                     instance->struct_->name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      if (!instance->struct_->fieldMutable[slot]) {
        runtimeError(&vm, "Field '%s' is immutable", name->chars);
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
      bool isPublic = READ_BYTE() != 0;
      bool isMutable = READ_BYTE() != 0;

      Value value = peekStack(1);

      if (!IS_STRUCT(value)) {
        runtimeError(&vm, "Field declaration outside struct.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjStruct *struct_ = AS_STRUCT(value);

      Value existing;

      if (tableGet(&struct_->fields, field_name, &existing)) {
        runtimeError(&vm, "Duplicate field '%s'.", field_name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }

      if (struct_->fieldCount >= 256) {
        runtimeError(&vm, "A struct can't have more than 256 fields.");
        return INTERPRET_RUNTIME_ERROR;
      }

      tableSet(vm.gc, &struct_->fields, field_name,
               NUMBER_F64_VAL(struct_->fieldCount));

      struct_->fieldPublic[struct_->fieldCount] = isPublic;
      struct_->fieldMutable[struct_->fieldCount] = isMutable;

      struct_->fieldCount++;

      popFromStack(); // initializer

      break;
    }
    case OP_METHOD: {
      // Reads next byte operand as an index in to the constants table
      // Read constant value as a string
      ObjString *method_name = READ_STRING();

      if (!defineMethod(method_name)) {
        return INTERPRET_RUNTIME_ERROR;
      }

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

      ObjArray *array = newArray(vm.gc);

      pushOnStack(OBJ_VAL(array));

      for (int i = count - 1; i >= 0; i--) {
        // Stack layout right now: [..., elem_{count-1}, ..., elem_0, array]
        // so the i-th element (counting from the end) sits one slot further
        // from the top than it did before `array` was pushed.
        writeValueToArrayObj(vm.gc, array, peekStack(i + 1));
      }

      Value arrayValue = popFromStack();
      popNFromStack(count);

      pushOnStack(arrayValue);

      break;
    }
    case OP_GET_INDEX: {
      Value index = popFromStack();
      Value arrayVal = popFromStack();

      if (!IS_ARRAY(arrayVal)) {
        runtimeError(&vm, "Can only index arrays.");
        return INTERPRET_RUNTIME_ERROR;
      }

      if (!IS_NUMBER_F64(index)) {
        runtimeError(&vm, "Array index must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjArray *array = AS_ARRAY(arrayVal);

      const char *indexError =
          checkArrayIndex(AS_NUMBER_F64(index), array->count);
      if (indexError != NULL) {
        runtimeError(&vm, "%s", indexError);
        return INTERPRET_RUNTIME_ERROR;
      }

      int i = (int)AS_NUMBER_F64(index);

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

      if (!IS_NUMBER_F64(index)) {
        runtimeError(&vm, "Array index must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }

      ObjArray *array = AS_ARRAY(arrayValue);

      const char *indexError =
          checkArrayIndex(AS_NUMBER_F64(index), array->count);
      if (indexError != NULL) {
        runtimeError(&vm, "%s", indexError);
        return INTERPRET_RUNTIME_ERROR;
      }

      int i = (int)AS_NUMBER_F64(index);

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
