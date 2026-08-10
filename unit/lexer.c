#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/lexer.h"
#include "../src/token.h"
#include "../src/token_stream.h"

int main(void) {
  const char *source = "[];";

  TokenStream tokens = lex(source);

  TokenType expected[] = {
      TOKEN_LEFT_BRACKET,
      TOKEN_RIGHT_BRACKET,
      TOKEN_SEMICOLON,
      TOKEN_EOF,
  };

  assert(tokens.count == (int)(sizeof(expected) / sizeof(expected[0])));

  for (int i = 0; i < tokens.count; i++) {
    assert(tsAdvance(&tokens).type == expected[i]);
  }

  return 0;
}
