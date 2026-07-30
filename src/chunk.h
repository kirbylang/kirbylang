#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_NEGATE,
  OP_PRINT,
  OP_RETURN,
  OP_EQUAL,
  OP_NOT,
  OP_POP,
  OP_CLOSE_UPVALUE,
  OP_DEFINE_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_NOT_NIL, // Jump for nullish coalescing
  OP_JUMP,
  OP_LOOP,
  OP_GREATER,
  OP_LESS,
  OP_CALL,
  OP_CLOSURE,
  OP_STRUCT,
  OP_STRUCT_INIT, // The struct's { field: value, } intializer
  OP_FIELD,
  OP_GET_PROPERTY,
  OP_SET_PROPERTY,
  OP_METHOD,
  OP_INVOKE,
  OP_ARRAY,
  OP_GET_INDEX,
  OP_SET_INDEX,
  OP_CLOSE_BLOCK_EXPR,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk);
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line);

int addConstantToChunk(Chunk *chunk, Value value);

#endif
