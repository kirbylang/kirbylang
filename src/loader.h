#ifndef kirby_loader_h
#define kirby_loader_h

#include "compiled_unit.h"
#include "object.h"
#include "vm.h"

/**
 * Materialize a flat CompiledUnit into live runtime objects on the VM's heap.
 * This is the ONLY code that turns the compiler's heap-free artifact into
 * ObjFunctions/ObjStrings: it is the sole bridge between the decoupled compiler
 * and the GC.
 *
 * Returns the top-level script function (functions[0]).
 */
ObjFunction *loadUnit(VM *vm, const CompiledUnit *unit);

#endif
