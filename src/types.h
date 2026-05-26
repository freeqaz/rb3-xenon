#pragma once

#include "macros.h" // IWYU pragma: export

#ifdef HX_NATIVE
// POSIX equivalents of MSVC functions
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
// MSVC-specific CRT functions
#include <cstdio>
#define _snprintf snprintf
#define _vsnprintf vsnprintf
// sprintf_s: MSVC provides both sprintf_s(buf, size, fmt, ...) and
// sprintf_s<N>(buf, fmt, ...) template form. Provide both for native.
#include <cstdarg>
template <size_t N>
inline int sprintf_s(char (&buf)[N], const char *fmt, ...) __attribute__((format(printf, 2, 3)));
template <size_t N>
inline int sprintf_s(char (&buf)[N], const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, N, fmt, ap);
    va_end(ap);
    return r;
}
inline int sprintf_s(char *buf, size_t size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
inline int sprintf_s(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}
// MSVC CRT functions
#define _open open
#define _close close
#define _read read
#define _write write
#define _lseek lseek
#define _lseeki64 lseek
#endif

#ifdef HX_NATIVE
// On x86_64 Linux (LP64), long is 8 bytes. Use int for 32-bit types.
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#else
// On Xbox 360 (ILP32), long is 4 bytes.
typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;
#endif

typedef unsigned int uint;

#ifdef HX_NATIVE
#include <cstdint> // intptr_t, uintptr_t
#else
// MSVC PPC doesn't have <cstdint>; on ILP32, intptr_t == int
typedef int intptr_t;
typedef unsigned int uintptr_t;
#endif

typedef float f32;
typedef double f64;
