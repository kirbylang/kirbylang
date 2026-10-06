#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/token.h"

static void test_tokens_equal(void) {
  Token abc = tokenFromCString("abc");
  Token abcAgain = tokenFromCString("abc");
  Token abd = tokenFromCString("abd");
  Token ab = tokenFromCString("ab");

  assert(tokensEqual(&abc, &abcAgain));
  assert(!tokensEqual(&abc, &abd));
  assert(!tokensEqual(&abc, &ab));
}

static void test_token_text_equals(void) {
  Token self = tokenFromCString("Self");

  assert(tokenTextEquals(&self, "Self"));
  assert(!tokenTextEquals(&self, "Sel"));
  assert(!tokenTextEquals(&self, "Selff"));
  assert(!tokenTextEquals(&self, "self"));
}

static void test_token_from_c_string(void) {
  Token token = tokenFromCString("hello");

  assert(token.type == TOKEN_IDENTIFIER);
  assert(token.length == 5);
  assert(memcmp(token.start, "hello", 5) == 0);
  assert(token.line == 0);
}

static void test_primitive_type_names(void) {
  const char *primitives[] = {"unit", "bool", "string", "f64", "Array"};
  for (int i = 0; i < 5; i++) {
    Token name = tokenFromCString(primitives[i]);
    assert(tokenIsPrimitiveTypeName(&name));
  }

  Token point = tokenFromCString("Point");
  Token number = tokenFromCString("number");
  assert(!tokenIsPrimitiveTypeName(&point));
  assert(!tokenIsPrimitiveTypeName(&number));
}

static void test_token_to_cstring(void) {
  assert(strcmp("TOKEN_LEFT_PAREN", tokenTypeToString(TOKEN_LEFT_PAREN)) == 0);
  assert(strcmp("TOKEN_RIGHT_PAREN", tokenTypeToString(TOKEN_RIGHT_PAREN)) ==
         0);
  assert(strcmp("TOKEN_LEFT_BRACE", tokenTypeToString(TOKEN_LEFT_BRACE)) == 0);
  assert(strcmp("TOKEN_RIGHT_BRACE", tokenTypeToString(TOKEN_RIGHT_BRACE)) ==
         0);
  assert(strcmp("TOKEN_LEFT_BRACKET", tokenTypeToString(TOKEN_LEFT_BRACKET)) ==
         0);
  assert(strcmp("TOKEN_RIGHT_BRACKET",
                tokenTypeToString(TOKEN_RIGHT_BRACKET)) == 0);
  assert(strcmp("TOKEN_COMMA", tokenTypeToString(TOKEN_COMMA)) == 0);
  assert(strcmp("TOKEN_COLON", tokenTypeToString(TOKEN_COLON)) == 0);
  assert(strcmp("TOKEN_DOT", tokenTypeToString(TOKEN_DOT)) == 0);
  assert(strcmp("TOKEN_MINUS", tokenTypeToString(TOKEN_MINUS)) == 0);
  assert(strcmp("TOKEN_PLUS", tokenTypeToString(TOKEN_PLUS)) == 0);
  assert(strcmp("TOKEN_SEMICOLON", tokenTypeToString(TOKEN_SEMICOLON)) == 0);
  assert(strcmp("TOKEN_SLASH", tokenTypeToString(TOKEN_SLASH)) == 0);
  assert(strcmp("TOKEN_STAR", tokenTypeToString(TOKEN_STAR)) == 0);
  assert(strcmp("TOKEN_MODULO", tokenTypeToString(TOKEN_MODULO)) == 0);
  assert(strcmp("TOKEN_BANG", tokenTypeToString(TOKEN_BANG)) == 0);
  assert(strcmp("TOKEN_BANG_EQUAL", tokenTypeToString(TOKEN_BANG_EQUAL)) == 0);
  assert(strcmp("TOKEN_EQUAL", tokenTypeToString(TOKEN_EQUAL)) == 0);
  assert(strcmp("TOKEN_EQUAL_EQUAL", tokenTypeToString(TOKEN_EQUAL_EQUAL)) ==
         0);
  assert(strcmp("TOKEN_FAT_ARROW", tokenTypeToString(TOKEN_FAT_ARROW)) == 0);
  assert(strcmp("TOKEN_GREATER", tokenTypeToString(TOKEN_GREATER)) == 0);
  assert(strcmp("TOKEN_GREATER_EQUAL",
                tokenTypeToString(TOKEN_GREATER_EQUAL)) == 0);
  assert(strcmp("TOKEN_LESS", tokenTypeToString(TOKEN_LESS)) == 0);
  assert(strcmp("TOKEN_LESS_EQUAL", tokenTypeToString(TOKEN_LESS_EQUAL)) == 0);
  assert("TOKEN_QUESTION_QUESTION" ==
         tokenTypeToString(TOKEN_QUESTION_QUESTION));
  assert(strcmp("TOKEN_IDENTIFIER", tokenTypeToString(TOKEN_IDENTIFIER)) == 0);
  assert(strcmp("TOKEN_STRING", tokenTypeToString(TOKEN_STRING)) == 0);
  assert(strcmp("TOKEN_NUMBER", tokenTypeToString(TOKEN_NUMBER)) == 0);
  assert(strcmp("TOKEN_INTERP_STRING",
                tokenTypeToString(TOKEN_INTERP_STRING)) == 0);
  assert(strcmp("TOKEN_INTERP_START", tokenTypeToString(TOKEN_INTERP_START)) ==
         0);
  assert(strcmp("TOKEN_INTERP_MIDDLE",
                tokenTypeToString(TOKEN_INTERP_MIDDLE)) == 0);
  assert(strcmp("TOKEN_INTERP_END", tokenTypeToString(TOKEN_INTERP_END)) == 0);
  assert(strcmp("TOKEN_AND", tokenTypeToString(TOKEN_AND)) == 0);
  assert(strcmp("TOKEN_STRUCT", tokenTypeToString(TOKEN_STRUCT)) == 0);
  assert(strcmp("TOKEN_IMPL", tokenTypeToString(TOKEN_IMPL)) == 0);
  assert(strcmp("TOKEN_TRAIT", tokenTypeToString(TOKEN_TRAIT)) == 0);
  assert(strcmp("TOKEN_ELSE", tokenTypeToString(TOKEN_ELSE)) == 0);
  assert(strcmp("TOKEN_FALSE", tokenTypeToString(TOKEN_FALSE)) == 0);
  assert(strcmp("TOKEN_FOR", tokenTypeToString(TOKEN_FOR)) == 0);
  assert(strcmp("TOKEN_FUN", tokenTypeToString(TOKEN_FUN)) == 0);
  assert(strcmp("TOKEN_IF", tokenTypeToString(TOKEN_IF)) == 0);
  assert(strcmp("TOKEN_NIL", tokenTypeToString(TOKEN_NIL)) == 0);
  assert(strcmp("TOKEN_OR", tokenTypeToString(TOKEN_OR)) == 0);
  assert(strcmp("TOKEN_PUB", tokenTypeToString(TOKEN_PUB)) == 0);
  assert(strcmp("TOKEN_RETURN", tokenTypeToString(TOKEN_RETURN)) == 0);
  assert(strcmp("TOKEN_SELF", tokenTypeToString(TOKEN_SELF)) == 0);
  assert(strcmp("TOKEN_TRUE", tokenTypeToString(TOKEN_TRUE)) == 0);
  assert(strcmp("TOKEN_TYPE", tokenTypeToString(TOKEN_TYPE)) == 0);
  assert(strcmp("TOKEN_VAR", tokenTypeToString(TOKEN_VAR)) == 0);
  assert(strcmp("TOKEN_LET", tokenTypeToString(TOKEN_LET)) == 0);
  assert(strcmp("TOKEN_WHILE", tokenTypeToString(TOKEN_WHILE)) == 0);
  assert(strcmp("TOKEN_BREAK", tokenTypeToString(TOKEN_BREAK)) == 0);
  assert(strcmp("TOKEN_CONTINUE", tokenTypeToString(TOKEN_CONTINUE)) == 0);
  assert(strcmp("TOKEN_EOF", tokenTypeToString(TOKEN_EOF)) == 0);
  assert(strcmp("TOKEN_ERROR", tokenTypeToString(TOKEN_ERROR)) == 0);
  assert(strcmp("UNKNOWN TOKEN",
                tokenTypeToString(NumberOfDefinedTokens + 1)) == 0);
}

int main(void) {
  assert(NumberOfDefinedTokens == 55);
  test_token_to_cstring();
  test_tokens_equal();
  test_token_text_equals();
  test_token_from_c_string();
  test_primitive_type_names();
  return 0;
}
