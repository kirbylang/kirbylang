#include <stdlib.h>

#include "memory.h"
#include "token.h"
#include "token_stream.h"

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
    int oldCapacity = ts->capacity;
    ts->capacity = GROW_CAPACITY(oldCapacity);
    ts->tokens = GROW_ARRAY(Token, ts->tokens, oldCapacity, ts->capacity);
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
