#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "stringset.h"

#define STRINGSET_INITIAL_CAPACITY 8
#define STRINGSET_MAX_LOAD 0.75

static void *xrealloc(void *ptr, size_t size, const char *what) {
  void *result = realloc(ptr, size);
  if (result == NULL && size != 0) {
    fprintf(stderr, "realloc failed in %s\n", what);
    exit(EXIT_CODE_OS_ERR);
  }
  return result;
}

/**
 * Append `length` bytes to the arena and return the offset they start at.
 */
static int arenaAppend(StringSet *set, const char *chars, int length) {
  if (set->arenaLen + length > set->arenaCapacity) {
    int needed = set->arenaLen + length;
    int cap = set->arenaCapacity < 64 ? 64 : set->arenaCapacity;
    while (cap < needed)
      cap *= 2;
    set->arena = (char *)xrealloc(set->arena, (size_t)cap, "stringset arena");
    set->arenaCapacity = cap;
  }

  int offset = set->arenaLen;
  memcpy(set->arena + offset, chars, (size_t)length);
  set->arenaLen += length;
  return offset;
}

/**
 * Find the slot for `chars`/`length`/`hash`: either the existing matching
 * entry, or the first empty slot on its probe sequence (where it should be
 * inserted). Mirrors hashtable.c's findEntry(), minus tombstone handling.
 */
static StringSetEntry *findSlot(StringSetEntry *entries, int capacity,
                                const char *arena, const char *chars,
                                int length, uint32_t hash) {
  uint32_t index = hash % (uint32_t)capacity;

  for (;;) {
    StringSetEntry *entry = &entries[index];

    if (entry->length == -1) {
      return entry;
    }

    if (entry->length == length && entry->hash == hash &&
        memcmp(arena + entry->offset, chars, (size_t)length) == 0) {
      return entry;
    }

    index = (index + 1) % (uint32_t)capacity;
  }
}

static void adjustCapacity(StringSet *set, int newCapacity) {
  StringSetEntry *entries = (StringSetEntry *)xrealloc(
      NULL, sizeof(StringSetEntry) * (size_t)newCapacity, "stringset entries");

  for (int i = 0; i < newCapacity; i++) {
    entries[i].offset = 0;
    entries[i].length = -1;
    entries[i].hash = 0;
  }

  for (int i = 0; i < set->capacity; i++) {
    StringSetEntry *old = &set->entries[i];
    if (old->length == -1)
      continue;

    StringSetEntry *dest =
        findSlot(entries, newCapacity, set->arena, set->arena + old->offset,
                 old->length, old->hash);
    *dest = *old;
  }

  free(set->entries);
  set->entries = entries;
  set->capacity = newCapacity;
}

void stringSetInit(StringSet *set) {
  set->arena = NULL;
  set->arenaLen = 0;
  set->arenaCapacity = 0;
  set->entries = NULL;
  set->count = 0;
  set->capacity = 0;
}

bool stringSetContains(StringSet *set, const char *chars, int length) {
  if (set->count == 0) {
    return false;
  }

  uint32_t hash = hashBytes(chars, length);
  StringSetEntry *entry =
      findSlot(set->entries, set->capacity, set->arena, chars, length, hash);
  return entry->length != -1;
}

void stringSetAdd(StringSet *set, const char *chars, int length) {
  if (set->count + 1 > (int)(set->capacity * STRINGSET_MAX_LOAD)) {
    int newCapacity = set->capacity < STRINGSET_INITIAL_CAPACITY
                          ? STRINGSET_INITIAL_CAPACITY
                          : set->capacity * 2;
    adjustCapacity(set, newCapacity);
  }

  uint32_t hash = hashBytes(chars, length);
  StringSetEntry *entry =
      findSlot(set->entries, set->capacity, set->arena, chars, length, hash);

  if (entry->length != -1) {
    return; // already present
  }

  int offset = arenaAppend(set, chars, length);
  entry->offset = offset;
  entry->length = length;
  entry->hash = hash;
  set->count++;
}

void stringSetFree(StringSet *set) {
  free(set->arena);
  free(set->entries);
  stringSetInit(set);
}
