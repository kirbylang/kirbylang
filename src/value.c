#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Appends a character to `buffer`, keeping room for the '\0'.
static void appendChar(char *buffer, size_t size, size_t *length, char c) {
  if (*length + 1 < size)
    buffer[(*length)++] = c;
}

void formatNumber(double value, char *buffer, size_t size) {
  if (isnan(value)) {
    snprintf(buffer, size, "nan");
    return;
  }

  if (isinf(value)) {
    snprintf(buffer, size, value < 0 ? "-inf" : "inf");
    return;
  }

  if (value == 0) {
    snprintf(buffer, size, signbit(value) ? "-0" : "0");
    return;
  }

  // Find the fewest significant digits that read back as the same number.
  // 17 always do.
  double magnitude = fabs(value);
  char scientific[32];

  for (int precision = 0; precision <= 16; precision++) {
    snprintf(scientific, sizeof(scientific), "%.*e", precision, magnitude);

    if (strtod(scientific, NULL) == magnitude)
      break;
  }

  // Split "d.ddde+XX" into its digits and exponent.
  char digits[20];
  int digitCount = 0;
  const char *c = scientific;

  for (; *c != 'e'; c++) {
    if (*c != '.')
      digits[digitCount++] = *c;
  }

  int exponent = atoi(c + 1);

  while (digitCount > 1 && digits[digitCount - 1] == '0')
    digitCount--;

  // The number is 0.[digits] x 10^point.
  int point = exponent + 1;
  size_t length = 0;

  if (value < 0)
    appendChar(buffer, size, &length, '-');

  if (digitCount <= point && point <= 21) {
    // A whole number: the digits, then zeros.
    for (int i = 0; i < digitCount; i++)
      appendChar(buffer, size, &length, digits[i]);
    for (int i = digitCount; i < point; i++)
      appendChar(buffer, size, &length, '0');
  } else if (0 < point && point <= 21) {
    // The point falls inside the digits.
    for (int i = 0; i < digitCount; i++) {
      if (i == point)
        appendChar(buffer, size, &length, '.');
      appendChar(buffer, size, &length, digits[i]);
    }
  } else if (-6 < point && point <= 0) {
    // A small number: "0.", zeros, then the digits.
    appendChar(buffer, size, &length, '0');
    appendChar(buffer, size, &length, '.');
    for (int i = point; i < 0; i++)
      appendChar(buffer, size, &length, '0');
    for (int i = 0; i < digitCount; i++)
      appendChar(buffer, size, &length, digits[i]);
  } else {
    // Exponent form: d.ddd e+X
    appendChar(buffer, size, &length, digits[0]);
    if (digitCount > 1) {
      appendChar(buffer, size, &length, '.');
      for (int i = 1; i < digitCount; i++)
        appendChar(buffer, size, &length, digits[i]);
    }

    char exponentText[16];
    snprintf(exponentText, sizeof(exponentText), "e%c%d",
             point - 1 < 0 ? '-' : '+', abs(point - 1));

    for (const char *e = exponentText; *e != '\0'; e++)
      appendChar(buffer, size, &length, *e);
  }

  if (size > 0)
    buffer[length] = '\0';
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
    formatNumber(AS_NUMBER(value), buffer, size);
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
