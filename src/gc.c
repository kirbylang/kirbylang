#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "gc.h"
#include "object.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(GC *gc, void *pointer, size_t oldSize, size_t newSize) {
  gc->bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage(gc);
#endif

    if (gc->bytesAllocated > gc->nextGC) {
      collectGarbage(gc);
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, newSize);

  if (result == NULL) {
    fprintf(stderr, "Realloc failed");
    exit(EXIT_CODE_OS_ERR);
  }

  return result;
}

void markObject(GC *gc, Obj *object) {
  if (object == NULL)
    return;
  if (object->isMarked)
    return;

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  object->isMarked = true;

  if (gc->grayCapacity < gc->grayCount + 1) {
    gc->grayCapacity = GROW_CAPACITY(gc->grayCapacity);
    gc->grayStack =
        (Obj **)realloc(gc->grayStack, sizeof(Obj *) * gc->grayCapacity);

    if (gc->grayStack == NULL) {
      fprintf(stderr, "gc->grayStack == NULL");
      exit(EXIT_CODE_OS_ERR);
    }
  }

  gc->grayStack[gc->grayCount++] = object;
}

void markValue(GC *gc, Value value) {
  if (IS_OBJ(value))
    markObject(gc, AS_OBJ(value));
}

static void markArray(GC *gc, ValueArray *array) {
  for (int i = 0; i < array->count; i++) {
    markValue(gc, array->values[i]);
  }
}

static void blackenObject(GC *gc, Obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void *)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
  case OBJ_STRUCT: {
    ObjStruct *struct_ = (ObjStruct *)object;
    markObject(gc, (Obj *)struct_->name);
    markTable(gc, &struct_->methods);
    markTable(gc, &struct_->fields);

    break;
  }
  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    markObject(gc, (Obj *)instance->struct_);

    if (instance->fields != NULL) {
      for (int i = 0; i < instance->struct_->fieldCount; i++) {
        markValue(gc, instance->fields[i]);
      }
    }

    break;
  }
  case OBJ_BOUND_METHOD: {
    ObjBoundMethod *bound = (ObjBoundMethod *)object;
    markValue(gc, bound->receiver);
    markObject(gc, (Obj *)bound->method);
    break;
  }
  case OBJ_UPVALUE:
    markValue(gc, ((ObjUpvalue *)object)->closed);
    break;
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    markObject(gc, (Obj *)closure->function);
    markObject(gc, (Obj *)closure->owner);

    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject(gc, (Obj *)closure->upvalues[i]);
    }

    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    markObject(gc, (Obj *)function->name);
    markArray(gc, &function->chunk.constants);
    break;
  }
  case OBJ_ARRAY: {
    ObjArray *array = (ObjArray *)object;

    for (int i = 0; i < array->count; i++) {
      markValue(gc, array->values[i]);
    }

    break;
  }
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  }
}

static void freeObject(GC *gc, Obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void *)object, object->type);
#endif

  switch (object->type) {
  case OBJ_STRUCT: {
    ObjStruct *struct_ = (ObjStruct *)object;
    freeTable(gc, &struct_->methods);
    freeTable(gc, &struct_->fields);
    FREE(gc, ObjStruct, object);
    break;
  }
  case OBJ_INSTANCE: {
    ObjInstance *instance = (ObjInstance *)object;
    FREE_ARRAY(gc, Value, instance->fields, instance->struct_->fieldCount);
    FREE(gc, ObjInstance, object);
    break;
  }
  case OBJ_BOUND_METHOD:
    FREE(gc, ObjBoundMethod, object);
    break;
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    FREE_ARRAY(gc, ObjUpvalue *, closure->upvalues, closure->upvalueCount);

    FREE(gc, ObjClosure, object);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(gc, &function->chunk);
    FREE(gc, ObjFunction, object);
    break;
  }
  case OBJ_NATIVE: {
    FREE(gc, ObjNative, object);
    break;
  }
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(gc, char, string->chars, string->length + 1);
    FREE(gc, ObjString, object);
    break;
  }
  case OBJ_UPVALUE:
    FREE(gc, ObjUpvalue, object);
    break;
  case OBJ_ARRAY: {
    ObjArray *array = (ObjArray *)object;
    FREE_ARRAY(gc, Value, array->values, array->capacity);
    FREE(gc, ObjArray, object);
    break;
  }
  }
}

static void markRoots(GC *gc) {
  if (gc->rootMarkerCallback != NULL) {
    gc->rootMarkerCallback(gc, gc->rootMarkerCtx);
  }
}

static void traceReferences(GC *gc) {
  while (gc->grayCount > 0) {
    Obj *object = gc->grayStack[--gc->grayCount];
    blackenObject(gc, object);
  }
}

static void sweep(GC *gc) {
  Obj *previous = NULL;
  Obj *object = gc->objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Obj *unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        gc->objects = object;
      }

      freeObject(gc, unreached);
    }
  }
}

void collectGarbage(GC *gc) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = gc->bytesAllocated;
#endif

  markRoots(gc);
  traceReferences(gc);
  tableRemoveWhite(&gc->strings);
  sweep(gc);

  gc->nextGC = gc->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - gc->bytesAllocated, before, gc->bytesAllocated, gc->nextGC);
#endif
}

void freeObjects(GC *gc) {
  Obj *object = gc->objects;
  while (object != NULL) {
    Obj *next = object->next;
    freeObject(gc, object);
    object = next;
  }

  free(gc->grayStack);
}
