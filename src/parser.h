#ifndef clox_parser_h
#define clox_parser_h

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

#endif
