#include "common.h"
#include "scanner.h"
#include "token_stream.h"
#include <stdio.h>

TokenStream lex(const char *source) {
  TRACELN("---lexing start---");

  TokenStream ts;
  tsInit(&ts);

  Scanner scanner;
  initScanner(&scanner, source);

  for (;;) {
    Token token = scanToken(&scanner);
    tsWrite(&ts, token);
    if (token.type == TOKEN_EOF)
      break;
  }

  TRACELN("---lexing end---");

  return ts;
}
