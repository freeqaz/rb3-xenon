#pragma once
// #include <stddef.h> // we're now pulling in MSVC's built-in stddef this way
#include "types_compat.h"

#define NULL 0

#ifdef __cplusplus
extern "C" {
#endif

// #if !defined(__cplusplus)
// #include <wchar.h>
// #endif

#ifndef _PTRDIFF_T_DEFINED
typedef int ptrdiff_t;
#define _PTRDIFF_T_DEFINED
#endif

// NOTE (lane W16-P, 2026-09-14): this was `((int)&((T *)0->mem))`, which parses as
// `(T *)(0->mem)` -- `->` binds tighter than the cast -- so ANY expansion is a hard
// compile error, not merely a wrong value. Measured: no compiled TU expands it today
// (the only two call sites, curl memdebug.c:270/301, are under `#ifdef CURLDEBUG` and
// that TU is in neither objects.json nor the ninja graph), which is why the tree builds.
#define offsetof(T, mem) ((int)&(((T *)0)->mem))

#ifdef __cplusplus
}
#endif
