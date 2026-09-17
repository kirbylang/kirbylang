#ifndef kirby_resolved_impl_targets_h
#define kirby_resolved_impl_targets_h

#include "ast.h"
#include "token.h"
#include "types.h"

void resolvedImplTargetsReset(void);

void resolvedImplTargetsRecord(AstNode *implNode, InternedName structName);

const Token *resolvedImplTargetsLookup(AstNode *implNode);

#endif
