// w16sh_link_support.cpp -- the Quazal string surface the band3 data-result
// layer needs natively (W16-SH). rb3-render ONLY.
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
