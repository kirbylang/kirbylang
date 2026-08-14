#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/stringset.h"

static void test_empty_set_contains_nothing(void) {
  StringSet set;
  stringSetInit(&set);

  assert(stringSetContains(&set, "x", 1) == false);
  assert(set.count == 0);

  stringSetFree(&set);
}

static void test_add_then_contains(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "hello", 5);

  assert(stringSetContains(&set, "hello", 5) == true);
  assert(stringSetContains(&set, "world", 5) == false);
  assert(set.count == 1);

  stringSetFree(&set);
}

static void test_duplicate_add_is_a_no_op(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "x", 1);
  stringSetAdd(&set, "x", 1);
  stringSetAdd(&set, "x", 1);

  assert(set.count == 1);
  assert(stringSetContains(&set, "x", 1) == true);

  stringSetFree(&set);
}

/**
 * Same bytes at different lengths, and different bytes at the same length,
 * must not be confused with each other.
 */
static void test_distinguishes_prefixes_and_same_length_strings(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "a", 1);
  stringSetAdd(&set, "ab", 2);
  stringSetAdd(&set, "abc", 3);
  stringSetAdd(&set, "xy", 2);

  assert(set.count == 4);
  assert(stringSetContains(&set, "a", 1) == true);
  assert(stringSetContains(&set, "ab", 2) == true);
  assert(stringSetContains(&set, "abc", 3) == true);
  assert(stringSetContains(&set, "xy", 2) == true);
  assert(stringSetContains(&set, "ac", 2) == false);
  assert(stringSetContains(&set, "b", 1) == false);

  stringSetFree(&set);
}

/**
 * Comparison is byte-exact: differing case must be treated as distinct
 * entries, never folded together.
 */
static void test_case_sensitive(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "Foo", 3);

  assert(stringSetContains(&set, "Foo", 3) == true);
  assert(stringSetContains(&set, "foo", 3) == false);
  assert(stringSetContains(&set, "FOO", 3) == false);
  assert(set.count == 1);

  stringSetAdd(&set, "foo", 3);
  assert(set.count == 2);

  stringSetFree(&set);
}

/**
 * A zero-length string is a legal (if unusual) entry: hashing, arena
 * append, and comparison must all handle length 0 without special-casing
 * anywhere that could crash or divide by zero.
 */
static void test_empty_string_entry(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "", 0);

  assert(set.count == 1);
  assert(stringSetContains(&set, "", 0) == true);
  assert(stringSetContains(&set, "x", 1) == false);

  // Re-adding it is still a no-op.
  stringSetAdd(&set, "", 0);
  assert(set.count == 1);

  stringSetFree(&set);
}

/**
 * A single very long entry exercises the arena's own growth path in
 * isolation, separate from the entries-table growth path exercised by
 * test_many_entries_survive_growth.
 */
static void test_single_long_string(void) {
  StringSet set;
  stringSetInit(&set);

  char big[5000];
  for (int i = 0; i < 5000; i++) {
    big[i] = (char)('a' + (i % 26));
  }

  stringSetAdd(&set, big, 5000);

  assert(set.count == 1);
  assert(stringSetContains(&set, big, 5000) == true);

  // A one-byte difference at the very end must not be treated as a match.
  big[4999] = big[4999] == 'z' ? 'a' : 'z';
  assert(stringSetContains(&set, big, 5000) == false);

  stringSetFree(&set);
}

/**
 * Insert enough entries to force several capacity growths (starts at 8,
 * doubling), and confirm every entry survives every rehash.
 */
static void test_many_entries_survive_growth(void) {
  StringSet set;
  stringSetInit(&set);

  char names[200][8];
  int lengths[200];

  for (int i = 0; i < 200; i++) {
    lengths[i] = snprintf(names[i], sizeof(names[i]), "g%d", i);
    stringSetAdd(&set, names[i], lengths[i]);
  }

  assert(set.count == 200);
  assert(set.capacity > 200); // load factor keeps capacity above count

  for (int i = 0; i < 200; i++) {
    assert(stringSetContains(&set, names[i], lengths[i]) == true);
  }

  assert(stringSetContains(&set, "not-there", 9) == false);

  stringSetFree(&set);
}

/**
 * Re-adding an entry that was inserted before one or more rehashes must
 * still be recognized as a duplicate afterward -- i.e. growth must correctly
 * carry every entry's (offset, length, hash) into the new table, not just
 * enough of them to pass a plain membership check.
 */
static void test_duplicate_add_after_growth_is_still_a_no_op(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "first", 5);

  char names[300][8];
  int lengths[300];
  for (int i = 0; i < 300; i++) {
    lengths[i] = snprintf(names[i], sizeof(names[i]), "n%d", i);
    stringSetAdd(&set, names[i], lengths[i]);
  }

  int countBefore = set.count;
  stringSetAdd(&set, "first", 5); // re-add the very first entry, post-growth
  assert(set.count == countBefore);
  assert(stringSetContains(&set, "first", 5) == true);

  stringSetFree(&set);
}

/**
 * After stringSetFree(), the set must be safe to reuse directly (no explicit
 * re-init call needed) and must not remember anything from before the free.
 */
static void test_free_resets_and_is_reusable(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "old", 3);
  stringSetFree(&set);

  assert(set.count == 0);
  assert(set.capacity == 0);
  assert(stringSetContains(&set, "old", 3) == false);

  stringSetAdd(&set, "new", 3);
  assert(stringSetContains(&set, "new", 3) == true);
  assert(stringSetContains(&set, "old", 3) == false);

  stringSetFree(&set);
}

/**
 * Freeing a set that never had anything added to it must not crash.
 */
static void test_free_on_empty_set_is_safe(void) {
  StringSet set;
  stringSetInit(&set);
  stringSetFree(&set);
}

/**
 * Freeing an already-freed set must be safe and idempotent -- matching the
 * same guarantee the old NameSet implementation relied on (compilerSessionEnd
 * being callable without a preceding "session started" check).
 */
static void test_double_free_is_safe(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "x", 1);
  stringSetFree(&set);
  stringSetFree(&set); // must not double-free or crash

  assert(set.count == 0);
  assert(stringSetContains(&set, "x", 1) == false);
}

/**
 * count must reflect only unique entries through a realistic interleaving
 * of unique and duplicate adds, not just in the all-unique or all-duplicate
 * cases tested above individually.
 */
static void test_count_accounting_with_interleaved_duplicates(void) {
  StringSet set;
  stringSetInit(&set);

  stringSetAdd(&set, "a", 1);
  stringSetAdd(&set, "b", 1);
  stringSetAdd(&set, "a", 1); // dup
  stringSetAdd(&set, "c", 1);
  stringSetAdd(&set, "b", 1); // dup
  stringSetAdd(&set, "b", 1); // dup

  assert(set.count == 3);
  assert(stringSetContains(&set, "a", 1));
  assert(stringSetContains(&set, "b", 1));
  assert(stringSetContains(&set, "c", 1));
  assert(!stringSetContains(&set, "d", 1));

  stringSetFree(&set);
}

int main(void) {
  test_empty_set_contains_nothing();
  test_add_then_contains();
  test_duplicate_add_is_a_no_op();
  test_distinguishes_prefixes_and_same_length_strings();
  test_case_sensitive();
  test_empty_string_entry();
  test_single_long_string();
  test_many_entries_survive_growth();
  test_duplicate_add_after_growth_is_still_a_no_op();
  test_free_resets_and_is_reusable();
  test_free_on_empty_set_is_safe();
  test_double_free_is_safe();
  test_count_accounting_with_interleaved_duplicates();

  printf("stringset: ok\n");
  return 0;
}
