#ifndef UTL_VECTORSIZEDEFS_H
#define UTL_VECTORSIZEDEFS_H

#include <vector>

/*
 * These defines exist as future-proofing for using STLs other than STLport.
 * Use them wherever the vector size argument must be used, rather than specifying the
 * size argument directly, as otherwise that argument will attempt to be used as the
 * allocator on standard STL implementations and fail miserably.
 *
 * Ported from rb3-Wii src/system/utl/VectorSizeDefs.h.
 * Required by src/network/Platform/qStd.h.
 */
// clang-format off
#if defined(STLPORT) && defined(_STLP_USE_SIZED_VECTOR)
    #define VECTOR_SIZE_SMALL , unsigned short
    #define VECTOR_SIZE_LARGE , unsigned int
    #define VECTOR_SIZE_PARAM , class VecSize
    #define VECTOR_SIZE_ARG , VecSize
    #define VECTOR_SIZE_DFLT_PARAM , class VecSize = unsigned short
    #define VECTOR_SIZE_DFLT_ARG , unsigned short
#else
    #define VECTOR_SIZE_SMALL
    #define VECTOR_SIZE_LARGE
    #define VECTOR_SIZE_PARAM
    #define VECTOR_SIZE_ARG
    #define VECTOR_SIZE_DFLT_PARAM
    #define VECTOR_SIZE_DFLT_ARG
#endif
// clang-format on

#endif
