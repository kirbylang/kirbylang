#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/token.h"

int main(void) {
  assert(NumberOfDefinedTokens == 44);
  assert(strcmp(tokenTypeToString(TOKEN_AND), "TOKEN_AND") == 0);
  return 0;
}
