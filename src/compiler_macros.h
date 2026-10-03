#ifndef COMPILER_MACROS_H
#define COMPILER_MACROS_H

// Compiler-portability macros for code written against Metrowerks.
//
// The keyword-erasing block below exists only so that a tool which is not a real
// compiler (an IDE indexer, with DECOMP_IDE_FLAG) can parse Metrowerks-specific
// declarations. It must never apply to a compiler that owns these keywords: under
// MSVC, `#define __declspec(x)` silently deleted every later
// `__declspec(noinline)` in the including TU, and the out-of-line deletes and
// calls retail keeps were inlined (W16-OZ, the three track TUs). MSVC, Metrowerks
// and GCC/clang keep their own keywords; only an unknown tool gets the erasure.

#ifndef __MWERKS__
#define __option(x) 0
#endif

#if defined(DECOMP_IDE_FLAG) && !defined(_MSC_VER)
#define __declspec(x)
#define __attribute__(x)
#elif !defined(__MWERKS__) && !defined(_MSC_VER) && !defined(__GNUC__) && !defined(__clang__)
#define __declspec(x)
#define __attribute__(x)
#endif

#if defined(_MSC_VER) && (defined(__declspec) || defined(__attribute__))
#error "__declspec/__attribute__ redefined as a macro under MSVC; it erases noinline and align"
#endif

#if defined(_MSC_VER)
#define ALIGN(x) __declspec(align(x))
#define DONT_INLINE __declspec(noinline)
#define DONT_INLINE_CLASS __declspec(noinline)
#else
#define ALIGN(x) __attribute__((aligned(x)))
// There are two attributes for this for whatever reason
// The __attribute__ didn't work on ec::malloc(), the __declspec errors on class methods
#define DONT_INLINE __declspec(noinline) // use for regular functions
#define DONT_INLINE_CLASS __attribute__((never_inline)) // use for class methods
#endif

// Metrowerks-only declaration syntax. Left undefined elsewhere so that a use
// fails to compile instead of meaning something else.
#ifdef __MWERKS__
#define DECL_SECTION(x) __declspec(section x)
#define DECL_WEAK __declspec(weak)
#endif

#ifdef VERSION_SZBE69_B8
#define RETAIL_DONT_INLINE_FUNC inline
#define RETAIL_DONT_INLINE_CLASS inline
#else
#define RETAIL_DONT_INLINE_FUNC DONT_INLINE
#define RETAIL_DONT_INLINE_CLASS DONT_INLINE_CLASS
#endif

#endif
