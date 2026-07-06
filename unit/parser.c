#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../src/ast.h"
#include "../src/memory.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/vm.h"

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
  int saved_argc = argc;
  char **saved_argv = malloc(sizeof(char *) * (argc + 1));
  for (int i = 0; i < argc; i++) {
    saved_argv[i] = argv[i];
  }
  saved_argv[argc] = NULL;

  initVM(saved_argc, saved_argv);
  int outCount = 0;
  bool hadError = false;
  int endLine = 0;

  const char *source =
      readFile("/Users/kylee/bench/cproj/tests/arrays/array_empty.lox");

  AstNode **ast = parse(source, &outCount, &hadError, &endLine);

  for (int i = 0; i < outCount; i++) {
    const char *ast_str = print_ast(ast[i]);
    printf("%s\n", ast_str);
    free((void *)ast_str);
  }

  astFreeAll();
  free(ast);
  freeVM();
  free(saved_argv);

  return 0;
}
