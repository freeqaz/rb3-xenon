// rb3-app: the Quazal data slice the OFFLINE boot uses (lane A1, 2026-11-22).
// Native only, rb3-app only.
//
// DataResults (meta_band/DataResults.cpp) stores its rows in Quazal::String
// and allocates through Quazal::RootObject, as plain data structures. It is
// built by ProfileMgr's constructor, i.e. at static init, so this slice is on
// the boot path even though nothing here talks to a network. The retail
// bodies (src/network/quazal/Core/String.cpp, Platform/MemoryManager.cpp) are
// X360-shaped (32-bit size_t prototypes for the CRT, the variadic __va_start
// intrinsic, MemoryManager's instrumented allocator) and do not compile on
// LP64, so this file gives the same semantics with the host allocator:
//
//   * RootObject::operator new/delete: retail routes to
//     MemoryManager::Allocate/Free on the default memory manager. Here:
//     malloc/free. Same contract (never null on success; delete of null ok).
//   * String: one owned NUL-terminated char buffer, m_szContent. Default
//     constructs to "" (not null), exactly as String.cpp:83. Assignment frees
//     the old buffer and copies; self-assignment from its own pointer is a
//     no-op (String.cpp:97). The wide overloads narrow through
//     WideCharToMultiByte(CP_ACP) on retail; here each code unit < 0x100 maps to
//     itself and anything else becomes '?', which is CP_ACP's own default
//     character and is identical for the ASCII the game feeds it.
//
// Everything else in Quazal (sessions, RendezVous, DDL) stays out of rb3-app
// and aborts through rb3_app_link_stubs.s with ring online-quazal.
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "Platform/String.h"

namespace {
char *QuazalCopy(const char *s) { // ring: native-real
    if (!s)
        return nullptr;
    size_t n = strlen(s) + 1;
    char *d = static_cast<char *>(malloc(n));
    memcpy(d, s, n);
    return d;
}
char *QuazalCopy(const wchar_t *s) { // ring: native-real
    if (!s)
        return nullptr;
    size_t n = wcslen(s);
    char *d = static_cast<char *>(malloc(n + 1));
    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned)s[i] < 0x100u ? (char)s[i] : '?';
    d[n] = 0;
    return d;
}
}

namespace Quazal {
void *RootObject::operator new(size_t size) { // ring: native-real
    void *p = malloc(size ? size : 1);
    if (!p)
        abort();
    return p;
}
void RootObject::operator delete(void *p) { free(p); } // ring: native-real

String::String() { m_szContent = QuazalCopy(""); } // ring: native-real
String::String(const char *s) { m_szContent = QuazalCopy(s); } // ring: native-real
String::String(const wchar_t *s) { m_szContent = QuazalCopy(s); } // ring: native-real
String::String(const String &o) { m_szContent = QuazalCopy(o.m_szContent); } // ring: native-real
String::~String() { free(m_szContent); } // ring: native-real

String &String::operator=(const char *s) { // ring: native-real
    if (m_szContent == s)
        return *this;
    char *old = m_szContent;
    m_szContent = QuazalCopy(s);
    free(old);
    return *this;
}
String &String::operator=(const wchar_t *s) { // ring: native-real
    free(m_szContent);
    m_szContent = QuazalCopy(s);
    return *this;
}
String &String::operator=(const String &o) { // ring: native-real
    if (m_szContent != o.m_szContent)
        *this = o.m_szContent;
    return *this;
}
uint String::GetLength() const { // ring: native-real
    return m_szContent ? (uint)strlen(m_szContent) : 0;
}
}
