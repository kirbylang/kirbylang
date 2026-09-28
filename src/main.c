#include "../lib/toml-c/toml-c.h"
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
#include "project.h"
#include "resolved_impl_targets.h"
#include "stdlib_source.h"
#include "strbuf.h"
#include "token_stream.h"
#include "typecheck.h"
#include "version.h"
#include "vm.h"

static void repl(void);
static char *readFile(const char *path);
static CompiledUnit *compileSource(const char *source, bool typecheck);
static void runFile(const char *path);
static char *readFileNoExit(const char *path);
static void runCode(const char *source);
static void compileFile(const char *path);
static void compileCode(const char *source);

char *doc = ""
            "bin = \"bin/main.krb\"\n";

const char *help_message =
    "Usage: krb [-h] [-v] <command> [args]\n"
    "\n"
    "Commands:\n"
    "\n"
    "  run [path] [args...]   Run a file. Without a path, runs the \"bin\" "
    "file\n"
    "                         from kirby.project.toml. Extra args are passed\n"
    "                         to the script\n"
    "  repl                   Start the interactive REPL\n"
    "  exec <source>          Run source code given as a string\n"
    "  compile <path>         Compile a file without running it\n"
    "  lex <path>             Print the tokens of a file\n"
    "  parse <path>           Print the AST of a file\n"
    "\n"
    "Examples:\n"
    "\n"
    "krb --help                        # -h is the short option\n"
    "krb --version                     # -v is the short option\n"
    "krb run path/to/file.krb\n"
    "krb run                           # runs the project's \"bin\" file\n"
    "krb run -- arg1 arg2              # passes args to the project's \"bin\"\n"
    "krb repl\n"
    "krb compile path/to/file.krb\n"
    "krb parse path/to/file.krb\n"
    "krb exec 'print \"Hello World\";'\n"
    "";

const char *short_options = "hv";
static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                       {"version", no_argument, 0, 'v'},
                                       {0, 0, 0, 0}};

typedef int (*CommandFn)(int argc, char *argv[]);

typedef struct {
  const char *name;
  CommandFn fn;
} Command;

static int usageError(void) {
  fprintf(stderr, "kirby %s\n\n", KIRBY_VERSION);
  fprintf(stderr, "%s", help_message);
  return 64;
}

/* Reads kirby.project.toml from the current directory, if present. */
static KirbyProject loadProject(void) {
  KirbyProject krb_project = {0};

  char errbuf[200];

  char *tomlsource = readFileNoExit("kirby.project.toml");

  if (tomlsource != NULL) {
    toml_table_t *project = toml_parse(tomlsource, errbuf, sizeof(errbuf));

    if (project) {
      toml_value_t bin_value = toml_table_string(project, "bin");

      if (bin_value.ok) {
        krb_project.bin = bin_value.u.s;
      }
    }
  }

  return krb_project;
}

/* Starts the VM and type checker, then loads the standard library. */
static void sessionBegin(int argc, char *argv[]) {
  initVM(argc, argv);
  typchkSessionBegin();
  runCode(KIRBY_STDLIB);
}

static void sessionEnd(void) {
  compilerSessionEnd();
  typchkSessionEnd();
  freeVM();
}

/* krb run [path] [args...]
 * With no path, or "--" in place of the path, the project's "bin" file runs. */
static int cmdRun(int argc, char *argv[]) {
  char *file = NULL;

  if (argc >= 3 && strcmp(argv[2], "--") != 0) {
    file = argv[2];
  } else {
    file = loadProject().bin;
  }

  if (file == NULL) {
    fprintf(stderr, "No file was passed\n");
    exit(1);
  }

  sessionBegin(argc, argv);
  runFile(file);
  sessionEnd();
  return 0;
}

/* krb repl */
static int cmdRepl(int argc, char *argv[]) {
  sessionBegin(argc, argv);
  repl();
  sessionEnd();
  return 0;
}

/* krb exec <source> */
static int cmdExec(int argc, char *argv[]) {
  if (argc < 3)
    return usageError();

  sessionBegin(argc, argv);
  runCode(argv[2]);
  sessionEnd();
  return 0;
}

/* krb compile <path> */
static int cmdCompile(int argc, char *argv[]) {
  if (argc < 3)
    return usageError();

  typchkSessionBegin();
  compileCode(KIRBY_STDLIB);
  compileFile(argv[2]);
  compilerSessionEnd();
  typchkSessionEnd();
  fprintf(stderr, "Compiled!\n");
  return 0;
}

/* krb lex <path> */
static int cmdLex(int argc, char *argv[]) {
  if (argc < 3)
    return usageError();

  const char *source = readFile(argv[2]);
  TokenStream tokens = lex(source);

  for (int i = 0; i < tokens.count; i++) {
    Token token = tsAdvance(&tokens);

    printf("%s\n", tokenTypeToString(token.type));
  }

  tsFree(&tokens);
  return 0;
}

/* krb parse <path> */
static int cmdParse(int argc, char *argv[]) {
  if (argc < 3)
    return usageError();

  int outCount = 0;
  bool hadError = false;
  int endLine = 0;

  const char *source = readFile(argv[2]);
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

static const Command commands[] = {
    {"run", cmdRun},         {"repl", cmdRepl}, {"exec", cmdExec},
    {"compile", cmdCompile}, {"lex", cmdLex},   {"parse", cmdParse},
};

static const Command *findCommand(const char *name) {
  size_t count = sizeof(commands) / sizeof(commands[0]);

  for (size_t i = 0; i < count; i++) {
    if (strcmp(commands[i].name, name) == 0)
      return &commands[i];
  }

  return NULL;
}

int main(int argc, char *argv[]) {
  /* A first argument without a leading '-' is a command name. Commands are
   * matched before getopt runs so that getopt never reorders or consumes
   * the script's own arguments. */
  if (argc >= 2 && argv[1][0] != '-') {
    const Command *command = findCommand(argv[1]);

    if (command == NULL) {
      fprintf(stderr, "Unknown command \"%s\".\n\n", argv[1]);
      return usageError();
    }

    return command->fn(argc, argv);
  }

  int opt;
  int long_index = 0;

  while ((opt = getopt_long(argc, argv, short_options, long_options,
                            &long_index)) != -1) {
    switch (opt) {
    case 'h':
      fprintf(stderr, "kirby %s\n\n", KIRBY_VERSION);
      fprintf(stderr, "%s\n", help_message);
      return 0;
    case 'v':
      printf("%s\n", KIRBY_VERSION);
      return 0;
    }
  }

  return usageError();
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

    CompiledUnit *unit = compileSource(line, /*typecheck=*/true);

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
  if (path == NULL) {
    fprintf(stderr, "%s", help_message);
    exit(64);
  }

  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(EXIT_CODE_OS_ERR);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);

  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(EXIT_CODE_OS_ERR);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static char *readFileNoExit(const char *path) {
  if (path == NULL) {
    fprintf(stderr, "%s", help_message);
    exit(64);
  }

  FILE *file = fopen(path, "rb");

  if (file == NULL) {
    return NULL;
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);

  char *buffer = (char *)malloc(fileSize + 1);

  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(EXIT_CODE_OS_ERR);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';

  fclose(file);
  return buffer;
}

static CompiledUnit *compileSource(const char *source, bool typecheck) {
  // Entries recorded here are keyed by AstNode* identity, valid only for
  // the AST this one call parses and (if it gets that far) compiles --
  // see resolved_impl_targets.h.
  resolvedImplTargetsReset();

  int count = 0;
  bool hadError = false;
  int endLine = 0;
  AstNode **ast = parse(source, &count, &hadError, &endLine);

  if (!hadError && typecheck) {
    if (!typchkCheckProgram(ast, count)) {
      hadError = true;
    }
  }

  CompiledUnit *unit = hadError ? NULL : compile(ast, count, endLine);

  astFreeAll();
  free(ast);

  return unit;
}

static void compileFile(const char *path) {
  char *source = readFile(path);
  compileCode(source);
  free(source);
}

static void compileCode(const char *source) {
  CompiledUnit *unit = compileSource(source, /*typecheck=*/true);

  if (unit == NULL) {
    exit(EXIT_CODE_COMPILER_ERR);
  }
}

static void runFile(const char *path) {
  char *source = readFile(path);
  CompiledUnit *unit = compileSource(source, /*typecheck=*/true);
  free(source);

  if (unit == NULL) {
    exit(EXIT_CODE_COMPILER_ERR);
  }

  InterpretResult result = interpret(unit);

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(EXIT_CODE_RUNTIME_ERR);
}

static void runCode(const char *source) {
  CompiledUnit *unit = compileSource(source, /*typecheck=*/true);

  if (unit == NULL) {
    exit(EXIT_CODE_COMPILER_ERR);
  }

  InterpretResult result = interpret(unit);

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(EXIT_CODE_RUNTIME_ERR);
}
