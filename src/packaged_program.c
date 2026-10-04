#ifndef __APPLE__
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "common.h"
#include "packaged_program.h"
#include "unit_bytes.h"

static const char TRAILER_MAGIC[8] = {'K', 'R', 'B', 'A', 'P', 'P', '0', '1'};

#define COPY_CHUNK_SIZE 65536

static void putU32At(char *destination, uint32_t value) {
  for (int i = 0; i < 4; i++) {
    destination[i] = (char)((value >> (8 * i)) & 0xff);
  }
}

static uint32_t getU32At(const char *source) {
  const uint8_t *bytes = (const uint8_t *)source;

  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

void packagedAddUnit(StrBuf *payload, const CompiledUnit *unit) {
  // The length goes first, but is only known once the unit is encoded.
  size_t lengthAt = payload->len;
  sb_append_bytes(payload, "\0\0\0\0", 4);

  unitEncode(unit, payload);

  putU32At(payload->data + lengthAt, (uint32_t)(payload->len - lengthAt - 4));
}

/**
 * Copy everything left in `from` to `to`.
 */
static bool copyStream(FILE *from, FILE *to) {
  char *chunk = (char *)malloc(COPY_CHUNK_SIZE);

  if (chunk == NULL) {
    fprintf(stderr, "malloc failed in packagedWrite\n");
    exit(EXIT_CODE_OS_ERR);
  }

  bool ok = true;
  size_t count;

  while (ok && (count = fread(chunk, 1, COPY_CHUNK_SIZE, from)) > 0) {
    ok = fwrite(chunk, 1, count, to) == count;
  }

  ok = ok && !ferror(from);
  free(chunk);
  return ok;
}

static bool writeTrailer(FILE *file, size_t payloadLength) {
  char trailer[PACKAGED_TRAILER_SIZE];
  memcpy(trailer, TRAILER_MAGIC, sizeof TRAILER_MAGIC);

  uint64_t length = payloadLength;
  for (int i = 0; i < 8; i++) {
    trailer[sizeof TRAILER_MAGIC + (size_t)i] =
        (char)((length >> (8 * i)) & 0xff);
  }

  return fwrite(trailer, 1, sizeof trailer, file) == sizeof trailer;
}

/**
 * Give the file execute permission wherever it has read permission, which
 * keeps the user's umask in effect.
 */
static bool makeExecutable(const char *path) {
  struct stat info;

  if (stat(path, &info) != 0) {
    return false;
  }

  mode_t mode = info.st_mode & 0777;
  return chmod(path, mode | ((mode & 0444) >> 2)) == 0;
}

static bool isSameFile(const struct stat *a, const struct stat *b) {
  return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

bool packagedWrite(const char *runtimePath, const char *outputPath,
                   const StrBuf *payload, char *error, size_t errorSize) {
  FILE *runtime = fopen(runtimePath, "rb");

  if (runtime == NULL) {
    snprintf(error, errorSize, "could not open the runtime \"%s\": %s",
             runtimePath, strerror(errno));
    return false;
  }

  struct stat runtimeInfo;
  struct stat outputInfo;

  if (fstat(fileno(runtime), &runtimeInfo) == 0 &&
      stat(outputPath, &outputInfo) == 0 &&
      isSameFile(&runtimeInfo, &outputInfo)) {
    snprintf(error, errorSize, "the output \"%s\" is the runtime itself",
             outputPath);
    fclose(runtime);
    return false;
  }

  FILE *output = fopen(outputPath, "wb");

  if (output == NULL) {
    snprintf(error, errorSize, "could not write \"%s\": %s", outputPath,
             strerror(errno));
    fclose(runtime);
    return false;
  }

  bool ok = copyStream(runtime, output);
  ok = ok && fwrite(payload->data, 1, payload->len, output) == payload->len;
  ok = ok && writeTrailer(output, payload->len);
  int problem = errno;

  // Closing writes out what is still buffered, which can fail too.
  if (fclose(output) != 0 && ok) {
    ok = false;
    problem = errno;
  }

  fclose(runtime);

  if (ok && !makeExecutable(outputPath)) {
    ok = false;
    problem = errno;
  }

  if (!ok) {
    snprintf(error, errorSize, "could not write \"%s\": %s", outputPath,
             strerror(problem));
    remove(outputPath);
    return false;
  }

  return true;
}

PackagedReadStatus packagedRead(const char *path, StrBuf *payload) {
  sb_init(payload);

  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return PACKAGED_READ_IO_ERROR;
  }

  PackagedReadStatus status = PACKAGED_READ_IO_ERROR;

  if (fseek(file, 0, SEEK_END) != 0) {
    goto done;
  }

  long size = ftell(file);

  if (size < 0) {
    goto done;
  }

  if ((unsigned long)size < PACKAGED_TRAILER_SIZE) {
    status = PACKAGED_READ_NO_PROGRAM;
    goto done;
  }

  char trailer[PACKAGED_TRAILER_SIZE];

  if (fseek(file, size - PACKAGED_TRAILER_SIZE, SEEK_SET) != 0 ||
      fread(trailer, 1, sizeof trailer, file) != sizeof trailer) {
    goto done;
  }

  if (memcmp(trailer, TRAILER_MAGIC, sizeof TRAILER_MAGIC) != 0) {
    status = PACKAGED_READ_NO_PROGRAM;
    goto done;
  }

  uint64_t length = 0;
  for (int i = 0; i < 8; i++) {
    length |= (uint64_t)(uint8_t)trailer[sizeof TRAILER_MAGIC + (size_t)i]
              << (8 * i);
  }

  if (length > (uint64_t)size - PACKAGED_TRAILER_SIZE) {
    status = PACKAGED_READ_DAMAGED;
    goto done;
  }

  if (fseek(file, size - PACKAGED_TRAILER_SIZE - (long)length, SEEK_SET) != 0) {
    goto done;
  }

  char chunk[4096];
  uint64_t left = length;

  while (left > 0) {
    size_t want = left < sizeof chunk ? (size_t)left : sizeof chunk;

    if (fread(chunk, 1, want, file) != want) {
      goto done;
    }

    sb_append_bytes(payload, chunk, want);
    left -= want;
  }

  status = PACKAGED_READ_OK;

done:
  fclose(file);
  return status;
}

PackagedUnitStatus packagedNextUnit(const StrBuf *payload, size_t *cursor,
                                    const uint8_t **bytes, size_t *length) {
  size_t position = *cursor;

  if (position == payload->len) {
    return PACKAGED_UNIT_END;
  }

  if (payload->len - position < 4) {
    return PACKAGED_UNIT_DAMAGED;
  }

  size_t unitLength = getU32At(payload->data + position);
  position += 4;

  if (unitLength > payload->len - position) {
    return PACKAGED_UNIT_DAMAGED;
  }

  *bytes = (const uint8_t *)payload->data + position;
  *length = unitLength;
  *cursor = position + unitLength;
  return PACKAGED_UNIT_OK;
}

#if defined(__linux__)

char *packagedSelfPath(void) {
  size_t size = 256;

  for (;;) {
    char *path = (char *)malloc(size);

    if (path == NULL) {
      return NULL;
    }

    ssize_t count = readlink("/proc/self/exe", path, size);

    if (count < 0) {
      free(path);
      return NULL;
    }

    if ((size_t)count < size) {
      path[count] = '\0';
      return path;
    }

    free(path);
    size *= 2;
  }
}

#elif defined(__APPLE__)

char *packagedSelfPath(void) {
  uint32_t size = 0;
  _NSGetExecutablePath(NULL, &size);

  char *path = (char *)malloc(size);

  if (path == NULL || _NSGetExecutablePath(path, &size) != 0) {
    free(path);
    return NULL;
  }

  char *resolved = realpath(path, NULL);
  free(path);
  return resolved;
}

#else

char *packagedSelfPath(void) { return NULL; }

#endif
