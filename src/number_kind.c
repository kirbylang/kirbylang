#include "number_kind.h"

#include <string.h>

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

int64_t numberKindMin(NumberKind kind) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (!info->isSigned)
    return 0;

  return (int64_t)(UINT64_MAX << (info->size * 8 - 1));
}

uint64_t numberKindMax(NumberKind kind) {
  const NumberKindInfo *info = numberKindInfo(kind);
  int bits = info->size * 8 - (info->isSigned ? 1 : 0);

  return bits == 64 ? UINT64_MAX : ((uint64_t)1 << bits) - 1;
}

bool numberKindFromName(const char *name, int length, NumberKind *kind) {
  for (int i = 0; i < NUMBER_KIND_COUNT; i++) {
    if (!infos[i].supported)
      continue;

    if ((int)strlen(infos[i].name) == length &&
        memcmp(infos[i].name, name, (size_t)length) == 0) {
      *kind = (NumberKind)i;

      return true;
    }
  }

  return false;
}

double numberToDouble(NumberKind kind, NumberPayload payload) {
  const NumberKindInfo *info = numberKindInfo(kind);

  if (info->isFloat)
    return payload.f64;
  if (info->isSigned)
    return (double)payload.i64;
  return (double)payload.u64;
}
