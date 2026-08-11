#include <stdio.h>

#include "gc.h"
#include "object.h"
#include "string.h"
#include "value.h"

void initValueArray(ValueArray *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(GC *gc, ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values =
        GROW_ARRAY(gc, Value, array->values, oldCapacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(GC *gc, ValueArray *array) {
  FREE_ARRAY(gc, Value, array->values, array->capacity);
  initValueArray(array);
}

void valueToString(Value value, char *buffer, size_t size) {
  switch (value.type) {
  case VAL_BOOL:
    snprintf(buffer, size, "%s", AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NIL:
    snprintf(buffer, size, "nil");
    break;
  case VAL_NUMBER:
    snprintf(buffer, size, "%f", AS_NUMBER(value));
    break;
  case VAL_OBJ:
    objectToString(value, buffer, size); // signal: handled elsewhere
    break;
  }
}

void valueTypeToString(Value value, char *buffer, size_t size) {
  switch (value.type) {
  case VAL_BOOL:
    snprintf(buffer, size, "bool");
    break;
  case VAL_NIL:
    snprintf(buffer, size, "nil");
    break;
  case VAL_NUMBER:
    snprintf(buffer, size, "number");
    break;
  case VAL_OBJ:
    objectTypeToString(value.as.obj->type, buffer, size);
    break;
  }
}

void printValue(Value value) {
  if (value.type == VAL_OBJ) {
    printObject(value);
    return;
  }

  char buffer[VALUE_STRING_MAX];
  valueToString(value, buffer, sizeof(buffer));

  printf("%s", buffer);
}

void printValueToErr(Value value) {
  if (value.type == VAL_OBJ) {
    printObjectToErr(value);
    return;
  }

  char buffer[VALUE_STRING_MAX];
  valueToString(value, buffer, sizeof(buffer));

  fprintf(stderr, "%s", buffer);
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_OBJ:
    return AS_OBJ(a) == AS_OBJ(b);
  default:
    return false; // Unreachable.
  }
}
