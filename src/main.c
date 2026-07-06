#include <getopt.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "lexer.h"
#include "parser.h"
#include "token_stream.h"
#include "version.h"
#include "vm.h"

static void repl(void);
static char *readFile(const char *path);
static void runFile(const char *path);
static void runCode(const char *source);

const char *help_message =
    "Usage: clox [-h] [-v] [-r|-f [path]|-c [source]|-l [path]|-p [path]] \n";

const char *short_options = "hvrfclp";
static struct option long_options[] = {
    {"help", no_argument, 0, 'h'},  {"version", no_argument, 0, 'v'},
    {"file", no_argument, 0, 'f'},  {"repl", no_argument, 0, 'r'},
    {"code", no_argument, 0, 'c'},  {"lex", no_argument, 0, 'l'},
    {"parse", no_argument, 0, 'p'}, {0, 0, 0, 0}};

int main(int argc, char *argv[]) {
  int saved_argc = argc;
  char **saved_argv = malloc(sizeof(char *) * (argc + 1));
  for (int i = 0; i < argc; i++) {
    saved_argv[i] = argv[i];
  }
  saved_argv[argc] = NULL;

  int opt;
  int long_index = 0;

  while ((opt = getopt_long(argc, argv, short_options, long_options,
                            &long_index)) != -1) {
    switch (opt) {
    case 'h':
      printf("%s\n", help_message);
      return 0;

    case 'v':
      printf("%s\n", CLOX_VERSION);
      return 0;
    case 'r':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.lox");
      repl();
      freeVM();
      free(saved_argv);
      return 0;
    case 'f':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.lox");
      runFile(argv[optind]);
      freeVM();
      free(saved_argv);
      return 0;
    case 'l': {
      const char *source = readFile(argv[optind]);
      TokenStream tokens = lex(source);

      for (int i = 0; i < tokens.count; i++) {
        Token token = tsAdvance(&tokens);

        printf("%s\n", tokenTypeToString(token.type));
      }

      free(saved_argv);
      tsFree(&tokens);
      return 0;
    }
    case 'p': {
      TRACELN("PARSE TIME");
      initVM(saved_argc, saved_argv);
      int outCount = 0;
      bool hadError = false;

      const char *source = readFile(argv[optind]);

      TRACELN("--- START PARSE ---");

      int endLine = 0;
      AstNode **ast = parse(source, &outCount, &hadError, &endLine);

      TRACELN("--- END PARSE ---");

      for (int i = 0; i < outCount; i++) {
        const char *ast_str = print_ast(ast[i]);
        printf("AST: %s\n", ast_str);
        free((void *)ast_str);
      }

      astFreeAll();
      free(ast);
      freeVM();
      free(saved_argv);
      return 0;
    }
    case 'c':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.lox");
      char *source = argv[optind];
      runCode(source);
      freeVM();
      free(saved_argv);
      return 0;
    }
  }

  printf("%s", help_message);
  return 64;
}

static void repl(void) {
  fprintf(stderr, "========================================\n");
  fprintf(stderr, "REPL %30s clox\n\n", CLOX_VERSION);
  fprintf(stderr, "Enter code or type 'exit' to quit.\n\n");
  for (;;) {
    char *line = readline("> ");

    if (line == NULL) {
      printf("\n");
      break;
    }

    if (*line)
      add_history(line);

    if (strcmp(line, "exit") == 0)
      exit(0);

    InterpretResult result = interpret(line);

    switch (result) {
    case INTERPRET_COMPILE_ERROR:
      fprintf(stderr, "Compiler Error!\n");
      break;
    case INTERPRET_RUNTIME_ERROR:
      fprintf(stderr, "Runtime Error!\n");
      break;
    default:
      break;
    }

    free(line);
  }
}

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

static void runFile(const char *path) {
  char *source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);

  if (result == INTERPRET_COMPILE_ERROR) {
    exit(65);
  }

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

static void runCode(const char *source) {
  InterpretResult result = interpret(source);

  if (result == INTERPRET_COMPILE_ERROR) {
    exit(65);
  }

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}
