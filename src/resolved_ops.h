#ifndef kirby_resolved_ops_h
#define kirby_resolved_ops_h

#include <stdbool.h>

#include "ast.h"

// Bridges the type checker's static resolution to the compiler for the
// handful of cases where the compiler can't produce correct bytecode from
// the AST alone -- primitive method calls (Phase 4b). Every other AstNode
// has no entry here, and the compiler falls back to its ordinary,
// type-blind codegen.
//
// Entries are keyed by AstNode* identity. That's only safe because the
// checker and the compiler run over the exact same in-memory AST within
// one compileSource() call (see main.c): resolvedOpsReset() must be
// called once before each such call, and never in the middle of one, or
// stale entries could be looked up against an unrelated later AST that
// happens to reuse a freed node's address.
typedef enum {
  // A call resolved to a primitive's impl/trait-impl method -- compile it
  // as a direct call to a mangled global instead of OP_INVOKE.
  RESOLVED_OP_PRIMITIVE_CALL,
} ResolvedOpKind;

typedef struct {
  ResolvedOpKind kind;
  // True if the receiver must be pushed as an extra leading argument (an
  // instance method call, e.g. `x.toString()`); false for a bare static
  // call with no receiver at all (e.g. `f64.default()`).
  bool hasSelf;
  // The global name to call, e.g. "@f64.Display.toString" -- see
  // mangle.h. Owned by whichever allocator the checker used (the types
  // arena); resolved_ops itself never allocates or frees this memory.
  const char *mangledName;
  int mangledLength;
} ResolvedOp;

void resolvedOpsReset(void);
void resolvedOpsRecord(AstNode *node, ResolvedOp op);

// Returns NULL if `node` has no recorded resolution.
const ResolvedOp *resolvedOpsLookup(AstNode *node);

#endif
