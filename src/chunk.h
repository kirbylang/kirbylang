#ifndef kirby_chunk_h
#define kirby_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,         // 0
  OP_NIL,              // 1
  OP_TRUE,             // 2
  OP_FALSE,            // 3
  OP_ADD,              // 4
  OP_SUBTRACT,         // 5
  OP_MULTIPLY,         // 6
  OP_DIVIDE,           // 7
  OP_MODULO,           // 8
  OP_NEGATE,           // 9
  OP_PRINT,            // 10
  OP_RETURN,           // 11
  OP_EQUAL,            // 12
  OP_NOT,              // 13
  OP_POP,              // 14
  OP_CLOSE_UPVALUE,    // 15
  OP_DEFINE_GLOBAL,    // 16
  OP_GET_GLOBAL,       // 17
  OP_SET_GLOBAL,       // 18
  OP_GET_UPVALUE,      // 19
  OP_SET_UPVALUE,      // 20
  OP_GET_LOCAL,        // 21
  OP_SET_LOCAL,        // 22
  OP_JUMP_IF_FALSE,    // 23
  OP_JUMP_IF_NOT_NIL,  // 24; Jump for nullish coalescing
  OP_JUMP,             // 25
  OP_LOOP,             // 26
  OP_GREATER,          // 27
  OP_LESS,             // 28
  OP_CALL,             // 29
  OP_CLOSURE,          // 30
  OP_STRUCT,           // 31
  OP_STRUCT_INIT,      // 32; The struct's { field: value, } intializer
  OP_FIELD,            // 33
  OP_GET_PROPERTY,     // 34
  OP_SET_PROPERTY,     // 35
  OP_METHOD,           // 36
  OP_INVOKE,           // 37
  OP_ARRAY,            // 38
  OP_GET_INDEX,        // 39
  OP_SET_INDEX,        // 40
  OP_CLOSE_BLOCK_EXPR, // 41
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
