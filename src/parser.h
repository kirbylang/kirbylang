#ifndef clox_parser_h
#define clox_parser_h

#include "ast.h"

AstNode **parse(const char *source, int *outCount, bool *hadError);

#endif