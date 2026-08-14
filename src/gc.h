#ifndef kirby_gc_h
#define kirby_gc_h

#include "common.h"
#include "hashtable.h"

struct Obj;
struct ObjString;

/**
 * Callback function called during garbage collection to mark the VM's reachable
 * objects.
 */
typedef void (*RootMarkerCallback)(struct GC *gc, void *ctx);

/**
 * Garbage-collecter state
 */
typedef struct GC {
  size_t bytesAllocated;
  size_t nextGC;

  struct Obj *objects; // Head allocated Obj instances
  Table strings;       // interned strings for ObjString instances

  int grayCount;
  int grayCapacity;
  struct Obj **grayStack;

  RootMarkerCallback
      rootMarkerCallback; // Function that marks runtime roots (set in initVM)
  void *rootMarkerCtx;
} GC;

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

#define ALLOCATE(gc, type, count)                                              \
  (type *)reallocate(gc, NULL, 0, sizeof(type) * (count))

#define FREE(gc, type, pointer) reallocate(gc, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(gc, type, pointer, oldCount, newCount)                      \
  (type *)reallocate(gc, pointer, sizeof(type) * (oldCount),                   \
                     sizeof(type) * (newCount))

#define FREE_ARRAY(gc, type, pointer, oldCount)                                \
  reallocate(gc, pointer, sizeof(type) * (oldCount), 0)

/**
 * Primary function to allocate GC'd memory
 */
void *reallocate(GC *gc, void *pointer, size_t oldSize, size_t newSize);

// ---------------------------------------------------------------------------
// Garbage collector
// ---------------------------------------------------------------------------

/**
 * Mark an object as reachable so the sweep phase does not collect it.
 */
void markObject(GC *gc, struct Obj *object);

/**
 * Mark a value as reachable (no-op for non-object values).
 */
void markValue(GC *gc, Value value);

/**
 * Run a full mark-sweep collection cycle.
 */
void collectGarbage(GC *gc);

/**
 * Free every object on the heap. Called at VM shutdown.
 */
void freeObjects(GC *gc);

#endif
