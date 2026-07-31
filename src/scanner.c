#include "scanner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "assert.h"
#include "common.h"

static bool isDigit(char c);
static bool isAlpha(char c);
static char peek(Scanner *scanner);
static char advance(Scanner *scanner);
static bool isAtEnd(Scanner *scanner);
static char peekNext(Scanner *scanner);
static bool match(Scanner *scanner, char expected);
static Token makeToken(Scanner *scanner, TokenType type);
static Token errorToken(Scanner *scanner, const char *message);
static TokenType checkKeyword(Scanner *scanner, int start, int length,
                              const char *rest, TokenType type);
static TokenType identifierType(Scanner *scanner);
static Token identifier(Scanner *scanner);
static Token number(Scanner *scanner);
static Token string(Scanner *scanner);
static void skipWhitespace(Scanner *scanner);

/**
 * Initialize the scanner's state
 */
void initScanner(Scanner *scanner, const char *source) {
  TRACELN("scanner.initScanner()");

  scanner->start = source;
  scanner->current = source;
  scanner->line = 1;
}

/**
 * Advance scanner to the next character and return the associated token
 */
Token scanToken(Scanner *scanner) {
  TRACELN(ANSI_COLOR_GREEN "scanner.scanToken()" ANSI_COLOR_RESET);

  skipWhitespace(scanner);

  scanner->start = scanner->current;

  if (isAtEnd(scanner)) {
    TRACELN(ANSI_COLOR_GREEN
            "scanner.scanToken.isAtEnd = true" ANSI_COLOR_RESET);
    return makeToken(scanner, TOKEN_EOF);
  }

  assert(*scanner->current != '\0');

  char c = advance(scanner);

  if (isAlpha(c))
    return identifier(scanner);

  if (isDigit(c))
    return number(scanner);

  switch (c) {
  case '(':
    return makeToken(scanner, TOKEN_LEFT_PAREN);
  case ')':
    return makeToken(scanner, TOKEN_RIGHT_PAREN);
  case '{':
    return makeToken(scanner, TOKEN_LEFT_BRACE);
  case '}':
    return makeToken(scanner, TOKEN_RIGHT_BRACE);
  case '[':
    return makeToken(scanner, TOKEN_LEFT_BRACKET);
  case ']':
    return makeToken(scanner, TOKEN_RIGHT_BRACKET);
  case ';':
    return makeToken(scanner, TOKEN_SEMICOLON);
  case ',':
    return makeToken(scanner, TOKEN_COMMA);
  case ':':
    return makeToken(scanner, TOKEN_COLON);
  case '.':
    return makeToken(scanner, TOKEN_DOT);
  case '-':
    return makeToken(scanner, TOKEN_MINUS);
  case '+':
    return makeToken(scanner, TOKEN_PLUS);
  case '/':
    return makeToken(scanner, TOKEN_SLASH);
  case '*':
    return makeToken(scanner, TOKEN_STAR);
  case '?':
    return makeToken(scanner, match(scanner, '?') ? TOKEN_QUESTION_QUESTION
                                                  : TOKEN_ERROR);
  case '&':
    return makeToken(scanner, match(scanner, '&') ? TOKEN_AND : TOKEN_ERROR);
  case '|':
    return makeToken(scanner, match(scanner, '|') ? TOKEN_OR : TOKEN_ERROR);
  case '!':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
  case '=':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
  case '<':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
  case '>':
    return makeToken(scanner,
                     match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
  case '%':
    return makeToken(scanner, TOKEN_MODULO);
  case '"':
    return string(scanner);
  }

  return errorToken(scanner, "Unexpected character.");
}

static bool isDigit(char c) { return c >= '0' && c <= '9'; }

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static char peek(Scanner *scanner) {
  char peeked = *scanner->current;

  TRACELN("scanner.peek() -> '%c'", peeked);

  return peeked;
}

static char advance(Scanner *scanner) {
  TRACELN("scanner.advance()");
  if (isAtEnd(scanner))
    return '\0';
  scanner->current++;
  return scanner->current[-1];
}

static bool isAtEnd(Scanner *scanner) { return *scanner->current == '\0'; }

static char peekNext(Scanner *scanner) {
  TRACELN("scanner.peekNext()");
  if (isAtEnd(scanner) || scanner->current[1] == '\0')
    return '\0';
  return scanner->current[1];
}

static bool match(Scanner *scanner, char expected) {
  TRACELN("scanner.match(%s)", &expected);

  if (isAtEnd(scanner))
    return false;
  if (*scanner->current != expected)
    return false;
  scanner->current++;
  return true;
}

static Token makeToken(Scanner *scanner, TokenType type) {
  const char *start = scanner->start;
  int length = (int)(scanner->current - scanner->start);

#ifdef DEBUG_TRACE_EXECUTION
  if (type == TOKEN_IDENTIFIER) {
    const char *typeString = tokenTypeToString(type);
    TRACELN("scanner.makeToken() -> %s = '%.*s'", typeString, length, start);
  } else {
    const char *typeString = tokenTypeToString(type);
    TRACELN("scanner.makeToken() -> %s", typeString);
  }
#endif // DEBUG_TRACE_EXECUTION

  Token token;
  token.type = type;
  token.start = start;
  token.length = length;
  token.line = scanner->line;
  return token;
}

static Token errorToken(Scanner *scanner, const char *message) {
  TRACELN("errorToken() -> %s", message);
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner->line;
  return token;
}

static TokenType checkKeyword(Scanner *scanner, int start, int length,
                              const char *rest, TokenType type) {
  if (scanner->current - scanner->start == start + length &&
      memcmp(scanner->start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static TokenType identifierType(Scanner *scanner) {
  switch (scanner->start[0]) {
  case 'a':
    return checkKeyword(scanner, 1, 2, "nd", TOKEN_AND);
  case 'b':
    return checkKeyword(scanner, 1, 4, "reak", TOKEN_BREAK);
  case 'e':
    return checkKeyword(scanner, 1, 3, "lse", TOKEN_ELSE);
  case 'f':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return checkKeyword(scanner, 2, 3, "lse", TOKEN_FALSE);
      case 'o':
        return checkKeyword(scanner, 2, 1, "r", TOKEN_FOR);
      case 'u':
        return checkKeyword(scanner, 2, 1, "n", TOKEN_FUN);
      }
    }
    break;
  case 'i':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'f':
        return checkKeyword(scanner, 1, 1, "f", TOKEN_IF);
      case 'm':
        return checkKeyword(scanner, 1, 3, "mpl", TOKEN_IMPL);
      }
    }
    break;
  case 'n':
    return checkKeyword(scanner, 1, 2, "il", TOKEN_NIL);
  case 'o':
    return checkKeyword(scanner, 1, 1, "r", TOKEN_OR);
  case 'p':
    return checkKeyword(scanner, 1, 4, "rint", TOKEN_PRINT);
  case 'r':
    return checkKeyword(scanner, 1, 5, "eturn", TOKEN_RETURN);
  case 's':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'e':
        return checkKeyword(scanner, 2, 2, "lf", TOKEN_SELF);
      case 't':
        return checkKeyword(scanner, 2, 4, "ruct", TOKEN_STRUCT);
      }
    }
    break;
  case 't':
    return checkKeyword(scanner, 1, 3, "rue", TOKEN_TRUE);
  case 'v':
    return checkKeyword(scanner, 1, 2, "ar", TOKEN_VAR);
  case 'w':
    return checkKeyword(scanner, 1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner *scanner) {
  TRACELN("scanner.identifier()");

  while (isAlpha(peek(scanner)) || isDigit(peek(scanner)))
    advance(scanner);
  return makeToken(scanner, identifierType(scanner));
}

static Token number(Scanner *scanner) {
  TRACELN("scanner.number()");

  while (isDigit(peek(scanner)))
    advance(scanner);

  // Look for a fractional part.
  if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
    // Consume the ".".
    advance(scanner);

    while (isDigit(peek(scanner)))
      advance(scanner);
  }

  return makeToken(scanner, TOKEN_NUMBER);
}

static Token string(Scanner *scanner) {
  TRACELN("scanner.string()");

  while (peek(scanner) != '"' && !isAtEnd(scanner)) {
    if (peek(scanner) == '\n') {
      scanner->line++;
    }

    if (peek(scanner) == '\\' && peekNext(scanner) != '\0') {
      advance(scanner);
    }

    advance(scanner);
  }

  if (isAtEnd(scanner))
    return errorToken(scanner, "Unterminated string.");

  // The closing quote.
  advance(scanner);
  return makeToken(scanner, TOKEN_STRING);
}

static void skipWhitespace(Scanner *scanner) {
  TRACELN("\nscanner.skipWhitespace()\n");

  for (;;) {
    char c = peek(scanner);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;
    case '\n':
      scanner->line++;
      advance(scanner);
      break;
    case '#':
      if (peekNext(scanner) == '!') {
        while (peek(scanner) != '\n' && !isAtEnd(scanner))
          advance(scanner);
      } else {
        return;
      }
      break;
    case '/':
      if (peekNext(scanner) == '/') {
        while (peek(scanner) != '\n' && !isAtEnd(scanner))
          advance(scanner);
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}
