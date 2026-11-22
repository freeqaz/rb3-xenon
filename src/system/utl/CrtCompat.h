#pragma once

// Native stand-ins for retail CRT functions whose glibc namesakes behave
// differently. On X360 each macro expands to the original call, token for
// token, so the match build is unchanged.
// docs/decomp/NATIVE_LP64_CRT_TRIAGE_2026-11-22.md has the per-site triage.

#ifdef HX_NATIVE
#include <climits>
#include <cstdlib>

// Retail rand (0x8282E898): `bl _getptd`, then the per-thread seed at
// ptd+0x14 (seed 1 for every thread, no srand call in this tree) stepped as
// seed * 214013 + 2531011, returning bits 16..30 (RAND_MAX 0x7FFF). glibc's
// rand() is 31-bit, a different generator, and one stream for the whole
// process. This returns retail's per-thread sequence.
inline int HxCrtRand() {
    static thread_local unsigned int sHoldRand = 1;
    sHoldRand = sHoldRand * 214013u + 2531011u;
    return (int)((sHoldRand >> 16) & 0x7FFF);
}

// Retail atoi (0x8282D9B8) is `li r5,10; li r4,0; b strtol` with a 32-bit
// long, so an out-of-range literal clamps to INT_MAX / INT_MIN. glibc's atoi
// truncates a 64-bit strtol result instead (3000000000 reads as -1294967296).
inline int HxCrtAtoi(const char *s) {
    long v = strtol(s, nullptr, 10);
    if (v > INT_MAX)
        return INT_MAX;
    if (v < INT_MIN)
        return INT_MIN;
    return (int)v;
}

#define HX_CRT_RAND() HxCrtRand()
#define HX_CRT_ATOI(s) HxCrtAtoi(s)
#else
#define HX_CRT_RAND() rand()
#define HX_CRT_ATOI(s) atoi(s)
#endif
