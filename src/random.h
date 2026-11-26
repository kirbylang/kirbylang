#ifndef RANDOM_H
#define RANDOM_H

#include <stdint.h>

// The largest number `randomNext` returns.
#define RANDOM_MAX 0x7fffffffL

// Scrambles the inputs into one seed. Changing any input by a little changes
// about half of the seed's bits.
unsigned int randomSeedMix(uint64_t seconds, uint64_t nanos, uint64_t pid);

// Prepares the random number generator. Call once, before `randomNext`.
void randomInit(void);

// A random number from 0 to `RANDOM_MAX`, both included.
long randomNext(void);

// A random number from 0 to `max`, both included, each equally likely.
uint64_t randomUpTo(uint64_t max);

#endif
