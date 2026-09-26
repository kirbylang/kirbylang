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

  TokenType interpNoPlaceholderExpected[] = {
      TOKEN_INTERP_STRING,
      TOKEN_EOF,
  };

  assert_token_types("$\"plain\"", interpNoPlaceholderExpected,
                     (int)(sizeof(interpNoPlaceholderExpected) /
                           sizeof(interpNoPlaceholderExpected[0])));

  TokenType interpExpected[] = {
      TOKEN_INTERP_START,  // $"a {
      TOKEN_IDENTIFIER,    // x
      TOKEN_INTERP_MIDDLE, // } b {
      TOKEN_IDENTIFIER,    // y
      TOKEN_INTERP_END,    // } c"
      TOKEN_EOF,
  };

  assert_token_types(
      "$\"a {x} b {y} c\"", interpExpected,
      (int)(sizeof(interpExpected) / sizeof(interpExpected[0])));

  TokenType interpBracesExpected[] = {
      TOKEN_INTERP_START, // $"
      TOKEN_IDENTIFIER,   // P
      TOKEN_LEFT_BRACE,   // {
      TOKEN_IDENTIFIER,   // x
      TOKEN_COLON,        // :
      TOKEN_NUMBER,       // 1
      TOKEN_RIGHT_BRACE,  // }
      TOKEN_INTERP_END,   // }"
      TOKEN_EOF,
  };

  assert_token_types(
      "$\"{P { x: 1 }}\"", interpBracesExpected,
      (int)(sizeof(interpBracesExpected) / sizeof(interpBracesExpected[0])));

  TokenType interpNestedExpected[] = {
      TOKEN_INTERP_START, // $"a{
      TOKEN_INTERP_START, // $"b{
      TOKEN_IDENTIFIER,   // c
      TOKEN_INTERP_END,   // }"
      TOKEN_INTERP_END,   // }d"
      TOKEN_EOF,
  };

  assert_token_types(
      "$\"a{$\"b{c}\"}d\"", interpNestedExpected,
      (int)(sizeof(interpNestedExpected) / sizeof(interpNestedExpected[0])));

  TokenType interpEscapedBraceExpected[] = {
      TOKEN_INTERP_STRING,
      TOKEN_EOF,
  };

  assert_token_types("$\"\\{x\\}\"", interpEscapedBraceExpected,
                     (int)(sizeof(interpEscapedBraceExpected) /
                           sizeof(interpEscapedBraceExpected[0])));

  TokenType interpDoubledBracesExpected[] = {
      TOKEN_INTERP_STRING, // $"{{x}}" -- literal braces, no placeholder
      TOKEN_EOF,
  };

  assert_token_types("$\"{{x}}\"", interpDoubledBracesExpected,
                     (int)(sizeof(interpDoubledBracesExpected) /
                           sizeof(interpDoubledBracesExpected[0])));

  TokenType interpBracesAroundPlaceholderExpected[] = {
      TOKEN_INTERP_START, // $"{{{
      TOKEN_IDENTIFIER,   // x
      TOKEN_INTERP_END,   // }}}"
      TOKEN_EOF,
  };

  assert_token_types("$\"{{{x}}}\"", interpBracesAroundPlaceholderExpected,
                     (int)(sizeof(interpBracesAroundPlaceholderExpected) /
                           sizeof(interpBracesAroundPlaceholderExpected[0])));

  TokenType loneDollarExpected[] = {
      TOKEN_ERROR,
      TOKEN_IDENTIFIER,
      TOKEN_EOF,
  };

  assert_token_types(
      "$x", loneDollarExpected,
      (int)(sizeof(loneDollarExpected) / sizeof(loneDollarExpected[0])));

  return 0;
}
