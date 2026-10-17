#ifndef kirby_number_kind_h
#define kirby_number_kind_h

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

typedef struct {
  const char *name;
} NumberKindInfo;

const NumberKindInfo *numberKindInfo(NumberKind kind);

#endif
