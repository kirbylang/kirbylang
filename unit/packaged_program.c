#ifndef __APPLE__
#define _POSIX_C_SOURCE 200809L
#endif

// Asserts must run in every build type.
#undef NDEBUG

#include <assert.h>
#include <dirent.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/compiled_unit.h"
#include "../src/packaged_program.h"
#include "../src/strbuf.h"
#include "../src/unit_bytes.h"

static char directory[] = "/tmp/krb-packaged-program-XXXXXX";

static char *pathTo(const char *name) {
  size_t length = strlen(directory) + 1 + strlen(name) + 1;
  char *path = (char *)malloc(length);
  snprintf(path, length, "%s/%s", directory, name);
  return path;
}

static void writeFile(const char *path, const void *bytes, size_t length,
                      mode_t mode) {
  FILE *file = fopen(path, "wb");
  assert(file != NULL);
  assert(fwrite(bytes, 1, length, file) == length);
  assert(fclose(file) == 0);
  assert(chmod(path, mode) == 0);
}

static StrBuf readFile(const char *path) {
  StrBuf contents;
  sb_init(&contents);

  FILE *file = fopen(path, "rb");
  assert(file != NULL);

  char chunk[256];
  size_t count;
  while ((count = fread(chunk, 1, sizeof chunk, file)) > 0) {
    sb_append_bytes(&contents, chunk, count);
  }

  fclose(file);
  return contents;
}

static bool exists(const char *path) {
  struct stat info;
  return stat(path, &info) == 0;
}

/**
 * A unit with a script function that has `functionCount` functions in all.
 */
static CompiledUnit *newUnit(int functionCount) {
  CompiledUnit *unit = (CompiledUnit *)malloc(sizeof(CompiledUnit));
  cuInit(unit);

  for (int i = 0; i < functionCount; i++) {
    int index = cuAddFunction(unit);
    cuWriteByte(cuGetFnByIndex(unit, index), (uint8_t)i, 1);
  }

  return unit;
}

static void freeUnit(CompiledUnit *unit) {
  freeCompiledUnit(unit);
  free(unit);
}

static const char RUNTIME_BYTES[] = "pretend this is an executable";

static char *newRuntime(mode_t mode) {
  char *path = pathTo("runtime");
  writeFile(path, RUNTIME_BYTES, sizeof RUNTIME_BYTES, mode);
  return path;
}

static void testWritesTheRuntimeThenThePayloadThenATrailer(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("app");

  CompiledUnit *first = newUnit(1);
  CompiledUnit *second = newUnit(2);
  StrBuf payload;
  sb_init(&payload);
  packagedAddUnit(&payload, first);
  packagedAddUnit(&payload, second);

  char error[256] = "";
  assert(packagedWrite(runtime, output, &payload, error, sizeof error));

  StrBuf written = readFile(output);
  assert(written.len ==
         sizeof RUNTIME_BYTES + payload.len + PACKAGED_TRAILER_SIZE);
  assert(memcmp(written.data, RUNTIME_BYTES, sizeof RUNTIME_BYTES) == 0);
  assert(memcmp(written.data + sizeof RUNTIME_BYTES, payload.data,
                payload.len) == 0);
  assert(memcmp(written.data + written.len - PACKAGED_TRAILER_SIZE, "KRBAPP01",
                8) == 0);

  sb_free(&written);
  sb_free(&payload);
  freeUnit(first);
  freeUnit(second);
  free(runtime);
  free(output);
}

static void testReadGivesBackTheUnitsInOrder(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("app");

  CompiledUnit *first = newUnit(1);
  CompiledUnit *second = newUnit(2);
  StrBuf payload;
  sb_init(&payload);
  packagedAddUnit(&payload, first);
  packagedAddUnit(&payload, second);

  char error[256];
  assert(packagedWrite(runtime, output, &payload, error, sizeof error));

  StrBuf read;
  assert(packagedRead(output, &read) == PACKAGED_READ_OK);
  assert(read.len == payload.len);
  assert(memcmp(read.data, payload.data, payload.len) == 0);

  size_t cursor = 0;
  const uint8_t *bytes;
  size_t length;
  int expectedFunctions[] = {1, 2};

  for (int i = 0; i < 2; i++) {
    assert(packagedNextUnit(&read, &cursor, &bytes, &length) ==
           PACKAGED_UNIT_OK);

    UnitDecodeStatus status;
    CompiledUnit *unit = unitDecode(bytes, length, &status);
    assert(status == UNIT_DECODE_OK);
    assert(unit->functionCount == expectedFunctions[i]);
    freeUnit(unit);
  }

  assert(packagedNextUnit(&read, &cursor, &bytes, &length) ==
         PACKAGED_UNIT_END);

  sb_free(&read);
  sb_free(&payload);
  freeUnit(first);
  freeUnit(second);
  free(runtime);
  free(output);
}

static void testOutputIsExecutableEvenWhenTheRuntimeFileIsNot(void) {
  char *runtime = newRuntime(0644);
  char *output = pathTo("app");

  StrBuf payload;
  sb_init(&payload);

  char error[256];
  assert(packagedWrite(runtime, output, &payload, error, sizeof error));

  struct stat info;
  assert(stat(output, &info) == 0);
  assert((info.st_mode & S_IXUSR) != 0);

  sb_free(&payload);
  free(runtime);
  free(output);
}

static void testAFileWithoutATrailerHasNoProgram(void) {
  char *runtime = newRuntime(0755);

  StrBuf payload;
  assert(packagedRead(runtime, &payload) == PACKAGED_READ_NO_PROGRAM);

  sb_free(&payload);
  free(runtime);
}

static void testAFileShorterThanATrailerHasNoProgram(void) {
  char *tiny = pathTo("tiny");
  writeFile(tiny, "KRBAPP01", 8, 0755);

  StrBuf payload;
  assert(packagedRead(tiny, &payload) == PACKAGED_READ_NO_PROGRAM);

  sb_free(&payload);
  free(tiny);
}

static void testAMissingFileIsAnIoError(void) {
  char *missing = pathTo("missing");

  StrBuf payload;
  assert(packagedRead(missing, &payload) == PACKAGED_READ_IO_ERROR);

  sb_free(&payload);
  free(missing);
}

static void testATrailerThatAsksForTooMuchIsDamaged(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("app");

  CompiledUnit *unit = newUnit(1);
  StrBuf payload;
  sb_init(&payload);
  packagedAddUnit(&payload, unit);

  char error[256];
  assert(packagedWrite(runtime, output, &payload, error, sizeof error));

  StrBuf written = readFile(output);
  written.data[written.len - 8] = 0x7f; // the top byte of the payload length
  writeFile(output, written.data, written.len, 0755);

  StrBuf read;
  assert(packagedRead(output, &read) == PACKAGED_READ_DAMAGED);

  sb_free(&read);
  sb_free(&written);
  sb_free(&payload);
  freeUnit(unit);
  free(runtime);
  free(output);
}

static void testAnEmptyPayloadHasNoUnits(void) {
  StrBuf payload;
  sb_init(&payload);

  size_t cursor = 0;
  const uint8_t *bytes;
  size_t length;
  assert(packagedNextUnit(&payload, &cursor, &bytes, &length) ==
         PACKAGED_UNIT_END);

  sb_free(&payload);
}

static void testAUnitThatRunsPastThePayloadIsDamaged(void) {
  StrBuf payload;
  sb_init(&payload);
  sb_append_bytes(&payload, "\x09\0\0\0abc", 7); // says 9 bytes, has 3

  size_t cursor = 0;
  const uint8_t *bytes;
  size_t length;
  assert(packagedNextUnit(&payload, &cursor, &bytes, &length) ==
         PACKAGED_UNIT_DAMAGED);

  sb_free(&payload);
}

static void testStrayBytesAfterTheLastUnitAreDamaged(void) {
  StrBuf payload;
  sb_init(&payload);
  sb_append_bytes(&payload, "\x01\0", 2); // too short to hold a length

  size_t cursor = 0;
  const uint8_t *bytes;
  size_t length;
  assert(packagedNextUnit(&payload, &cursor, &bytes, &length) ==
         PACKAGED_UNIT_DAMAGED);

  sb_free(&payload);
}

static void testAMissingRuntimeIsAnErrorAndLeavesNoOutput(void) {
  char *runtime = pathTo("not-there");
  char *output = pathTo("app");

  StrBuf payload;
  sb_init(&payload);

  char error[256] = "";
  assert(!packagedWrite(runtime, output, &payload, error, sizeof error));
  assert(strstr(error, "not-there") != NULL);
  assert(!exists(output));

  sb_free(&payload);
  free(runtime);
  free(output);
}

static void testAnOutputInAMissingFolderIsAnError(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("no-such-folder/app");

  StrBuf payload;
  sb_init(&payload);

  char error[256] = "";
  assert(!packagedWrite(runtime, output, &payload, error, sizeof error));
  assert(strlen(error) > 0);

  sb_free(&payload);
  free(runtime);
  free(output);
}

static void testTheRuntimeCanNotBeItsOwnOutput(void) {
  char *runtime = newRuntime(0755);

  StrBuf payload;
  sb_init(&payload);

  char error[256] = "";
  assert(!packagedWrite(runtime, runtime, &payload, error, sizeof error));
  assert(strlen(error) > 0);

  StrBuf after = readFile(runtime);
  assert(after.len == sizeof RUNTIME_BYTES);
  assert(memcmp(after.data, RUNTIME_BYTES, sizeof RUNTIME_BYTES) == 0);

  sb_free(&after);
  sb_free(&payload);
  free(runtime);
}

static const char PREVIOUS_PROGRAM[] = "the program that was here before";

// The number of files in the test folder, so a test can tell that nothing but
// the files it made is there.
static int countFiles(void) {
  DIR *folder = opendir(directory);
  assert(folder != NULL);

  int count = 0;
  struct dirent *entry;

  while ((entry = readdir(folder)) != NULL) {
    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
      count++;
    }
  }

  closedir(folder);
  return count;
}

static void testAFailedWriteLeavesAnExistingOutputAlone(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("app");
  writeFile(output, PREVIOUS_PROGRAM, sizeof PREVIOUS_PROGRAM, 0755);

  CompiledUnit *unit = newUnit(1);
  StrBuf payload;
  sb_init(&payload);
  packagedAddUnit(&payload, unit);

  // A file size limit makes the write fail after it has started, as a full
  // disk would. Going over the limit also raises SIGXFSZ, which is ignored so
  // that the write fails with an error and the process carries on.
  struct rlimit original;
  assert(getrlimit(RLIMIT_FSIZE, &original) == 0);
  void (*previousHandler)(int) = signal(SIGXFSZ, SIG_IGN);

  struct rlimit small = original;
  small.rlim_cur = 8;
  assert(setrlimit(RLIMIT_FSIZE, &small) == 0);

  char error[256] = "";
  bool written = packagedWrite(runtime, output, &payload, error, sizeof error);

  assert(setrlimit(RLIMIT_FSIZE, &original) == 0);
  signal(SIGXFSZ, previousHandler);

  assert(!written);
  assert(strlen(error) > 0);

  StrBuf kept = readFile(output);
  assert(kept.len == sizeof PREVIOUS_PROGRAM);
  assert(memcmp(kept.data, PREVIOUS_PROGRAM, sizeof PREVIOUS_PROGRAM) == 0);
  assert(countFiles() == 2);  // the runtime and the output, nothing left over

  sb_free(&kept);
  sb_free(&payload);
  freeUnit(unit);
  free(runtime);
  free(output);
}

static void testReplacingAnOutputGivesTheNewProgramAndLeavesNothingElse(void) {
  char *runtime = newRuntime(0755);
  char *output = pathTo("app");

  // Longer than the new program, so a leftover tail would show.
  char previous[512];
  memset(previous, 'x', sizeof previous);
  writeFile(output, previous, sizeof previous, 0755);

  StrBuf payload;
  sb_init(&payload);

  char error[256] = "";
  assert(packagedWrite(runtime, output, &payload, error, sizeof error));

  StrBuf written = readFile(output);
  assert(written.len == sizeof RUNTIME_BYTES + PACKAGED_TRAILER_SIZE);
  assert(memcmp(written.data, RUNTIME_BYTES, sizeof RUNTIME_BYTES) == 0);
  assert(countFiles() == 2);  // the runtime and the output, nothing left over

  sb_free(&written);
  sb_free(&payload);
  free(runtime);
  free(output);
}

static void testFindsItsOwnPath(void) {
  char *path = packagedSelfPath();
  assert(path != NULL);
  assert(strstr(path, "packaged-program-unit-test") != NULL);
  assert(exists(path));
  free(path);
}

static void removeFiles(void) {
  const char *names[] = {"runtime", "app", "tiny"};

  for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
    char *path = pathTo(names[i]);
    remove(path);
    free(path);
  }
}

typedef void (*TestFn)(void);

int main(void) {
  assert(mkdtemp(directory) != NULL);

  TestFn tests[] = {
      testWritesTheRuntimeThenThePayloadThenATrailer,
      testReadGivesBackTheUnitsInOrder,
      testOutputIsExecutableEvenWhenTheRuntimeFileIsNot,
      testAFileWithoutATrailerHasNoProgram,
      testAFileShorterThanATrailerHasNoProgram,
      testAMissingFileIsAnIoError,
      testATrailerThatAsksForTooMuchIsDamaged,
      testAnEmptyPayloadHasNoUnits,
      testAUnitThatRunsPastThePayloadIsDamaged,
      testStrayBytesAfterTheLastUnitAreDamaged,
      testAMissingRuntimeIsAnErrorAndLeavesNoOutput,
      testAnOutputInAMissingFolderIsAnError,
      testTheRuntimeCanNotBeItsOwnOutput,
      testAFailedWriteLeavesAnExistingOutputAlone,
      testReplacingAnOutputGivesTheNewProgramAndLeavesNothingElse,
      testFindsItsOwnPath,
  };

  for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
    removeFiles();
    tests[i]();
  }

  removeFiles();
  rmdir(directory);
  return 0;
}
