#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiled_unit.h"
#include "packaged_program.h"
#include "strbuf.h"
#include "unit_bytes.h"
#include "vm.h"

/*
 * The runtime of a packaged program: the VM without the compiler.
 *
 * Built by `krb build`, a packaged program is this executable with its compiled
 * units attached (see packaged_program.h). This reads them from its own file
 * and runs them in order on one VM, as `krb run` does.
 */

_Noreturn static void die(int exitCode, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(exitCode);
}

static void readPayload(StrBuf *payload) {
  char *self = packagedSelfPath();

  if (self == NULL) {
    die(EXIT_CODE_OS_ERR, "Could not find the file this program runs from.\n");
  }

  PackagedReadStatus status = packagedRead(self, payload);
  free(self);

  switch (status) {
  case PACKAGED_READ_OK:
    return;
  case PACKAGED_READ_NO_PROGRAM:
    die(64, "This is the Kirby runtime. It has no program to run.\n"
            "Make one with: krb build <path> -o <output>\n");
  case PACKAGED_READ_DAMAGED:
    die(EXIT_CODE_OS_ERR, "The program in this file is damaged.\n");
  case PACKAGED_READ_IO_ERROR:
    die(EXIT_CODE_OS_ERR, "Could not read the program in this file.\n");
  }
}

/**
 * Decode every unit before running any, so a damaged program stops before it
 * does anything.
 */
static CompiledUnit **decodeUnits(const StrBuf *payload, int *count) {
  CompiledUnit **units = NULL;
  int capacity = 0;
  size_t cursor = 0;

  *count = 0;

  for (;;) {
    const uint8_t *bytes;
    size_t length;
    PackagedUnitStatus found =
        packagedNextUnit(payload, &cursor, &bytes, &length);

    if (found == PACKAGED_UNIT_END) {
      break;
    }

    if (found == PACKAGED_UNIT_DAMAGED) {
      die(EXIT_CODE_OS_ERR, "The program in this file is damaged.\n");
    }

    UnitDecodeStatus status;
    CompiledUnit *unit = unitDecode(bytes, length, &status);

    if (unit == NULL) {
      die(EXIT_CODE_OS_ERR, "Could not load the program in this file: %s.\n",
          unitDecodeStatusMessage(status));
    }

    if (*count == capacity) {
      capacity = capacity == 0 ? 2 : capacity * 2;
      units = (CompiledUnit **)realloc(units, sizeof(CompiledUnit *) *
                                                  (size_t)capacity);

      if (units == NULL) {
        die(EXIT_CODE_OS_ERR, "realloc failed in decodeUnits\n");
      }
    }

    units[(*count)++] = unit;
  }

  if (*count == 0) {
    die(EXIT_CODE_OS_ERR, "The program in this file is damaged.\n");
  }

  return units;
}

int main(int argc, char *argv[]) {
  StrBuf payload;
  readPayload(&payload);

  int count;
  CompiledUnit **units = decodeUnits(&payload, &count);
  sb_free(&payload);

  initVM(argc, argv);

  for (int i = 0; i < count; i++) {
    bool disassembleCode = i > 0;

    if (interpret(units[i], disassembleCode) == INTERPRET_RUNTIME_ERROR) {
      exit(EXIT_CODE_RUNTIME_ERR);
    }
  }

  free(units);
  freeVM();
  return 0;
}
