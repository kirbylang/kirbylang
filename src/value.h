#ifndef kirby_value_h
#define kirby_value_h

#include "common.h"
#include "number_kind.h"

#define VALUE_STRING_MAX 128

/**
 * Forward declaration of the GC struct.
 *
 * gc.h uses this file. This avoid circular dependencies.
 */
struct GC;

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum { VAL_BOOL, VAL_NIL, VAL_NUMBER, VAL_OBJ } ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    struct {
      NumberKind kind;
      NumberPayload as;
    } number;
    Obj *obj;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_NUMBER_F64(value)                                                   \
  (IS_NUMBER(value) && (value).as.number.kind == NUMBER_KIND_F64)
#define IS_NUMBER_U8(value)                                                    \
  (IS_NUMBER(value) && (value).as.number.kind == NUMBER_KIND_U8)
#define IS_OBJ(value) ((value).type == VAL_OBJ)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER_F64(value) ((value).as.number.as.f64)
#define AS_NUMBER_U8(value) ((uint8_t)(value).as.number.as.u64)
#define AS_OBJ(value) ((value).as.obj)

#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.boolean = false}})
#define NUMBER_F64_VAL(value)                                                  \
  ((Value){.type = VAL_NUMBER,                                                 \
           .as.number = {.kind = NUMBER_KIND_F64, .as = {.f64 = value}}})
#define NUMBER_I64_VAL(value)                                                  \
  ((Value){.type = VAL_NUMBER,                                                 \
           .as.number = {.kind = NUMBER_KIND_I64, .as = {.i64 = (value)}}})
#define NUMBER_U8_VAL(value)                                                   \
  ((Value){.type = VAL_NUMBER,                                                 \
           .as.number = {.kind = NUMBER_KIND_U8,                               \
                         .as = {.u64 = (uint8_t)(value)}}})
#define OBJ_VAL(object) ((Value){VAL_OBJ, {.obj = (Obj *)object}})

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

Value numberValue(NumberKind kind, NumberPayload payload);
bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray *array);
void writeValueArray(struct GC *gc, ValueArray *array, Value value);
void freeValueArray(struct GC *gc, ValueArray *array);

/**
 * Room for any number formatted by formatNumberF64, including the '\0'.
 */
#define NUMBER_STRING_MAX 32

/**
 * Formats a number with as many digits as it needs and no more: the fewest
 * digits that read back as exactly the same number. Numbers from 1e-6 up to
 * 1e21 are written in plain decimal form, others in exponent form, e.g. `10`,
 * `123.456`, `0.000025`, `1e-7`, `1e+21`.
 */
void formatNumberF64(double value, char *buffer, size_t size);
void formatNumber(NumberKind kind, NumberPayload payload, char *buffer,
                  size_t size);

void valueToString(Value value, char *buffer, size_t size);
void valueTypeToString(Value value, char *buffer, size_t size);
void printValue(Value value);
void printValueToErr(Value value);

#endif
