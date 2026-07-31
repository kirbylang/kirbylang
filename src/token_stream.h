#ifndef kirby_token_stream_h
#define kirby_token_stream_h

#include <stdbool.h>

#include "token.h"

typedef struct {
  Token *tokens;
  int count;
  int capacity;
  int current;
} TokenStream;

void tsInit(TokenStream *ts);
void tsFree(TokenStream *ts);
void tsWrite(TokenStream *ts, Token token);
Token tsPeek(TokenStream *ts);
Token tsPeekNext(TokenStream *ts);
Token tsAdvance(TokenStream *ts);
bool tsIsAtEnd(TokenStream *ts);

#endif // kirby_token_stream_h
