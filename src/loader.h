#ifndef kirby_loader_h
#define kirby_loader_h

#include "compiled_unit.h"
#include "object.h"
#include "vm.h"

/**
 * Load a CompiledUnit's function and constants into runtime Obj instances on
 * the VM's heap.
 *
 * Returns the top-level script function (functions[0]).
 */
ObjFunction *loadUnit(VM *vm, const CompiledUnit *unit);

#endif
