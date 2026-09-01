#ifndef kirby_definite_assignment_h
#define kirby_definite_assignment_h

#include <stdbool.h>

#include "ast.h"
#include "token.h"

// A set of identifier tokens waiting to be assigned a value
//
// Used in definite assignment analysis
typedef struct {
  Token *names;
  int count;
  int capacity;
} DaaSet;

void daaSetInit(DaaSet *daa);
void daaSetFree(DaaSet *daa);

// Returns true if the statement unconditionally exits (return/break/continue),
// meaning the rest of its enclosing block is unreachable..
bool daaCheckAssignmentStmt(DaaSet *daa, AstNode *node);

void daaCheckFn(FunctionNode *fn);

#endif
