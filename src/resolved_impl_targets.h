#ifndef kirby_resolved_impl_targets_h
#define kirby_resolved_impl_targets_h

#include "ast.h"
#include "token.h"

void resolvedImplTargetsReset(void);
void resolvedImplTargetsRecord(AstNode *implNode, Token structName);

const Token *resolvedImplTargetsLookup(AstNode *implNode);

#endif
