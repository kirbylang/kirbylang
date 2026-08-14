#ifndef kirby_chunk_h
#define kirby_chunk_h

#include "common.h"
#include "gc.h"
#include "opcode.h"
#include "value.h"

/**
 * A runtime bytecode chunk: the compiled bytecode, line information, and
 * constant pool for one function.
 */
typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines;
  ValueArray constants;
} Chunk;

/**
 * Initialize a new chunk
 */
void initChunk(Chunk *chunk);

/**
 * Free a chunk's memory
 */
void freeChunk(GC *gc, Chunk *chunk);

/**
 * Write a single byte to the chunk
 */
void writeChunk(GC *gc, Chunk *chunk, uint8_t byte, int line);

/**
 * Add a new value to a chunk's constant pool
 */
int addConstantToChunk(GC *gc, Chunk *chunk, Value value);

#endif
