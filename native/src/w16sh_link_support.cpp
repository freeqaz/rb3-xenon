// w16sh_link_support.cpp -- native bodies the W16-SH link needs that no
// portable TU supplies. rb3-render ONLY. Two parts: the Quazal string surface
// the band3 data-result layer needs (below), and three VMX128 intrinsics
// (end of file).
//
// net_band/DataResults.cpp (DataResultList, the container every BandProfile,
// SongStatusMgr and ProfileMgr carries for RockCentral replies) holds its raw
// reply as a `Quazal::String *`. The real TU is network/quazal/Core/String.cpp,
// a /Od Quazal NetZ file that declares its own ILP32 CRT prototypes and cannot
// compile natively, and Quazal is out of the native port's scope. Linking
// DataResults.cpp for real therefore needs exactly the five members below,
// which --gc-sections leaves live: they are DataResultList's ctor/dtor/Clear
// path, reached at static-init time through `ProfileMgr TheProfileMgr`.
//
// These are PORTS of String.cpp's bodies, not stubs: CopyString's "null source
// gives a null buffer" rule is kept, and so is the wide-to-narrow conversion
// (code page 0 on X360; here every wchar_t is narrowed to its low byte, which is
// the same answer for the ASCII replies RockCentral sends). RootObject's
// allocator is Quazal's MemoryManager, whose default backend is the C heap.
// Native-only; the X360 build never compiles this file.

#include <cstdlib>
#include <cstring>
#include <cwchar>

#include "network/Platform/String.h"

namespace {
    char *CopyNarrow(const char *src) {
        if (!src)
            return nullptr;
        char *dst = new char[strlen(src) + 1];
        strcpy(dst, src);
        return dst;
    }
    char *CopyWide(const wchar_t *src) {
        if (!src)
            return nullptr;
        size_t n = wcslen(src);
        char *dst = new char[n + 1];
        for (size_t i = 0; i < n; i++)
            dst[i] = (char)src[i];
        dst[n] = 0;
        return dst;
    }
}

namespace Quazal {
    void *RootObject::operator new(size_t size) {
        void *p = malloc(size);
        if (!p)
            abort();
        return p;
    }
    void RootObject::operator delete(void *p) { free(p); }

    String::String() : m_szContent(CopyNarrow("")) {}
    String::~String() { delete[] m_szContent; }
    String &String::operator=(const wchar_t *src) {
        delete[] m_szContent;
        m_szContent = CopyWide(src);
        return *this;
    }
}

// ---------------------------------------------------------------------------
// The three VMX128 intrinsics dsp/SndAnalysis.cpp's ShiftedDotProduct fast path
// uses (xdk/LIBCMT/vectorintrinsics.h declares them; on X360 they are
// instructions, natively nothing defined them). Semantics per the PowerPC ISA:
//   vmaddfp  d[i] = a[i]*b[i] + c[i]
//   vspltw   d[i] = a[imm & 3]
//   vperm    d.byte[k] = (a||b).byte[perm.byte[k] & 0x1F], byte order BIG-ENDIAN
// vperm is emulated on the big-endian byte image of each word, so a selector
// like 0x04050607 means "word 1" here exactly as it does on the console.
// ---------------------------------------------------------------------------
#include "xdk/LIBCMT/vectorintrinsics.h"

extern "C" {
XMVECTOR __vmaddfp(XMVECTOR a, XMVECTOR b, XMVECTOR c) {
    XMVECTOR d;
    for (int i = 0; i < 4; i++)
        d.vector4_f32[i] = a.vector4_f32[i] * b.vector4_f32[i] + c.vector4_f32[i];
    return d;
}

XMVECTOR __vspltw(XMVECTOR a, unsigned int imm) {
    XMVECTOR d;
    for (int i = 0; i < 4; i++)
        d.vector4_u32[i] = a.vector4_u32[imm & 3];
    return d;
}

XMVECTOR __vperm(XMVECTOR a, XMVECTOR b, XMVECTOR perm) {
    unsigned char src[32], sel[16], out[16];
    for (int w = 0; w < 4; w++)
        for (int k = 0; k < 4; k++) {
            src[w * 4 + k] = (unsigned char)(a.vector4_u32[w] >> (24 - 8 * k));
            src[16 + w * 4 + k] = (unsigned char)(b.vector4_u32[w] >> (24 - 8 * k));
            sel[w * 4 + k] = (unsigned char)(perm.vector4_u32[w] >> (24 - 8 * k));
        }
    for (int i = 0; i < 16; i++)
        out[i] = src[sel[i] & 0x1F];
    XMVECTOR d;
    for (int w = 0; w < 4; w++)
        d.vector4_u32[w] = ((unsigned int)out[w * 4] << 24) | ((unsigned int)out[w * 4 + 1] << 16)
            | ((unsigned int)out[w * 4 + 2] << 8) | (unsigned int)out[w * 4 + 3];
    return d;
}
}
