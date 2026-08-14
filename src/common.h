#ifndef kirby_common_h
#define kirby_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

#define EXIT_CODE_COMPILER_ERR 65 // INTERPRET_COMPILE_ERROR
#define EXIT_CODE_RUNTIME_ERR 70  // INTERPRET_RUNTIME_ERROR
#define EXIT_CODE_OS_ERR 71

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#ifdef DEBUG_TRACE_EXECUTION
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define TRACELN(...)                                                           \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
  } while (0)
#else
#define TRACE(...)                                                             \
  do {                                                                         \
  } while (0)
#define TRACELN(...)                                                           \
  do {                                                                         \
  } while (0)
#endif

uint32_t hashBytes(const char *key, int length);

#endif
