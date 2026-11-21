#ifndef kirby_top_level_h
#define kirby_top_level_h

#include <stdbool.h>

#include "ast.h"

/**
 * Checks the top level of a file: it may hold only declarations, and the
 * initializer of each `let` and `var` must be a comptime value. An entry file
 * also needs `fun main(): unit`. Reports every problem on stderr.
 *
 * Returns false if any error was reported.
 */
bool topLevelCheck(AstNode **ast, int count, bool isEntryFile);

#endif
