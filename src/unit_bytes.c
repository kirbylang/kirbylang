#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "unit_bytes.h"
#include "version.h"

static const char MAGIC[4] = {'K', 'R', 'B', 'C'};

// The smallest a function can be: its fixed fields and three empty lists.
#define MIN_FUNCTION_BYTES (4 + 4 + 1 + 4 + 4 + 4 + 4 + 4)

// One code byte takes a byte, and its line takes four.
#define CODE_BYTES_EACH (1 + 4)

// A constant takes at least its kind byte.
#define MIN_CONSTANT_BYTES 1

// An upvalue takes `isLocal` and `index`.
#define UPVALUE_BYTES_EACH 2

#define FLAG_STATIC 0x01
#define FLAG_PUBLIC 0x02

// Encoding

static void putU8(StrBuf *out, uint8_t value) {
  sb_append_bytes(out, &value, 1);
}

static void putU16(StrBuf *out, uint16_t value) {
  uint8_t bytes[2] = {(uint8_t)(value & 0xff), (uint8_t)(value >> 8)};
  sb_append_bytes(out, bytes, sizeof bytes);
}

static void putU32(StrBuf *out, uint32_t value) {
  uint8_t bytes[4];
  for (int i = 0; i < 4; i++) {
    bytes[i] = (uint8_t)((value >> (8 * i)) & 0xff);
  }
  sb_append_bytes(out, bytes, sizeof bytes);
}

static void putI32(StrBuf *out, int32_t value) { putU32(out, (uint32_t)value); }

static void putDouble(StrBuf *out, double value) {
  uint64_t bits;
  memcpy(&bits, &value, sizeof bits);

  uint8_t bytes[8];
  for (int i = 0; i < 8; i++) {
    bytes[i] = (uint8_t)((bits >> (8 * i)) & 0xff);
  }
  sb_append_bytes(out, bytes, sizeof bytes);
}

static void encodeConstant(StrBuf *out, const CompiledConst *constant) {
  putU8(out, (uint8_t)constant->kind);

  switch (constant->kind) {
  case CONST_NUMBER:
    putDouble(out, constant->as.number);
    break;
  case CONST_BOOL:
    putU8(out, constant->as.boolean ? 1 : 0);
    break;
  case CONST_NIL:
    break;
  case CONST_STRING:
    putI32(out, constant->as.string.offset);
    putI32(out, constant->as.string.length);
    break;
  case CONST_FUNCTION:
    putI32(out, constant->as.functionIndex);
    break;
  }
}

static void encodeFunction(StrBuf *out, const CompiledFn *fn) {
  putI32(out, fn->arity);
  putI32(out, fn->upvalueCount);
  putU8(out, (uint8_t)((fn->isStatic ? FLAG_STATIC : 0) |
                       (fn->isPublic ? FLAG_PUBLIC : 0)));
  putI32(out, fn->nameOffset);
  putI32(out, fn->nameLength);

  putU32(out, (uint32_t)fn->codeCount);
  if (fn->codeCount > 0) {
    sb_append_bytes(out, fn->code, (size_t)fn->codeCount);
  }
  for (int i = 0; i < fn->codeCount; i++) {
    putI32(out, fn->codeLines[i]);
  }

  putU32(out, (uint32_t)fn->constantCount);
  for (int i = 0; i < fn->constantCount; i++) {
    encodeConstant(out, &fn->constants[i]);
  }

  putU32(out, (uint32_t)fn->upvalueDescCount);
  for (int i = 0; i < fn->upvalueDescCount; i++) {
    putU8(out, fn->upvalues[i].isLocal ? 1 : 0);
    putU8(out, fn->upvalues[i].index);
  }
}

void unitEncode(const CompiledUnit *unit, StrBuf *out) {
  sb_append_bytes(out, MAGIC, sizeof MAGIC);
  putU16(out, UNIT_BYTES_FORMAT_VERSION);

  size_t versionLength = strlen(KIRBY_VERSION);
  putU8(out, (uint8_t)versionLength);
  sb_append_bytes(out, KIRBY_VERSION, versionLength);

  putU32(out, (uint32_t)unit->strings.arenaLen);
  if (unit->strings.arenaLen > 0) {
    sb_append_bytes(out, unit->strings.arena, (size_t)unit->strings.arenaLen);
  }

  putU32(out, (uint32_t)unit->functionCount);
  for (int i = 0; i < unit->functionCount; i++) {
    encodeFunction(out, &unit->functions[i]);
  }
}

// Decoding

/**
 * Reads from a block of bytes. The first problem it finds is kept in `status`.
 * After a problem every read returns 0, so a caller can read a run of fields
 * and check `status` once.
 */
typedef struct {
  const uint8_t *data;
  size_t length;
  size_t position;
  UnitDecodeStatus status;
} Reader;

static void fail(Reader *reader, UnitDecodeStatus status) {
  if (reader->status == UNIT_DECODE_OK) {
    reader->status = status;
  }
}

static const uint8_t *take(Reader *reader, size_t count) {
  if (reader->status != UNIT_DECODE_OK) {
    return NULL;
  }

  if (count > reader->length - reader->position) {
    fail(reader, UNIT_DECODE_TRUNCATED);
    return NULL;
  }

  const uint8_t *bytes = reader->data + reader->position;
  reader->position += count;
  return bytes;
}

static uint8_t readU8(Reader *reader) {
  const uint8_t *bytes = take(reader, 1);
  return bytes == NULL ? 0 : bytes[0];
}

static uint16_t readU16(Reader *reader) {
  const uint8_t *bytes = take(reader, 2);
  return bytes == NULL ? 0 : (uint16_t)(bytes[0] | (bytes[1] << 8));
}

static uint32_t readU32(Reader *reader) {
  const uint8_t *bytes = take(reader, 4);

  if (bytes == NULL) {
    return 0;
  }

  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int32_t readI32(Reader *reader) { return (int32_t)readU32(reader); }

static double readDouble(Reader *reader) {
  const uint8_t *bytes = take(reader, 8);

  if (bytes == NULL) {
    return 0;
  }

  uint64_t bits = 0;
  for (int i = 0; i < 8; i++) {
    bits |= (uint64_t)bytes[i] << (8 * i);
  }

  double value;
  memcpy(&value, &bits, sizeof value);
  return value;
}

/**
 * True if `count` items of at least `bytesEach` bytes can still be in the data.
 * Checked before allocating, so a damaged count can't ask for a huge block.
 */
static bool countFits(Reader *reader, uint32_t count, size_t bytesEach) {
  if (reader->status != UNIT_DECODE_OK) {
    return false;
  }

  if (count > INT_MAX) {
    fail(reader, UNIT_DECODE_INVALID);
    return false;
  }

  if (count > (reader->length - reader->position) / bytesEach) {
    fail(reader, UNIT_DECODE_TRUNCATED);
    return false;
  }

  return true;
}

static bool rangeInside(int32_t offset, int32_t length, int total) {
  return offset >= 0 && length >= 0 && (int64_t)offset + length <= total;
}

static void *allocateArray(uint32_t count, size_t size) {
  if (count == 0) {
    return NULL;
  }

  void *array = calloc(count, size);

  if (array == NULL) {
    fprintf(stderr, "calloc failed in unitDecode\n");
    exit(EXIT_CODE_OS_ERR);
  }

  return array;
}

static void readConstant(Reader *reader, const CompiledUnit *unit,
                         CompiledConst *constant) {
  uint8_t kind = readU8(reader);

  switch (kind) {
  case CONST_NUMBER:
    constant->kind = CONST_NUMBER;
    constant->as.number = readDouble(reader);
    break;
  case CONST_BOOL: {
    uint8_t value = readU8(reader);

    if (value > 1) {
      fail(reader, UNIT_DECODE_INVALID);
    }

    constant->kind = CONST_BOOL;
    constant->as.boolean = value == 1;
    break;
  }
  case CONST_NIL:
    constant->kind = CONST_NIL;
    break;
  case CONST_STRING:
    constant->kind = CONST_STRING;
    constant->as.string.offset = readI32(reader);
    constant->as.string.length = readI32(reader);

    if (reader->status == UNIT_DECODE_OK &&
        !rangeInside(constant->as.string.offset, constant->as.string.length,
                     unit->strings.arenaLen)) {
      fail(reader, UNIT_DECODE_INVALID);
    }
    break;
  case CONST_FUNCTION:
    constant->kind = CONST_FUNCTION;
    constant->as.functionIndex = readI32(reader);

    if (reader->status == UNIT_DECODE_OK &&
        (constant->as.functionIndex < 0 ||
         constant->as.functionIndex >= unit->functionCount)) {
      fail(reader, UNIT_DECODE_INVALID);
    }
    break;
  default:
    fail(reader, UNIT_DECODE_INVALID);
    break;
  }
}

static void readFunction(Reader *reader, const CompiledUnit *unit,
                         CompiledFn *fn) {
  fn->arity = readI32(reader);
  fn->upvalueCount = readI32(reader);

  uint8_t flags = readU8(reader);
  fn->isStatic = (flags & FLAG_STATIC) != 0;
  fn->isPublic = (flags & FLAG_PUBLIC) != 0;

  fn->nameOffset = readI32(reader);
  fn->nameLength = readI32(reader);

  if (reader->status != UNIT_DECODE_OK) {
    return;
  }

  if ((flags & ~(FLAG_STATIC | FLAG_PUBLIC)) != 0 || fn->arity < 0 ||
      fn->upvalueCount < 0 ||
      (fn->nameLength >= 0 &&
       !rangeInside(fn->nameOffset, fn->nameLength, unit->strings.arenaLen))) {
    fail(reader, UNIT_DECODE_INVALID);
    return;
  }

  uint32_t codeCount = readU32(reader);

  if (!countFits(reader, codeCount, CODE_BYTES_EACH)) {
    return;
  }

  fn->code = (uint8_t *)allocateArray(codeCount, sizeof(uint8_t));
  fn->codeLines = (int *)allocateArray(codeCount, sizeof(int));
  fn->codeCount = (int)codeCount;
  fn->codeCapacity = (int)codeCount;

  const uint8_t *code = take(reader, codeCount);

  if (code != NULL && codeCount > 0) {
    memcpy(fn->code, code, codeCount);
  }

  for (uint32_t i = 0; i < codeCount; i++) {
    fn->codeLines[i] = readI32(reader);
  }

  uint32_t constantCount = readU32(reader);

  if (!countFits(reader, constantCount, MIN_CONSTANT_BYTES)) {
    return;
  }

  fn->constants =
      (CompiledConst *)allocateArray(constantCount, sizeof(CompiledConst));
  fn->constantCount = (int)constantCount;
  fn->constantCapacity = (int)constantCount;

  for (uint32_t i = 0; i < constantCount; i++) {
    readConstant(reader, unit, &fn->constants[i]);

    if (reader->status != UNIT_DECODE_OK) {
      return;
    }
  }

  uint32_t upvalueCount = readU32(reader);

  if (!countFits(reader, upvalueCount, UPVALUE_BYTES_EACH)) {
    return;
  }

  fn->upvalues =
      (CompiledUpvalue *)allocateArray(upvalueCount, sizeof(CompiledUpvalue));
  fn->upvalueDescCount = (int)upvalueCount;
  fn->upvalueDescCapacity = (int)upvalueCount;

  for (uint32_t i = 0; i < upvalueCount; i++) {
    uint8_t isLocal = readU8(reader);

    if (isLocal > 1) {
      fail(reader, UNIT_DECODE_INVALID);
      return;
    }

    fn->upvalues[i].isLocal = isLocal == 1;
    fn->upvalues[i].index = readU8(reader);
  }
}

static CompiledUnit *rejectUnit(CompiledUnit *unit, UnitDecodeStatus reason,
                                UnitDecodeStatus *status) {
  if (unit != NULL) {
    freeCompiledUnit(unit);
    free(unit);
  }

  *status = reason;
  return NULL;
}

CompiledUnit *unitDecode(const uint8_t *bytes, size_t length,
                         UnitDecodeStatus *status) {
  Reader reader = {bytes, length, 0, UNIT_DECODE_OK};

  const uint8_t *magic = take(&reader, sizeof MAGIC);

  if (magic != NULL && memcmp(magic, MAGIC, sizeof MAGIC) != 0) {
    fail(&reader, UNIT_DECODE_BAD_MAGIC);
  }

  uint16_t formatVersion = readU16(&reader);

  if (reader.status == UNIT_DECODE_OK &&
      formatVersion != UNIT_BYTES_FORMAT_VERSION) {
    fail(&reader, UNIT_DECODE_BAD_FORMAT_VERSION);
  }

  uint8_t versionLength = readU8(&reader);
  const uint8_t *version = take(&reader, versionLength);

  if (version != NULL && (versionLength != strlen(KIRBY_VERSION) ||
                          memcmp(version, KIRBY_VERSION, versionLength) != 0)) {
    fail(&reader, UNIT_DECODE_VERSION_MISMATCH);
  }

  uint32_t blobLength = readU32(&reader);

  if (reader.status == UNIT_DECODE_OK && blobLength > INT_MAX) {
    fail(&reader, UNIT_DECODE_INVALID);
  }

  const uint8_t *blob = take(&reader, blobLength);
  uint32_t functionCount = readU32(&reader);

  if (reader.status == UNIT_DECODE_OK && functionCount == 0) {
    fail(&reader, UNIT_DECODE_INVALID);
  }

  if (!countFits(&reader, functionCount, MIN_FUNCTION_BYTES)) {
    return rejectUnit(NULL, reader.status, status);
  }

  CompiledUnit *unit = (CompiledUnit *)malloc(sizeof(CompiledUnit));

  if (unit == NULL) {
    fprintf(stderr, "malloc failed in unitDecode\n");
    exit(EXIT_CODE_OS_ERR);
  }

  cuInit(unit);

  if (blobLength > 0) {
    unit->strings.arena = (char *)allocateArray(blobLength, sizeof(char));
    memcpy(unit->strings.arena, blob, blobLength);
    unit->strings.arenaLen = (int)blobLength;
    unit->strings.arenaCapacity = (int)blobLength;
  }

  unit->functions =
      (CompiledFn *)allocateArray(functionCount, sizeof(CompiledFn));
  unit->functionCount = (int)functionCount;
  unit->functionCapacity = (int)functionCount;

  for (uint32_t i = 0; i < functionCount; i++) {
    readFunction(&reader, unit, &unit->functions[i]);

    if (reader.status != UNIT_DECODE_OK) {
      return rejectUnit(unit, reader.status, status);
    }
  }

  if (reader.position != reader.length) {
    return rejectUnit(unit, UNIT_DECODE_INVALID, status);
  }

  *status = UNIT_DECODE_OK;
  return unit;
}

const char *unitDecodeStatusMessage(UnitDecodeStatus status) {
  switch (status) {
  case UNIT_DECODE_OK:
    return "no problem";
  case UNIT_DECODE_TRUNCATED:
    return "the data ends too soon";
  case UNIT_DECODE_BAD_MAGIC:
    return "this is not a compiled Kirby program";
  case UNIT_DECODE_BAD_FORMAT_VERSION:
    return "it was made with a different file layout";
  case UNIT_DECODE_VERSION_MISMATCH:
    return "it was made by a different version of Kirby";
  case UNIT_DECODE_INVALID:
    return "the data is damaged";
  }

  return "unknown problem";
}
