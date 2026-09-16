#ifndef kirby_resolved_impl_targets_h
#define kirby_resolved_impl_targets_h

#include "ast.h"
#include "token.h"
#include "types.h"

void resolvedImplTargetsReset(void);

// `structName` is stored as an (offset, length) pair into the shared name
// arena (see internTokenName/internedNameChars in types.c), not as a
// materialized pointer. The arena can move when later declarations intern
// more names, which would leave a raw pointer dangling -- see
// resolvedImplTargetsLookup.
void resolvedImplTargetsRecord(AstNode *implNode, InternedName structName);

// Returns NULL if `implNode` has no recorded target. The Token returned is
// rebuilt fresh from the arena's current address on every call, so callers
// must use it immediately and never cache it past a point where more names
// might be interned.
const Token *resolvedImplTargetsLookup(AstNode *implNode);

#endif
