#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count)                                                  \
  (type *)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount)                          \
  (type *)reallocate(pointer, sizeof(type) * (oldCount),                       \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount)                                    \
  reallocate(pointer, sizeof(type) * (oldCount), 0)

/**
 * Reallocate a block of memory
 *
 * - Triggers garbage collection check when allocating more memory
 */
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

/**
 * Mark object as reachable/used
 */
void markObject(Obj *object);

/**
 * Mark value as reachable/used
 *
 * This only happens if the value is an object
 */
void markValue(Value value);

/**
 * Check for unreachable/unsed objects and free them from memory
 */
void collectGarbage(void);

/**
 * Free object from memory
 *
 * This happens when the VM is shutting down
 */
void freeObjects(void);

#endif
