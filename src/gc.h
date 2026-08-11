#ifndef kirby_gc_h
#define kirby_gc_h

#include "common.h"
#include "hashtable.h"

// Forward declaration only needed for signatures; full Obj definition in object.h
struct Obj;
struct ObjString;

/**
 * A root-marker callback registered with the GC by a client (typically the
 * VM). Called during collection to mark a client's reachable objects. Keeping
 * this as a callback means the GC never has to know about VM or compiler
 * internals directly.
 */
typedef void (*RootMarker)(struct GC *gc, void *ctx);

/**
 * Garbage-collected heap context.
 *
 * Owns everything the collector touches: the object allocation list, the
 * interned-string table, the gray stack, and the collection budget. It is
 * independent of runtime execution state (which lives in VM). Both the loader
 * and the VM allocate through a GC; the compiler does not touch one at all.
 */
typedef struct GC {
  size_t bytesAllocated;
  size_t nextGC;

  struct Obj *objects; // intrusive allocation list

  int grayCount;
  int grayCapacity;
  struct Obj **grayStack;
  bool gcEnabled;

  Table strings; // interned strings live with the heap, not the runtime

  RootMarker rootMarker; // marks runtime roots (set by the VM)
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
 * Core allocator. All GC-managed memory goes through here so the collector
 * can track live bytes and trigger collection when the budget is exceeded.
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
