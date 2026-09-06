#ifndef kirby_mangle_h
#define kirby_mangle_h

#include "token.h"

// Mangled names use '@' and '.', which the scanner never produces inside
// an identifier -- so a mangled name can never collide with a real Kirby
// global, no matter what the user names things.
#define MANGLED_NAME_MAX 128

// Writes the mangled global name for a primitive impl method into
// `buffer` (which must be at least MANGLED_NAME_MAX bytes) and returns
// its length.
//
// `traitName` is NULL for a method from a plain `impl f64 { ... }` block
// (produces "@f64.helper"), or the owning trait's name token for a
// method from `impl Trait for f64 { ... }` (produces
// "@f64.Display.toString").
//
// This has no dependency on the type system on purpose -- it's shared by
// the type checker (which records the name into the resolved-ops side
// table at a call site -- see resolved_ops.h) and the compiler (which
// builds the same name again, from the same AST fields, when compiling
// the impl block itself), and the compiler stays type-blind otherwise.
// Both sides passing the same inputs through the same function is what
// guarantees they always agree on the global to use.
int mangledPrimitiveMethodName(char *buffer, const char *primitiveTypeName,
                               int primitiveTypeNameLength,
                               const Token *traitName, const Token *methodName);

#endif
