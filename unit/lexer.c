#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/lexer.h"
#include "../src/token.h"
#include "../src/token_stream.h"

static char *readFile(const char *path) {
  FILE *file = fopen(path, "rb");

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

int main(int argc, char *argv[]) {
  const char *source =
      readFile("/Users/kylee/bench/cproj/tests/arrays/array_empty.krb");

  TokenStream tokens = lex(source);

  for (int i = 0; i < tokens.count; i++) {
    Token token = tsAdvance(&tokens);

    printf("%s\n", tokenTypeToString(token.type));
  }

  free(source);

  return 0;
}
