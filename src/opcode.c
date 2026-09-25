#include "opcode.h"

#include <stddef.h>

// Must match how vm.c runs each instruction, because the compiler tracks the
// stack height from these.
static const OpInfo opInfos[OP_COUNT] = {
    // isKnownOp, operandBytes, stackEffect, countOperand, perCount
    [OP_CONSTANT] = {true, 1, 1, -1, 0},
    [OP_NIL] = {true, 0, 1, -1, 0},
    [OP_TRUE] = {true, 0, 1, -1, 0},
    [OP_FALSE] = {true, 0, 1, -1, 0},
    [OP_ADD] = {true, 0, -1, -1, 0},
    [OP_SUBTRACT] = {true, 0, -1, -1, 0},
    [OP_MULTIPLY] = {true, 0, -1, -1, 0},
    [OP_DIVIDE] = {true, 0, -1, -1, 0},
    [OP_MODULO] = {true, 0, -1, -1, 0},
    [OP_NEGATE] = {true, 0, 0, -1, 0},
    [OP_PRINT] = {true, 0, -1, -1, 0},
    [OP_RETURN] = {true, 0, -1, -1, 0},
    [OP_EQUAL] = {true, 0, -1, -1, 0},
    [OP_NOT] = {true, 0, 0, -1, 0},
    [OP_POP] = {true, 0, -1, -1, 0},
    [OP_CLOSE_UPVALUE] = {true, 0, -1, -1, 0},
    [OP_DEFINE_GLOBAL] = {true, 1, -1, -1, 0},
    [OP_GET_GLOBAL] = {true, 1, 1, -1, 0},
    [OP_SET_GLOBAL] = {true, 1, 0, -1, 0},
    [OP_GET_UPVALUE] = {true, 1, 1, -1, 0},
    [OP_SET_UPVALUE] = {true, 1, 0, -1, 0},
    [OP_GET_LOCAL] = {true, 1, 1, -1, 0},
    [OP_SET_LOCAL] = {true, 1, 0, -1, 0},
    [OP_JUMP_IF_FALSE] = {true, 2, 0, -1, 0},
    [OP_JUMP_IF_NOT_NIL] = {true, 2, 0, -1, 0},
    [OP_JUMP] = {true, 2, 0, -1, 0},
    [OP_LOOP] = {true, 2, 0, -1, 0},
    [OP_GREATER] = {true, 0, -1, -1, 0},
    [OP_LESS] = {true, 0, -1, -1, 0},
    // The callee and its arguments become the result.
    [OP_CALL] = {true, 1, 0, 0, -1},
    [OP_CLOSURE] = {true, 1, 1, -1, 0},
    [OP_STRUCT] = {true, 1, 1, -1, 0},
    // The struct and a name and value per field become the instance.
    [OP_STRUCT_INIT] = {true, 1, 0, 0, -2},
    [OP_FIELD] = {true, 2, -1, -1, 0},
    [OP_GET_PROPERTY] = {true, 1, 0, -1, 0},
    [OP_SET_PROPERTY] = {true, 1, -1, -1, 0},
    [OP_METHOD] = {true, 1, -1, -1, 0},
    // The receiver and its arguments become the result.
    [OP_INVOKE] = {true, 2, 0, 1, -1},
    [OP_ARRAY] = {true, 1, 1, 0, -1},
    [OP_GET_INDEX] = {true, 0, -1, -1, 0},
    [OP_SET_INDEX] = {true, 0, -2, -1, 0},
    // The block's locals are removed from under its result.
    [OP_CLOSE_BLOCK_EXPR] = {true, 1, 0, 0, -1},
};

const OpInfo *opInfo(OpCode op) {
  if (op < 0 || op >= OP_COUNT)
    return NULL;

  return &opInfos[op];
}
