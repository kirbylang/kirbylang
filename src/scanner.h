#ifndef kirby_scanner_h
#define kirby_scanner_h

#include "token.h"

typedef struct {
  const char *start;
  /**
   * The current character being scanned.
   */
  const char *current;
  int line;
} Scanner;

void initScanner(Scanner *scanner, const char *source);
Token scanToken(Scanner *scanner);

#endif
