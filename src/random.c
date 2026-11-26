// For `random`, `srandom` and `clock_gettime`.
#ifndef __APPLE__
#define _XOPEN_SOURCE 700
#endif

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "random.h"

// A 64 bit mixing step (the finalizer from SplitMix64).
static uint64_t mix64(uint64_t x) {
  x += 0x9e3779b97f4a7c15ULL;
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;

  return x ^ (x >> 31);
}

unsigned int randomSeedMix(uint64_t seconds, uint64_t nanos, uint64_t pid) {
  uint64_t h = mix64(seconds);

  h = mix64(h ^ nanos);
  h = mix64(h ^ pid);

  return (unsigned int)(h ^ (h >> 32));
}

#ifdef __APPLE__

// `arc4random` seeds itself from the system.
void randomInit(void) {}

long randomNext(void) { return (long)(arc4random() >> 1); }

#else

// Seeded from the clock and the process ID.
void randomInit(void) {
  struct timespec now;

  clock_gettime(CLOCK_REALTIME, &now);

  srandom(randomSeedMix((uint64_t)now.tv_sec, (uint64_t)now.tv_nsec,
                        (uint64_t)getpid()));
}

long randomNext(void) { return random(); }

#endif

// 64 random bits. One `randomNext` gives 31, so it takes three.
static uint64_t randomBits64(void) {
  uint64_t high = (uint64_t)randomNext();
  uint64_t middle = (uint64_t)randomNext();
  uint64_t low = (uint64_t)randomNext();

  return (high << 33) ^ (middle << 2) ^ (low & 3);
}

uint64_t randomUpTo(uint64_t max) {
  if (max == UINT64_MAX)
    return randomBits64();

  uint64_t count = max + 1;
  // Values at or above `limit` would make the low results more likely.
  uint64_t limit = UINT64_MAX - UINT64_MAX % count;
  uint64_t bits = randomBits64();

  while (bits >= limit)
    bits = randomBits64();

  return bits % count;
}
