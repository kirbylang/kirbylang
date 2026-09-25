// See ./build/generated/stdlib.c
#ifndef STDLIB_SOURCE_H
#define STDLIB_SOURCE_H

// The source of stdlib/stdlib.krb, built into the binary so krb doesn't need
// to find the file at runtime. Ends with '\0'.
extern const char KIRBY_STDLIB[];
// Length of KIRBY_STDLIB, not counting the '\0'.
extern const unsigned int KIRBY_STDLIB_len;

#endif
