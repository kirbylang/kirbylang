#include <stdio.h>
#include <string.h>

#include "hashtable.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType)                                         \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;

  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif

  return object;
}

ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method) {
  ObjBoundMethod *bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjStruct *newStruct(ObjString *name) {
  ObjStruct *struct_ = ALLOCATE_OBJ(ObjStruct, OBJ_STRUCT);
  struct_->name = name;
  initTable(&struct_->methods);
  initTable(&struct_->fields);
  struct_->fieldCount = 0;

  for (int i = 0; i < 256; i++) {
    struct_->fieldDefaults[i] = NIL_VAL;
    struct_->fieldPublic[i] = false;
  }

  return struct_;
}

bool structFieldSlot(ObjStruct *struct_, ObjString *name, int *slot) {
  Value value;
  if (!tableGet(&struct_->fields, name, &value))
    return false;
  *slot = (int)AS_NUMBER(value);
  return true;
}

ObjString *structFieldName(ObjStruct *struct_, int slot) {
  for (int i = 0; i < struct_->fields.capacity; i++) {
    Entry *entry = &struct_->fields.entries[i];

    if (entry->key != NULL && (int)AS_NUMBER(entry->value) == slot)
      return entry->key;
  }

  return NULL;
}

ObjClosure *newClosure(ObjFunction *function) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure *closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  closure->owner = NULL;
  return closure;
}

ObjFunction *newFunction(void) {
  ObjFunction *function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  function->isStatic = false;
  function->isPublic = false;
  initChunk(&function->chunk);
  return function;
}

ObjInstance *newInstance(ObjStruct *struct_) {
  ObjInstance *instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->struct_ = struct_;

  if (struct_->fieldCount == 0) {
    instance->fields = NULL;
  } else {
    instance->fields = ALLOCATE(Value, struct_->fieldCount);
    for (int i = 0; i < struct_->fieldCount; i++) {
      instance->fields[i] = struct_->fieldDefaults[i];
    }
  }

  return instance;
}

ObjNative *newNative(NativeFn function) {
  ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}

ObjArray *newArray(void) {
  ObjArray *array = ALLOCATE_OBJ(ObjArray, OBJ_ARRAY);
  array->count = 0;
  array->capacity = 0;
  array->values = NULL;

  return array;
}

void writeValueToArrayObj(ObjArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);

    array->values =
        GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count++] = value;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  pushOnStack(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  popFromStack();

  return string;
}

static uint32_t hashString(const char *key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);

  ObjString *interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    TRACELN("object.copyString() found interned string");
    return interned;
  } else {
    TRACELN("object.copyString() not interned string");
  }

  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash);
}

ObjUpvalue *newUpvalue(Value *slot) {
  ObjUpvalue *upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", function->name->chars);
}

static void printFunctionToErr(ObjFunction *function) {
  if (function->name == NULL) {
    fprintf(stderr, "<script>");
    return;
  }

  fprintf(stderr, "<fn %s>", function->name->chars);
}

void objectToString(Value value, char *buffer, size_t size) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    snprintf(buffer, size, "%s", AS_CSTRING(value));
    break;
  case OBJ_FUNCTION: {
    ObjFunction *function = AS_FUNCTION(value);

    if (function->name == NULL) {
      printf("<script>");
      return;
    }

    snprintf(buffer, size, "<fn %s>", function->name->chars);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure *closure = AS_CLOSURE(value);

    if (closure->function->name == NULL) {
      printf("<script>");
      return;
    }

    snprintf(buffer, size, "<fn %s>", closure->function->name->chars);
    break;
  }
  case OBJ_NATIVE: {
    snprintf(buffer, size, "<fn native>");
    break;
  }
  case OBJ_UPVALUE: {
    snprintf(buffer, size, "upvalue");
    break;
  }
  case OBJ_STRUCT: {
    snprintf(buffer, size, "%s", AS_STRUCT(value)->name->chars);
    break;
  }
  case OBJ_INSTANCE: {
    snprintf(buffer, size, "%s instance",
             AS_INSTANCE(value)->struct_->name->chars);
    break;
  }
  case OBJ_BOUND_METHOD: {
    snprintf(buffer, size, "<fn method %s>",
             AS_BOUND_METHOD(value)->method->function->name->chars);
    break;
  }
  case OBJ_ARRAY: {
    ObjArray *array = AS_ARRAY(value);

    printf("[");

    for (int i = 0; i < array->count; i++) {
      printValue(array->values[i]);

      if (i < array->count - 1) {
        printf(", ");
      }
    }

    printf("]");
    break;
  }
  }
}

void objectTypeToString(ObjType type, char *buffer, size_t size) {
  switch (type) {
  case OBJ_STRING:
    snprintf(buffer, size, "string");
    break;
  case OBJ_FUNCTION: {
    snprintf(buffer, size, "function");
    break;
  }
  case OBJ_CLOSURE: {
    snprintf(buffer, size, "function");
    break;
  }
  case OBJ_NATIVE: {
    snprintf(buffer, size, "function");
    break;
  }
  case OBJ_UPVALUE: {
    snprintf(buffer, size, "upvalue");
    break;
  }
  case OBJ_STRUCT: {
    snprintf(buffer, size, "struct");
    break;
  }
  case OBJ_INSTANCE: {
    snprintf(buffer, size, "instance");
    break;
  }
  case OBJ_BOUND_METHOD: {
    snprintf(buffer, size, "function");
    break;
  }
  case OBJ_ARRAY: {
    snprintf(buffer, size, "array");
    break;
  }
  }
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRUCT:
    printf("%s", AS_STRUCT(value)->name->chars);
    break;
  case OBJ_INSTANCE:
    printf("%s instance", AS_INSTANCE(value)->struct_->name->chars);
    break;
  case OBJ_BOUND_METHOD:
    printFunction(AS_BOUND_METHOD(value)->method->function);
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_NATIVE:
    printf("<native fn>");
    break;
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  case OBJ_ARRAY: {
    ObjArray *array = AS_ARRAY(value);

    printf("[");

    for (int i = 0; i < array->count; i++) {
      printValue(array->values[i]);

      if (i < array->count - 1) {
        printf(", ");
      }
    }

    printf("]");
    break;
  }
  }
}

void printObjectToErr(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    fprintf(stderr, "%s", AS_CSTRING(value));
    break;
  case OBJ_FUNCTION:
    if (AS_FUNCTION(value)->name == NULL) {
      fprintf(stderr, "<script>");
      return;
    }

    fprintf(stderr, "<fn %s>", AS_FUNCTION(value)->name->chars);
    break;
  case OBJ_CLOSURE: {
    ObjClosure *closure = AS_CLOSURE(value);

    printFunctionToErr(closure->function);
    break;
  }
  case OBJ_NATIVE:
    fprintf(stderr, "<fn native>");
    break;
  case OBJ_UPVALUE:
    fprintf(stderr, "upvalue");
    break;
  case OBJ_STRUCT:
    fprintf(stderr, "%s", AS_STRUCT(value)->name->chars);
    break;
  case OBJ_INSTANCE: {
    char buf[512];
    objectToString(value, buf, 1024);
    fprintf(stderr, "%s instance", AS_INSTANCE(value)->struct_->name->chars);
    break;
  }
  case OBJ_BOUND_METHOD:
    fprintf(stderr, "<fn method %s>",
            AS_BOUND_METHOD(value)->method->function->name->chars);
    break;
  case OBJ_ARRAY:
    fprintf(stderr, "<array size=%d>", AS_ARRAY(value)->count);
    break;
  }
}
