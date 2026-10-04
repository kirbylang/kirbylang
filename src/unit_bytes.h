#ifndef kirby_unit_bytes_h
#define kirby_unit_bytes_h

#include "compiled_unit.h"
#include "strbuf.h"

/**
 * The byte form of a CompiledUnit.
 *
 * Whole numbers are little-endian and numbers are IEEE-754 doubles, so the
 * bytes read the same on every machine. A unit made by one Kirby version is
 * refused by any other.
 */
#define UNIT_BYTES_FORMAT_VERSION 1

typedef enum {
  UNIT_DECODE_OK,
  UNIT_DECODE_TRUNCATED,         // ends before the data it announces
  UNIT_DECODE_BAD_MAGIC,         // not a compiled unit
  UNIT_DECODE_BAD_FORMAT_VERSION, // made with a different layout
  UNIT_DECODE_VERSION_MISMATCH,  // made by a different Kirby version
  UNIT_DECODE_INVALID,           // an index, offset or count is out of range
} UnitDecodeStatus;

/**
 * Append the byte form of `unit` to `out`.
 */
void unitEncode(const CompiledUnit *unit, StrBuf *out);

/**
 * Rebuild a unit from the bytes made by unitEncode(). `bytes` must hold exactly
 * one unit.
 *
 * Checks that the bytes are well formed, and that every index and offset in
 * them stays inside the unit. It does not check that the bytecode is safe to
 * run, so only decode bytes that Kirby made.
 *
 * Returns a unit the caller owns (free it with freeCompiledUnit() then free()),
 * or NULL with the reason in `status`.
 */
CompiledUnit *unitDecode(const uint8_t *bytes, size_t length,
                         UnitDecodeStatus *status);

/**
 * A short sentence for a status, for error messages.
 */
const char *unitDecodeStatusMessage(UnitDecodeStatus status);

#endif
