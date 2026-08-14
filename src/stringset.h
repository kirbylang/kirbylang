#ifndef kirby_stringset_h
#define kirby_stringset_h

#include "common.h"

typedef struct {
  int offset; // offset into the owning StringSet's arena
  int length; // -1 means this slot is empty
  uint32_t hash;
} StringSetEntry;

typedef struct {
  char *arena;
  int arenaLen;
  int arenaCapacity;

  StringSetEntry *entries;
  int count;    // live entries
  int capacity; // slot count in `entries`
} StringSet;

/**
 * Initialize an empty set.
 */
void stringSetInit(StringSet *set);

/**
 * Add `chars`/`length` to the set. No-op if already present.
 */
void stringSetAdd(StringSet *set, const char *chars, int length);

/**
 * True if `chars`/`length` is in the set.
 */
bool stringSetContains(StringSet *set, const char *chars, int length);

/**
 * Free everything owned by the set and reset it to empty. Safe to reuse
 * (stringSetAdd) afterward without calling stringSetInit() again.
 */
void stringSetFree(StringSet *set);

#endif
