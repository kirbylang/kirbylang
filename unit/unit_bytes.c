// Asserts must run in every build type.
#undef NDEBUG

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../src/compiled_unit.h"
#include "../src/strbuf.h"
#include "../src/unit_bytes.h"
#include "../src/version.h"

// Where fields start in the encoded bytes: the magic (4), the format version
// (2), the Kirby version (1 + its text), then the string blob (4 + its bytes)
// and the function count.
#define VERSION_LENGTH_OFFSET 6
#define VERSION_TEXT_OFFSET 7

static size_t blobLengthOffset(void) {
  return VERSION_TEXT_OFFSET + strlen(KIRBY_VERSION);
}

static size_t functionCountOffset(const CompiledUnit *unit) {
  return blobLengthOffset() + 4 + (size_t)unit->strings.arenaLen;
}

// The first function's code count follows the function count and the
// function's arity, upvalue count, flags, name offset and name length.
static size_t firstCodeCountOffset(const CompiledUnit *unit) {
  return functionCountOffset(unit) + 4 + 4 + 4 + 1 + 4 + 4;
}

/**
 * Three functions that use every field and every kind of constant: the script,
 * a named static public function with upvalues, and an empty function.
 */
static CompiledUnit *newSampleUnit(void) {
  CompiledUnit *unit = (CompiledUnit *)malloc(sizeof(CompiledUnit));
  cuInit(unit);

  int hello = cuInternString(unit, "hello", 5);
  int odd = cuInternString(unit, "a\0\xc3\xa9", 4);
  int add = cuInternString(unit, "add", 3);

  int scriptIndex = cuAddFunction(unit);
  int addIndex = cuAddFunction(unit);
  cuAddFunction(unit);

  CompiledFn *script = cuGetFnByIndex(unit, scriptIndex);
  const uint8_t code[] = {0, 1, 2, 255};
  for (int i = 0; i < 4; i++) {
    cuWriteByte(script, code[i], 1 + i * 100);
  }

  cuAddConstant(script, (CompiledConst){.kind = CONST_NUMBER, .as.number = 3.14});
  cuAddConstant(script, (CompiledConst){.kind = CONST_NUMBER, .as.number = -0.0});
  cuAddConstant(script, (CompiledConst){.kind = CONST_NUMBER, .as.number = NAN});
  cuAddConstant(script,
                (CompiledConst){.kind = CONST_NUMBER, .as.number = INFINITY});
  cuAddConstant(script, (CompiledConst){.kind = CONST_BOOL, .as.boolean = true});
  cuAddConstant(script, (CompiledConst){.kind = CONST_BOOL, .as.boolean = false});
  cuAddConstant(script, (CompiledConst){.kind = CONST_NIL});
  cuAddConstant(script, (CompiledConst){.kind = CONST_STRING,
                                        .as.string = {.offset = hello, .length = 5}});
  cuAddConstant(script, (CompiledConst){.kind = CONST_STRING,
                                        .as.string = {.offset = odd, .length = 4}});
  cuAddConstant(script,
                (CompiledConst){.kind = CONST_FUNCTION, .as.functionIndex = addIndex});

  CompiledFn *addFn = cuGetFnByIndex(unit, addIndex);
  addFn->arity = 2;
  addFn->upvalueCount = 2;
  addFn->isStatic = true;
  addFn->isPublic = true;
  addFn->nameOffset = add;
  addFn->nameLength = 3;
  cuAddUpvalue(addFn, true, 0);
  cuAddUpvalue(addFn, false, 255);
  cuWriteByte(addFn, 7, 5);
  cuAddConstant(addFn, (CompiledConst){.kind = CONST_NUMBER, .as.number = 1.0});

  return unit;
}

static void freeUnit(CompiledUnit *unit) {
  freeCompiledUnit(unit);
  free(unit);
}

static void assertSameBits(double a, double b) {
  assert(memcmp(&a, &b, sizeof(double)) == 0);
}

static void assertUnitsEqual(const CompiledUnit *a, const CompiledUnit *b) {
  assert(a->strings.arenaLen == b->strings.arenaLen);
  if (a->strings.arenaLen > 0) {
    assert(memcmp(a->strings.arena, b->strings.arena,
                  (size_t)a->strings.arenaLen) == 0);
  }

  assert(a->functionCount == b->functionCount);

  for (int i = 0; i < a->functionCount; i++) {
    const CompiledFn *x = &a->functions[i];
    const CompiledFn *y = &b->functions[i];

    assert(x->arity == y->arity);
    assert(x->upvalueCount == y->upvalueCount);
    assert(x->isStatic == y->isStatic);
    assert(x->isPublic == y->isPublic);
    assert(x->nameOffset == y->nameOffset);
    assert(x->nameLength == y->nameLength);

    assert(x->codeCount == y->codeCount);
    if (x->codeCount > 0) {
      assert(memcmp(x->code, y->code, (size_t)x->codeCount) == 0);
      assert(memcmp(x->codeLines, y->codeLines,
                    sizeof(int) * (size_t)x->codeCount) == 0);
    }

    assert(x->constantCount == y->constantCount);
    for (int j = 0; j < x->constantCount; j++) {
      const CompiledConst *p = &x->constants[j];
      const CompiledConst *q = &y->constants[j];

      assert(p->kind == q->kind);

      switch (p->kind) {
      case CONST_NUMBER:
        assertSameBits(p->as.number, q->as.number);
        break;
      case CONST_BOOL:
        assert(p->as.boolean == q->as.boolean);
        break;
      case CONST_NIL:
        break;
      case CONST_STRING:
        assert(p->as.string.offset == q->as.string.offset);
        assert(p->as.string.length == q->as.string.length);
        break;
      case CONST_FUNCTION:
        assert(p->as.functionIndex == q->as.functionIndex);
        break;
      }
    }

    assert(x->upvalueDescCount == y->upvalueDescCount);
    for (int j = 0; j < x->upvalueDescCount; j++) {
      assert(x->upvalues[j].isLocal == y->upvalues[j].isLocal);
      assert(x->upvalues[j].index == y->upvalues[j].index);
    }
  }
}

static StrBuf encode(const CompiledUnit *unit) {
  StrBuf bytes;
  sb_init(&bytes);
  unitEncode(unit, &bytes);
  return bytes;
}

static CompiledUnit *decodeWithLength(const StrBuf *bytes, size_t length,
                                      UnitDecodeStatus *status) {
  *status = UNIT_DECODE_OK;
  return unitDecode((const uint8_t *)bytes->data, length, status);
}

static void assertRejected(const StrBuf *bytes, UnitDecodeStatus expected) {
  UnitDecodeStatus status;
  CompiledUnit *decoded = decodeWithLength(bytes, bytes->len, &status);
  assert(decoded == NULL);
  assert(status == expected);
}

static void patchU32(StrBuf *bytes, size_t offset, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    bytes->data[offset + (size_t)i] = (char)((value >> (8 * i)) & 0xff);
  }
}

static void testRoundTripKeepsEverything(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  UnitDecodeStatus status;
  CompiledUnit *decoded = decodeWithLength(&bytes, bytes.len, &status);

  assert(status == UNIT_DECODE_OK);
  assert(decoded != NULL);
  assertUnitsEqual(original, decoded);

  freeUnit(decoded);
  sb_free(&bytes);
  freeUnit(original);
}

static void testEncodingIsStable(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf first = encode(original);

  UnitDecodeStatus status;
  CompiledUnit *decoded = decodeWithLength(&first, first.len, &status);
  StrBuf second = encode(decoded);

  assert(first.len == second.len);
  assert(memcmp(first.data, second.data, first.len) == 0);

  sb_free(&second);
  freeUnit(decoded);
  sb_free(&first);
  freeUnit(original);
}

static void testEveryTruncationIsRejected(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  for (size_t length = 0; length < bytes.len; length++) {
    UnitDecodeStatus status;
    CompiledUnit *decoded = decodeWithLength(&bytes, length, &status);

    assert(decoded == NULL);
    assert(status != UNIT_DECODE_OK);
  }

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsWrongMagic(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  bytes.data[0] = 'X';
  assertRejected(&bytes, UNIT_DECODE_BAD_MAGIC);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsOtherFormatVersion(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  bytes.data[4] = (char)(UNIT_BYTES_FORMAT_VERSION + 1);
  assertRejected(&bytes, UNIT_DECODE_BAD_FORMAT_VERSION);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsOtherKirbyVersion(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  assert((size_t)bytes.data[VERSION_LENGTH_OFFSET] == strlen(KIRBY_VERSION));
  bytes.data[VERSION_TEXT_OFFSET] ^= 1;
  assertRejected(&bytes, UNIT_DECODE_VERSION_MISMATCH);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsBytesAfterTheUnit(void) {
  CompiledUnit *original = newSampleUnit();
  StrBuf bytes = encode(original);

  sb_append_bytes(&bytes, "\0", 1);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsUnitWithNoFunctions(void) {
  CompiledUnit unit;
  cuInit(&unit);
  StrBuf bytes = encode(&unit);

  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
}

static void testRejectsHugeCountsWithoutAllocatingThem(void) {
  CompiledUnit *original = newSampleUnit();

  // Too many for the bytes that are left.
  StrBuf functions = encode(original);
  patchU32(&functions, functionCountOffset(original), 0x7FFFFFFFu);
  assertRejected(&functions, UNIT_DECODE_TRUNCATED);
  sb_free(&functions);

  StrBuf code = encode(original);
  patchU32(&code, firstCodeCountOffset(original), 0x7FFFFFFFu);
  assertRejected(&code, UNIT_DECODE_TRUNCATED);
  sb_free(&code);

  // Too many for a count that is stored in an `int`.
  StrBuf past = encode(original);
  patchU32(&past, functionCountOffset(original), 0xFFFFFFFFu);
  assertRejected(&past, UNIT_DECODE_INVALID);
  sb_free(&past);

  freeUnit(original);
}

static void testRejectsStringConstantOutsideTheBlob(void) {
  CompiledUnit *original = newSampleUnit();
  CompiledFn *script = cuGetFnByIndex(original, 0);
  script->constants[7].as.string.offset = original->strings.arenaLen;
  script->constants[7].as.string.length = 1;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsNegativeStringLength(void) {
  CompiledUnit *original = newSampleUnit();
  CompiledFn *script = cuGetFnByIndex(original, 0);
  script->constants[7].as.string.length = -1;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsFunctionConstantOutOfRange(void) {
  CompiledUnit *original = newSampleUnit();
  CompiledFn *script = cuGetFnByIndex(original, 0);
  script->constants[9].as.functionIndex = original->functionCount;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsFunctionNameOutsideTheBlob(void) {
  CompiledUnit *original = newSampleUnit();
  CompiledFn *addFn = cuGetFnByIndex(original, 1);
  addFn->nameOffset = original->strings.arenaLen;
  addFn->nameLength = 1;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsNegativeArity(void) {
  CompiledUnit *original = newSampleUnit();
  cuGetFnByIndex(original, 1)->arity = -1;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testRejectsUnknownConstantKind(void) {
  CompiledUnit *original = newSampleUnit();
  cuGetFnByIndex(original, 0)->constants[0].kind = (CompiledConstKind)99;

  StrBuf bytes = encode(original);
  assertRejected(&bytes, UNIT_DECODE_INVALID);

  sb_free(&bytes);
  freeUnit(original);
}

static void testEveryStatusHasAMessage(void) {
  for (int status = UNIT_DECODE_OK; status <= UNIT_DECODE_INVALID; status++) {
    const char *message = unitDecodeStatusMessage((UnitDecodeStatus)status);
    assert(message != NULL);
    assert(strlen(message) > 0);
  }
}

int main(void) {
  testRoundTripKeepsEverything();
  testEncodingIsStable();
  testEveryTruncationIsRejected();
  testRejectsWrongMagic();
  testRejectsOtherFormatVersion();
  testRejectsOtherKirbyVersion();
  testRejectsBytesAfterTheUnit();
  testRejectsUnitWithNoFunctions();
  testRejectsHugeCountsWithoutAllocatingThem();
  testRejectsStringConstantOutsideTheBlob();
  testRejectsNegativeStringLength();
  testRejectsFunctionConstantOutOfRange();
  testRejectsFunctionNameOutsideTheBlob();
  testRejectsNegativeArity();
  testRejectsUnknownConstantKind();
  testEveryStatusHasAMessage();
  return 0;
}
