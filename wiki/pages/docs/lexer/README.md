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
|   2 | `TOKEN_LEFT_BRACE`        |             `{` |
|   3 | `TOKEN_RIGHT_BRACE`       |             `}` |
|   4 | `TOKEN_LEFT_BRACKET`      |             `[` |
|   5 | `TOKEN_RIGHT_BRACKET`     |             `]` |
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
|  28 | `TOKEN_INTERP_STRING`     |      `$"Hello"` |
|  29 | `TOKEN_INTERP_START`      |     `$"Hello {` |
|  30 | `TOKEN_INTERP_MIDDLE`     |          `}, {` |
|  31 | `TOKEN_INTERP_END`        |           `}!"` |
|  32 | `TOKEN_AND`               |           `and` |
|  33 | `TOKEN_STRUCT`            |        `struct` |
|  34 | `TOKEN_IMPL`              |          `impl` |
|  35 | `TOKEN_TRAIT`             |         `trait` |
|  36 | `TOKEN_ELSE`              |          `else` |
|  37 | `TOKEN_FALSE`             |         `false` |
|  38 | `TOKEN_FOR`               |           `for` |
|  39 | `TOKEN_FUN`               |           `fun` |
|  40 | `TOKEN_IF`                |            `if` |
|  41 | `TOKEN_NIL`               |           `nil` |
|  42 | `TOKEN_OR`                |            `or` |
|  43 | `TOKEN_PRINT`             |         `print` |
|  44 | `TOKEN_PUB`               |           `pub` |
|  45 | `TOKEN_RETURN`            |        `return` |
|  46 | `TOKEN_SELF`              |          `self` |
|  47 | `TOKEN_TRUE`              |          `true` |
|  48 | `TOKEN_TYPE`              |          `type` |
|  49 | `TOKEN_VAR`               |           `var` |
|  50 | `TOKEN_LET`               |           `let` |
|  51 | `TOKEN_WHILE`             |         `while` |
|  52 | `TOKEN_BREAK`             |         `break` |
|  53 | `TOKEN_CONTINUE`          |      `continue` |
|  54 | `TOKEN_ERROR`             |             n/a |
|  55 | `TOKEN_EOF`               |             n/a |
