---
aliases:
  - Lexer
---
## Lexer

```c
TokenStream lex(const char *source);
```

## TokenStream

```c
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
```
## Tokens

```c
typedef struct {
	TokenType type;
	const char *start;
	int length;
	int line;
} Token;
```

### TokenType

| Idx | Token                     |         Example |
| --: | :------------------------ | --------------: |
|   0 | `TOKEN_LEFT_PAREN`        |             `(` |
|   1 | `TOKEN_RIGHT_PAREN`       |             `)` |
|   2 | `TOKEN_LEFT_BRACE`        |             `[` |
|   3 | `TOKEN_RIGHT_BRACE`       |             `]` |
|   4 | `TOKEN_LEFT_BRACKET`      |             `{` |
|   5 | `TOKEN_RIGHT_BRACKET`     |             `}` |
|   6 | `TOKEN_COMMA`             |             `,` |
|   7 | `TOKEN_COLON`             |             `:` |
|   8 | `TOKEN_DOT`               |             `.` |
|   9 | `TOKEN_MINUS`             |             `-` |
|  10 | `TOKEN_PLUS`              |             `+` |
|  11 | `TOKEN_SEMICOLON`         |             `;` |
|  12 | `TOKEN_SLASH`             |             `/` |
|  13 | `TOKEN_STAR`              |             `*` |
|  14 | `TOKEN_MODULO`            |             `%` |
|  15 | `TOKEN_BANG`              |             `!` |
|  16 | `TOKEN_BANG_EQUAL`        |            `!=` |
|  17 | `TOKEN_EQUAL`             |             `=` |
|  18 | `TOKEN_EQUAL_EQUAL`       |            `==` |
|  19 | `TOKEN_FAT_ARROW`         |            `=>` |
|  20 | `TOKEN_GREATER`           |             `>` |
|  21 | `TOKEN_GREATER_EQUAL`     |            `>=` |
|  22 | `TOKEN_LESS`              |             `<` |
|  23 | `TOKEN_LESS_EQUAL`        |            `<=` |
|  24 | `TOKEN_QUESTION_QUESTION` |            `??` |
|  25 | `TOKEN_IDENTIFIER`        | `StringBuilder` |
|  26 | `TOKEN_STRING`            | `"Hello World"` |
|  27 | `TOKEN_NUMBER`            |       `1234.56` |
|  28 | `TOKEN_AND`               |           `and` |
|  29 | `TOKEN_STRUCT`            |        `struct` |
|  30 | `TOKEN_IMPL`              |          `impl` |
|  31 | `TOKEN_TRAIT`             |         `trait` |
|  32 | `TOKEN_ELSE`              |          `else` |
|  33 | `TOKEN_FALSE`             |         `false` |
|  34 | `TOKEN_FOR`               |           `for` |
|  35 | `TOKEN_FUN`               |           `fun` |
|  36 | `TOKEN_IF`                |            `if` |
|  37 | `TOKEN_NIL`               |           `nil` |
|  38 | `TOKEN_OR`                |            `or` |
|  39 | `TOKEN_PRINT`             |         `print` |
|  40 | `TOKEN_PUB`               |           `pub` |
|  41 | `TOKEN_RETURN`            |        `return` |
|  42 | `TOKEN_SELF`              |          `self` |
|  43 | `TOKEN_TRUE`              |          `true` |
|  44 | `TOKEN_TYPE`              |          `type` |
|  45 | `TOKEN_VAR`               |           `var` |
|  46 | `TOKEN_LET`               |           `let` |
|  47 | `TOKEN_WHILE`             |         `while` |
|  48 | `TOKEN_BREAK`             |         `break` |
|  49 | `TOKEN_CONTINUE`          |      `continue` |
|  50 | `TOKEN_ERROR`             |             n/a |
|  51 | `TOKEN_EOF`               |             n/a |
