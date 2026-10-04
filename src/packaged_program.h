#ifndef kirby_packaged_program_h
#define kirby_packaged_program_h

#include "compiled_unit.h"
#include "strbuf.h"

/**
 * A packaged program is a runtime executable with compiled units attached to
 * its end:
 *
 *   [runtime executable][units][trailer]
 *
 * Each unit is its length (4 bytes, little-endian) and then its bytes (see
 * unit_bytes.h). The trailer is 16 bytes: the text "KRBAPP01" and the length of
 * all the units (8 bytes, little-endian). The runtime finds its program by
 * reading the trailer from the end of its own file.
 */
#define PACKAGED_TRAILER_SIZE 16

typedef enum {
  PACKAGED_READ_OK,
  PACKAGED_READ_NO_PROGRAM, // the file has no trailer
  PACKAGED_READ_DAMAGED,    // the trailer asks for more bytes than the file has
  PACKAGED_READ_IO_ERROR,   // the file could not be opened or read
} PackagedReadStatus;

typedef enum {
  PACKAGED_UNIT_OK,
  PACKAGED_UNIT_END,     // no more units
  PACKAGED_UNIT_DAMAGED, // the next unit runs past the end of the payload
} PackagedUnitStatus;

/**
 * Append a unit to a payload that is being built.
 */
void packagedAddUnit(StrBuf *payload, const CompiledUnit *unit);

/**
 * Make `outputPath` from the runtime executable at `runtimePath` with the
 * payload and a trailer attached, and mark it executable.
 *
 * The program is written to a temporary file that replaces `outputPath` only
 * once it is complete.
 *
 * On failure returns false, writes a sentence saying why into `error`, and
 * leaves `outputPath` as it was: an output that already exists keeps its
 * contents, and no file is created if there was none.
 */
bool packagedWrite(const char *runtimePath, const char *outputPath,
                   const StrBuf *payload, char *error, size_t errorSize);

/**
 * Read the payload attached to the executable at `path`.
 *
 * `payload` is always left ready to use, so the caller frees it with sb_free()
 * whatever the status.
 */
PackagedReadStatus packagedRead(const char *path, StrBuf *payload);

/**
 * Take the unit that starts at `*cursor` out of a payload and move the cursor
 * past it. `bytes` points into the payload. Start with a cursor of 0.
 */
PackagedUnitStatus packagedNextUnit(const StrBuf *payload, size_t *cursor,
                                    const uint8_t **bytes, size_t *length);

/**
 * The path of the running executable, or NULL if it can't be found. The caller
 * frees it.
 */
char *packagedSelfPath(void);

#endif
