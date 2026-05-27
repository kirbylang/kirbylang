#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UINT8_COUNT (UINT8_MAX + 1)

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define ANSI_BOLD "\x1b[1m"

#ifdef DEBUG_TRACE_EXECUTION
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#define TRACELN(...)                                                           \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fputc('\n', stderr);                                                       \
    break;                                                                     \
                                                                               \
  } while (0)
#else
#define TRACE(...)                                                             \
  do {                                                                         \
  } while (0)
#define TRACELN(...)                                                           \
  do {                                                                         \
  } while (0)
#endif

#endif
