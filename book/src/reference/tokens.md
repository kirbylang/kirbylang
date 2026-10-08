# Tokens

## Generate Tokens

```c
// src/lexer.h

TokenStream lex(const char *source);
```

```c
// src/token_stream.h

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

```c
// src/token.h

typedef struct {
	TokenType type;
	const char *start;
	int length;
	int line;
} Token;
```

## TokenType

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
|  28 | `TOKEN_F64`               |           `f64` |
|  29 | `TOKEN_INTERP_STRING`     |      `$"Hello"` |
|  30 | `TOKEN_INTERP_START`      |     `$"Hello {` |
|  31 | `TOKEN_INTERP_MIDDLE`     |          `}, {` |
|  32 | `TOKEN_INTERP_END`        |           `}!"` |
|  33 | `TOKEN_AND`               |           `and` |
|  34 | `TOKEN_STRUCT`            |        `struct` |
|  35 | `TOKEN_IMPL`              |          `impl` |
|  36 | `TOKEN_TRAIT`             |         `trait` |
|  37 | `TOKEN_ELSE`              |          `else` |
|  38 | `TOKEN_FALSE`             |         `false` |
|  39 | `TOKEN_FOR`               |           `for` |
|  40 | `TOKEN_FUN`               |           `fun` |
|  41 | `TOKEN_IF`                |            `if` |
|  42 | `TOKEN_NIL`               |           `nil` |
|  43 | `TOKEN_OR`                |            `or` |
|  44 | `TOKEN_PRINT`             |         `print` |
|  45 | `TOKEN_PUB`               |           `pub` |
|  46 | `TOKEN_RETURN`            |        `return` |
|  47 | `TOKEN_SELF`              |          `self` |
|  48 | `TOKEN_TRUE`              |          `true` |
|  49 | `TOKEN_TYPE`              |          `type` |
|  50 | `TOKEN_VAR`               |           `var` |
|  51 | `TOKEN_LET`               |           `let` |
|  52 | `TOKEN_WHILE`             |         `while` |
|  53 | `TOKEN_BREAK`             |         `break` |
|  54 | `TOKEN_CONTINUE`          |      `continue` |
|  55 | `TOKEN_ERROR`             |             n/a |
|  56 | `TOKEN_EOF`               |             n/a |
