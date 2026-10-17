#include "number_kind.h"

static const NumberKindInfo infos[] = {
    [NUMBER_KIND_I64] = {.name = "i64"},
    [NUMBER_KIND_I32] = {.name = "i32"},
    [NUMBER_KIND_I16] = {.name = "i16"},
    [NUMBER_KIND_I8] = {.name = "i8"},
    [NUMBER_KIND_U64] = {.name = "u64"},
    [NUMBER_KIND_U32] = {.name = "u32"},
    [NUMBER_KIND_U16] = {.name = "u16"},
    [NUMBER_KIND_U8] = {.name = "u8"},
    [NUMBER_KIND_F64] = {.name = "f64"},
    [NUMBER_KIND_F32] = {.name = "f32"},
};

_Static_assert(sizeof(infos) / sizeof(infos[0]) == NUMBER_KIND_COUNT,
               "every NumberKind needs a row");

const NumberKindInfo *numberKindInfo(NumberKind kind) { return &infos[kind]; }
