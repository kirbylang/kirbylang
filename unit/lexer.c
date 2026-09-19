#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/lexer.h"
#include "../src/token.h"
#include "../src/token_stream.h"

static void assert_token_types(const char *source, TokenType *expected,
                               int expectedCount) {
  TokenStream tokens = lex(source);

  assert(tokens.count == expectedCount);

  for (int i = 0; i < tokens.count; i++) {
    assert(tsAdvance(&tokens).type == expected[i]);
  }

  tsFree(&tokens);
}

int main(void) {
  TokenType bracketExpected[] = {
      TOKEN_LEFT_BRACKET,
      TOKEN_RIGHT_BRACKET,
      TOKEN_SEMICOLON,
      TOKEN_EOF,
  };

  assert_token_types(
      "[];", bracketExpected,
      (int)(sizeof(bracketExpected) / sizeof(bracketExpected[0])));

  TokenType traitExpected[] = {
      TOKEN_TRAIT,
      TOKEN_TRUE,
      TOKEN_IDENTIFIER,
      TOKEN_EOF,
  };

  assert_token_types("trait true trapdoor", traitExpected,
                     (int)(sizeof(traitExpected) / sizeof(traitExpected[0])));

  TokenType nativeRefExpected[] = {
      TOKEN_IDENTIFIER,
      TOKEN_LEFT_PAREN,
      TOKEN_RIGHT_PAREN,
      TOKEN_SEMICOLON,
      TOKEN_EOF,
  };

  assert_token_types(
      "@len();", nativeRefExpected,
      (int)(sizeof(nativeRefExpected) / sizeof(nativeRefExpected[0])));

  TokenType bareAtExpected[] = {
      TOKEN_ERROR,
      TOKEN_EOF,
  };

  assert_token_types("@", bareAtExpected,
                     (int)(sizeof(bareAtExpected) / sizeof(bareAtExpected[0])));

  return 0;
}
