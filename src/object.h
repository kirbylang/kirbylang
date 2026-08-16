#ifndef kirby_object_h
#define kirby_object_h

#include "chunk.h"
#include "common.h"
#include "hashtable.h"
#include "value.h"

typedef struct VM VM;
#include "gc.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRUCT(value) isObjType(value, OBJ_STRUCT)
#define IS_INSTANCE(value) isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define IS_ARRAY(value) isObjType(value, OBJ_ARRAY)

#define AS_STRUCT(value) ((ObjStruct *)AS_OBJ(value))
#define AS_INSTANCE(value) ((ObjInstance *)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod *)AS_OBJ(value))
#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjNative *)AS_OBJ(value))->function)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJ(value))
#define AS_ARRAY(value) ((ObjArray *)AS_OBJ(value))

typedef enum {
  OBJ_STRUCT,
  OBJ_INSTANCE,
  OBJ_BOUND_METHOD,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
  OBJ_ARRAY,
} ObjType;

struct Obj {
  ObjType type;
  bool isMarked;
  struct Obj *next;
};

struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

typedef struct {
  Obj obj;
  int count;
  int capacity;
  Value *values;
} ObjArray;

typedef struct ObjUpvalue {
  Obj obj;
  Value *location;
  Value closed;
  struct ObjUpvalue *next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString *name;
  bool isStatic;
  bool isPublic;
} ObjFunction;

typedef struct {
  Obj obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  int upvalueCount;
  /**
   * The struct this closure is allowed to see the private members of, or NULL
   * for closures compiled outside any `impl` block. Set when the method is
   * bound by OP_METHOD, and inherited by nested closures/lambdas.
   */
  struct ObjStruct *owner;
} ObjClosure;

typedef struct ObjStruct {
  Obj obj;
  ObjString *name;
  Table methods;
  Table fields;
  int fieldCount;
  bool fieldPublic[256];
} ObjStruct;

typedef struct {
  Obj obj;
  ObjStruct *struct_;
  Value *fields;
} ObjInstance;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure *method;
} ObjBoundMethod;

ObjArray *newArray(GC *gc);

/**
 * Allocate a new struct
 */
ObjStruct *newStruct(GC *gc, ObjString *name);

bool structFieldSlot(ObjStruct *struct_, ObjString *name, int *slot);

/**
 * The name of the field occupying `slot`, or NULL if there is no such field.
 */
ObjString *structFieldName(ObjStruct *struct_, int slot);

/**
 * Allocate a new closure
 */
ObjClosure *newClosure(GC *gc, ObjFunction *function);

typedef Value (*NativeFn)(struct VM *vm, int argCount, Value *args);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

/**
 * Allocate a new function
 */
ObjFunction *newFunction(GC *gc);

/**
 * Allocate a new struct instance
 */
ObjInstance *newInstance(GC *gc, ObjStruct *struct_);
ObjNative *newNative(GC *gc, NativeFn function);
ObjString *takeString(GC *gc, char *chars, int length);
ObjString *copyString(GC *gc, const char *chars, int length);

/**
 * Allocate a new closure upvalue
 */
ObjUpvalue *newUpvalue(GC *gc, Value *slot);
ObjBoundMethod *newBoundMethod(GC *gc, Value receiver, ObjClosure *method);

void objectToString(Value value, char *buffer, size_t size);
void objectTypeToString(ObjType type, char *buffer, size_t size);
void printObject(Value value);
void printObjectToErr(Value value);
void writeValueToArrayObj(GC *gc, ObjArray *array, Value value);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
