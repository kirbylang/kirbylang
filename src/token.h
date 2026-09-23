#ifndef kirby_token_h
#define kirby_token_h

#include <stdbool.h>

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_PLUS,
  TOKEN_SEMICOLON,
  TOKEN_SLASH,
  TOKEN_STAR,
  TOKEN_MODULO,
  // One or two character tokens.
  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_FAT_ARROW,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,
  TOKEN_QUESTION_QUESTION,
  // Literals.
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,
  // Keywords.
  TOKEN_AND,
  TOKEN_STRUCT,
  TOKEN_IMPL,
  TOKEN_TRAIT,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_FUN,
  TOKEN_IF,
  TOKEN_NIL,
  TOKEN_OR,
  TOKEN_PRINT,
  TOKEN_PUB,
  TOKEN_RETURN,
  TOKEN_SELF,
  TOKEN_TRUE,
  TOKEN_TYPE,
  TOKEN_VAR,
  TOKEN_LET,
  TOKEN_WHILE,
  TOKEN_BREAK,
  TOKEN_CONTINUE,

  TOKEN_ERROR,
  TOKEN_EOF,
  NumberOfDefinedTokens
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

const char *tokenTypeToString(TokenType type);

// True when both tokens have the same text
bool tokensEqual(const Token *a, const Token *b);

// True when the token's text is exactly `text`
bool tokenTextEquals(const Token *token, const char *text);

// An identifier token for a C string. The token does not copy `text`, so it
// must outlive the token.
Token tokenFromCString(const char *text);

// True for the names of the types every program has: unit, bool, string, f64
// and Array
bool tokenIsPrimitiveTypeName(const Token *token);

#endif
