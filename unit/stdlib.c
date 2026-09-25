#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/stdlib_source.h"

// The embedded copy must match stdlib/stdlib.krb exactly.
int main(void) {
  FILE *file = fopen(KIRBY_STDLIB_PATH, "rb");
  assert(file != NULL);

  fseek(file, 0L, SEEK_END);
  long size = ftell(file);
  rewind(file);

  char *contents = malloc((size_t)size + 1);
  assert(contents != NULL);
  size_t read = fread(contents, 1, (size_t)size, file);
  fclose(file);
  assert(read == (size_t)size);

  assert(KIRBY_STDLIB_len == (unsigned int)size);
  assert(memcmp(KIRBY_STDLIB, contents, (size_t)size) == 0);
  assert(KIRBY_STDLIB[KIRBY_STDLIB_len] == '\0');

  free(contents);
  printf("stdlib tests passed\n");
  return 0;
}
