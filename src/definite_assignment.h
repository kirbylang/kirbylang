#ifndef kirby_definite_assignment_h
#define kirby_definite_assignment_h

#include <stdbool.h>

#include "ast.h"
#include "token.h"

// Names still awaiting their first assignment at some point in the walk.
typedef struct {
  Token *names;
  int count;
  int capacity;
} PendingSet;

void pendingSetInit(PendingSet *set);
void pendingSetFree(PendingSet *set);

// Returns true if the statement unconditionally exits (return/break/continue),
// meaning the rest of its enclosing block is unreachable..
bool checkAssignmentStmt(PendingSet *pending, AstNode *node);

void checkDefiniteAssignment(FunctionNode *fn);

#endif
