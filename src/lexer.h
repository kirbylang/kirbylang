#ifndef clox_lexer_h

#include "scanner.h"
#include "token_stream.h"

/**
 * @brief Lexes a given source code and returns a token array
 * @param source The source code to lex
 * @return A token array containing the lexed tokens
 */
TokenStream lex(const char *source);

#define clox_lexer_h
#endif // clox_lexer_h
