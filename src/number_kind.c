#include "number_kind.h"

static const NumberKindInfo infos[] = {
    [NUMBER_KIND_I64] = {.name = "i64", .size = 8, .isSigned = true},
    [NUMBER_KIND_I32] = {.name = "i32", .size = 4, .isSigned = true},
    [NUMBER_KIND_I16] = {.name = "i16", .size = 2, .isSigned = true},
    [NUMBER_KIND_I8] = {.name = "i8", .size = 1, .isSigned = true},
    [NUMBER_KIND_U64] = {.name = "u64", .size = 8},
    [NUMBER_KIND_U32] = {.name = "u32", .size = 4},
    [NUMBER_KIND_U16] = {.name = "u16", .size = 2},
    [NUMBER_KIND_U8] = {.name = "u8", .size = 1, .supported = true},
    [NUMBER_KIND_F64] = {.name = "f64",
                         .size = 8,
                         .isSigned = true,
                         .isFloat = true,
                         .supported = true},
    [NUMBER_KIND_F32] = {.name = "f32",
                         .size = 4,
                         .isSigned = true,
                         .isFloat = true},
};

_Static_assert(sizeof(infos) / sizeof(infos[0]) == NUMBER_KIND_COUNT,
               "every NumberKind needs a row");

const NumberKindInfo *numberKindInfo(NumberKind kind) { return &infos[kind]; }

double numberToDouble(NumberKind kind, NumberPayload payload) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat)
    return payload.f64;
  if (info->isSigned)
    return (double)payload.i64;
  return (double)payload.u64;
}
