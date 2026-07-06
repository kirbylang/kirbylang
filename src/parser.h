#ifndef clox_parser_h
#define clox_parser_h

#include <stdbool.h>

#include "ast.h"

// Parses `source` into a flat array of top-level declaration nodes.
// Individual nodes live in the AST arena (freed via astFreeAll()) — the
// caller owns only the returned array itself and must free() it.
//
// *outCount is set to the number of top-level declarations parsed.
// *hadError is set to true if any parse error was reported.
// *outEndLine is set to the line of the EOF token (the last line of the
// source) — the compiler needs this to tag the script's trailing implicit
// return the same way the line-tag would land on a real "last consumed
// token", matching the original single-pass compiler's behavior.
AstNode **parse(const char *source, int *outCount, bool *hadError,
                int *outEndLine);

#endif
