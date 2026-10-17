#ifndef kirby_number_kind_h
#define kirby_number_kind_h

#include <stdbool.h>
#include <stdint.h>

// The supported number types in Kirby
typedef enum {
  NUMBER_KIND_I64,
  NUMBER_KIND_I32,
  NUMBER_KIND_I16,
  NUMBER_KIND_I8,
  NUMBER_KIND_U64,
  NUMBER_KIND_U32,
  NUMBER_KIND_U16,
  NUMBER_KIND_U8,
  NUMBER_KIND_F64,
  NUMBER_KIND_F32,
  NUMBER_KIND_COUNT,
} NumberKind;

/**
 * The value of a number of any kind, held in 64 bits. Signed integers are
 * widened by copying their sign bit into the extra bits, so -3 stays -3.
 * Unsigned integers are widened with zeros. Floats are held as an f64.
 */
typedef union {
  int64_t i64;
  uint64_t u64;
  double f64;
} NumberPayload;

typedef struct {
  const char *name;
  int size;      // in bytes
  bool isSigned; // can hold negative numbers, which includes floats
  bool isFloat;
  bool supported; // usable in programs
} NumberKindInfo;

const NumberKindInfo *numberKindInfo(NumberKind kind);

double numberToDouble(NumberKind kind, NumberPayload payload);

#endif
