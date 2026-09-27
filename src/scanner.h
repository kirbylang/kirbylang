#ifndef kirby_scanner_h
#define kirby_scanner_h

#include "token.h"

/**
 * How deeply interpolated strings can nest inside each other's placeholders.
 * The scanner tracks open placeholders in a fixed-size array.
 */
#define MAX_INTERP_DEPTH 16

typedef struct {
  const char *start;
  /**
   * The current character being scanned.
   */
  const char *current;
  int line;
  /**
   * How many placeholders the scanner is inside, e.g. 1 while scanning `x` in
   * `$"a{x}"`, and 0 while scanning `b` in `$"a{x}b{y}"`.
   */
  int interpDepth;
  /**
   * For each open placeholder, how many `{` inside it are still unclosed. A
   * `}` only ends the placeholder when its count is 0.
   */
  int interpBraces[MAX_INTERP_DEPTH];
} Scanner;

void initScanner(Scanner *scanner, const char *source);
Token scanToken(Scanner *scanner);

#endif
