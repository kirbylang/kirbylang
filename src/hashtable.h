#ifndef kirby_table_h
#define kirby_table_h

#include "common.h"
#include "value.h"

/**
 * Forward declaration of the GC struct.
 *
 * gc.h uses this file. This avoid circular dependencies.
 */
struct GC;

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(struct GC *gc, Table *table);
void markTable(struct GC *gc, Table *table);
void tableRemoveWhite(Table *table);
bool tableGet(Table *table, ObjString *key, Value *value);
bool tableSet(struct GC *gc, Table *table, ObjString *key, Value value);
bool tableDelete(Table *table, ObjString *key);
void tableAddAll(struct GC *gc, Table *from, Table *to);
ObjString *tableFindString(Table *table, const char *chars, int length,
                           uint32_t hash);

#endif
