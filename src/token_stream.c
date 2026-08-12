#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "token.h"
#include "token_stream.h"

#define TS_GROW_SIZE 2
#define TS_MIN_SIZE 8

void tsInit(TokenStream *ts) {
  ts->count = 0;
  ts->capacity = 0;
  ts->current = 0;
  ts->tokens = 0;
}

void tsFree(TokenStream *ts) {
  free(ts->tokens);
  tsInit(ts);
}

void tsWrite(TokenStream *ts, Token token) {
  if (ts->capacity < ts->count + 1) {
    ts->capacity =
        ts->capacity < TS_MIN_SIZE ? TS_MIN_SIZE : ts->capacity * TS_GROW_SIZE;
    ts->tokens = realloc(ts->tokens, sizeof(Token) * ts->capacity);

    if (ts->tokens == NULL) {
      fprintf(stderr, "realloc failed in tsWrite");
      exit(EXIT_CODE_OS_ERR);
    }
  }

  ts->tokens[ts->count] = token;
  ts->count++;
}

Token tsPeek(TokenStream *ts) { return ts->tokens[ts->current]; }

Token tsPeekNext(TokenStream *ts) {
  if (ts->current + 1 >= ts->count)
    return ts->tokens[ts->count - 1];
  return ts->tokens[ts->current + 1];
}

Token tsAdvance(TokenStream *ts) { return ts->tokens[ts->current++]; }

bool tsIsAtEnd(TokenStream *ts) {
  return ts->tokens[ts->current].type == TOKEN_EOF;
}
