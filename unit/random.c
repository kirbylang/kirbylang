#include <assert.h>
#include <stdint.h>

#include "../src/random.h"

static int bitsDifferent(unsigned int a, unsigned int b) {
  unsigned int x = a ^ b;
  int count = 0;

  while (x != 0) {
    count += (int)(x & 1u);
    x >>= 1;
  }

  return count;
}

static void testSeedMix(void) {
  // Same inputs, same seed.
  assert(randomSeedMix(1000, 5, 7) == randomSeedMix(1000, 5, 7));

  // Each input changes the seed.
  assert(randomSeedMix(1000, 5, 7) != randomSeedMix(1001, 5, 7));
  assert(randomSeedMix(1000, 5, 7) != randomSeedMix(1000, 6, 7));
  assert(randomSeedMix(1000, 5, 7) != randomSeedMix(1000, 5, 8));

  // Starting one second, or one nanosecond, later must give a seed that looks
  // unrelated. Some `rand()` versions return nearly the same first number for
  // seeds that are close together.
  for (uint64_t t = 1000; t < 2000; t++) {
    assert(bitsDifferent(randomSeedMix(t, 0, 1), randomSeedMix(t + 1, 0, 1)) >=
           4);
    assert(bitsDifferent(randomSeedMix(t, 0, 1), randomSeedMix(t, 1, 1)) >= 4);
    assert(bitsDifferent(randomSeedMix(t, 0, 1), randomSeedMix(t, 0, 2)) >= 4);
  }
}

static void testNext(void) {
  randomInit();

  long first = randomNext();
  int differs = 0;
  long highest = first;

  for (int i = 0; i < 1000; i++) {
    long n = randomNext();

    assert(n >= 0);
    assert(n <= RANDOM_MAX);

    if (n != first)
      differs = 1;
    if (n > highest)
      highest = n;
  }

  // Not stuck on one value, and the values reach the top half of the range.
  assert(differs);
  assert(highest > RANDOM_MAX / 2);
}

static void testUpTo(void) {
  randomInit();

  assert(randomUpTo(0) == 0);

  int seen[3] = {0};

  for (int i = 0; i < 1000; i++) {
    uint64_t n = randomUpTo(2);

    assert(n <= 2);
    seen[n] = 1;
  }

  assert(seen[0] && seen[1] && seen[2]);

  // The widest ranges use all 64 bits.
  uint64_t anyBit = 0;

  for (int i = 0; i < 1000; i++)
    anyBit |= randomUpTo(UINT64_MAX);

  assert(anyBit == UINT64_MAX);
}

int main(void) {
  testSeedMix();
  testNext();
  testUpTo();

  return 0;
}
