#include <getopt.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "lexer.h"
#include "parser.h"
#include "strbuf.h"
#include "token_stream.h"
// #include "typecheck.h"
#include "version.h"
#include "vm.h"

static void repl(void);
static char *readFile(const char *path);
static CompiledUnit *compileSource(const char *source);
static void runFile(const char *path);
static void runCode(const char *source);

const char *help_message =
    "Usage: krb [-h] [-v] [-r|-f [path]|-c [source]|-l [path]|-p [path]] \n";

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
      printf("%s\n", KIRBY_VERSION);
      return 0;
    case 'r':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.krb");
      repl();
      compilerSessionEnd();
      freeVM();
      free(saved_argv);
      return 0;
    case 'f':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.krb");
      runFile(argv[optind]);
      compilerSessionEnd();
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
      int outCount = 0;
      bool hadError = false;

      const char *source = readFile(argv[optind]);

      int endLine = 0;
      AstNode **ast = parse(source, &outCount, &hadError, &endLine);

      for (int i = 0; i < outCount; i++) {
        StrBuf ast_node_sb;
        sb_init(&ast_node_sb);

        print_ast(&ast_node_sb, ast[i]);
        printf("%s\n", ast_node_sb.data);

        sb_free(&ast_node_sb);
      }

      astFreeAll();
      free(ast);
      return 0;
    }
    case 'c':
      initVM(saved_argc, saved_argv);
      runFile("stdlib/stdlib.krb");
      char *source = argv[optind];
      runCode(source);
      compilerSessionEnd();
      freeVM();
      free(saved_argv);
      return 0;
    }
  }

  printf("%s", help_message);
  return 64;
}

static void repl(void) {
  fprintf(stderr, "============================================================"
                  "====================\n");
  fprintf(stderr, "kirby %74s\n", KIRBY_VERSION);
  fprintf(stderr, "============================================================"
                  "====================\n\n");
  fprintf(stderr,
          "Enter some code or type 'help' for help or 'exit' to quit.\n\n");
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

    if (strcmp(line, "help") == 0) {
      printf("\nhttps://github.com/kirbylang/kirbylang#documentation\n\n");
      continue;
    }

    CompiledUnit *unit = compileSource(line);

    if (unit == NULL) {
      fprintf(stderr, "Compiler Error!\n");
      free(line);
      continue;
    }

    InterpretResult result = interpret(unit);

    if (result == INTERPRET_RUNTIME_ERROR) {
      fprintf(stderr, "Runtime Error!\n");
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

static CompiledUnit *compileSource(const char *source) {
  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadError, &endLine);

  // bool ok = typecheckProgram(ast, count);

  // if (!ok) {
  //   astFreeAll();
  //   free(ast);
  //   return NULL;
  // }

  CompiledUnit *unit = hadError ? NULL : compile(ast, count, endLine);

  astFreeAll();
  free(ast);

  return unit;
}

static void runFile(const char *path) {
  char *source = readFile(path);
  CompiledUnit *unit = compileSource(source);
  free(source);

  if (unit == NULL) {
    exit(EXIT_CODE_COMPILER_ERR);
  }

  InterpretResult result = interpret(unit);

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(EXIT_CODE_RUNTIME_ERR);
}

static void runCode(const char *source) {
  CompiledUnit *unit = compileSource(source);

  if (unit == NULL) {
    exit(EXIT_CODE_COMPILER_ERR);
  }

  InterpretResult result = interpret(unit);

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(EXIT_CODE_RUNTIME_ERR);
}
