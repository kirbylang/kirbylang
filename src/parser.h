#ifndef kirby_parser_h
#define kirby_parser_h

#include <stdbool.h>

#include "ast.h"

/**
 * Parses `source` into a flat array of AST nodes.
 *
 * The caller must call astFreeAll() to free AST arena memory after use.
 *
 * @param source The source code to parse
 * @param outCount The number of top-level declarations parsed.
 * @param hadError If any parse error was reported.
 * @param outEndLine The line of the EOF token
 */
AstNode **parse(const char *source, int *outCount, bool *hadError,
                int *outEndLine);

/**
 * Parses `source` as a single type, such as `fun ([T]) => i64`.
 *
 * Returns NULL if the text isn't exactly one type. The caller must call
 * astFreeAll() to free AST arena memory after use.
 */
AstNode *parseTypeText(const char *source, bool *hadError);

#endif
