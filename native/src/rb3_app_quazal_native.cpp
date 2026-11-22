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
//
// Census tags (tools/native_stub_census.py --rings rb3-app): the String and
// RootObject members are written inside `namespace Quazal {`, so a per-line
// `// ring:` tag names `String::String`, not the demangled
// `Quazal::String::String`, and the operators carry no plain identifier at
// all. The class-wide tags below are what the census matches (lane A3).
// ring-class: Quazal::String native-real
// ring-class: Quazal::RootObject native-real
// ring-class: Quazal::qResult native-real
// ring-class: Quazal::StreamSettings native-real
// ring-class: Quazal::Option native-real
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include "Platform/String.h"
#include "Platform/Result.h"
#include "Core/PseudoSingleton.h"
#include "Plugins/Stream.h"
#include "Plugins/StreamSettings.h"

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

// ---- the Quazal transport state the idle session's ctors write (lane A3) ----
//
// App::App -> Net::Init -> NetSession::New() = new XboxSession (rb3-app runs only
// that part of Net::Init, rb3_app_gates.cpp). The two ctors (NetSession.cpp:101,
// NetSession_Xbox.cpp:404) write Quazal transport configuration and send
// nothing. Retail bodies were read from the disc binary (Ghidra default.xex,
// the band.exe build); each native body below is either that body or, for
// storage, the same storage left unconstructed with the reason it is
// equivalent on a machine with no Quazal stack.

namespace Quazal {

// Retail 0x82A8A090 (the out-of-line qResult(const int&)): copies the code and
// stamps the source position of Result.cpp, line 0x1B.
qResult::qResult(const int &code) { // ring: native-real
    m_iReturnCode = (uint)code;
    m_cszFilename = ".\\Core\\Result.cpp";
    m_iLineNumber = 0x1B;
}

// Retail 0x82A6FC60 is `li r3, 0; blr`: this build of Quazal never selects an
// extra context, so the current context is always the default one. Exact.
unsigned int PseudoSingleton::GetCurrentContext() { return 0; } // ring: native-real

// Retail 0x82A6E3E0: one store to +0x30 (mMaxSilenceTime), returns
// qResult(0x10001), Quazal's success code. Exact.
qResult StreamSettings::SetMaxSilenceTime(unsigned int t) { // ring: native-real
    mMaxSilenceTime = t;
    return qResult(0x10001);
}

// Retail: Stream::s_oStreamSettings is 16 PseudoGlobalVariable<StreamSettings>
// (0xB0 bytes each, 0x82E0F7F4), constructed at static init with Quazal's
// stream defaults, read by the PRUDP/routing streams when a connection opens.
// Natively there is no stream: the only native access is NetSession's ctor
// storing max_connection_silence into slot 1's default-context value (the
// retail GetStreamSettingsForContext path, context 0 always). So the storage is
// reserved but NOT constructed (no vptr, no Quazal defaults). Any native reader
// of the defaults would be a transport TU joining rb3-app, which would also
// bring the real definition and fail the link as a duplicate.
alignas(PseudoGlobalVariable<StreamSettings>) unsigned char
    gRb3AppStreamSettings[16 * sizeof(PseudoGlobalVariable<StreamSettings>)] __asm__(
        "_ZN6Quazal6Stream17s_oStreamSettingsE"); // ring: online-quazal
}

// Declared exactly as NetSession.cpp:45 declares it (no header names the class).
namespace Quazal {
class RootTransport {
public:
    static unsigned int s_uiDefaultConnectionTimeout;
};
// Retail 0x82C991D4, a plain word NetSession's ctor overwrites with
// net/session/connection_timeout before anything reads it; the readers are the
// transport's connection setup, absent natively. Zero-initialised like retail .bss.
unsigned int RootTransport::s_uiDefaultConnectionTimeout = 0; // ring: native-real
}

// Declared exactly as NetSession_Xbox.cpp:25-42 declares them (retail callees
// are anonymous, so that TU names them from their call sites).
namespace Quazal {
void SetXboxNetFlagA(bool);
void SetXboxNetFlagB(bool);
void ClearXNetKeyFlag();
class Option {
public:
    bool Set(bool);
};
Option *FindOption(const String &);
}

namespace {
// Retail flag bytes 0x82E1041B (A), 0x82E1041A (B), 0x82C99240 (the XNet key
// flag RegisterXNetKey tests) and the byte counter 0x82E10419 SetXboxNetFlagA
// bumps. Their readers are Quazal's Xbox socket layer, absent natively.
unsigned char gXboxNetFlagA, gXboxNetFlagB, gXNetKeyFlag, gXboxNetFlagACount; // ring: native-real

// Retail Option node (FindOption 0x82A8F7C0 walks a list; 0x82A8F788 Set):
// next at +0, name (Quazal::String) at +8, value at +0xC, locked byte at +0x10.
// NetSession_Xbox.cpp declares Option with no data members, so the node lives
// here and Option* is its address (no second layout of Quazal::Option).
struct NativeOption {
    const char *name;
    int value;
    bool locked;
};
// The one option rb3-app's code names. Retail registers it in Quazal's option
// list at static init (FindOption returning null would crash the retail ctor,
// which dereferences the result); its default value is never read natively.
NativeOption gNativeOptions[] = { { "UseDistinctPortForStreamManager", 0, false } }; // ring: native-real
}

// Retail 0x82A8EC08: store the flag byte, then increment the counter byte.
void Quazal::SetXboxNetFlagA(bool b) { // ring: native-real
    gXboxNetFlagA = b;
    gXboxNetFlagACount++;
}
// Retail 0x82A8ECB8: store the flag byte.
void Quazal::SetXboxNetFlagB(bool b) { gXboxNetFlagB = b; } // ring: native-real
// Retail 0x82A8EC88: `stb 0` to the key flag.
void Quazal::ClearXNetKeyFlag() { gXNetKeyFlag = 0; } // ring: native-real

// Retail 0x82A8F7C0: walk the option list, return the node whose name compares
// equal, or null. Same contract over the native table.
Quazal::Option *Quazal::FindOption(const Quazal::String &name) { // ring: native-real
    const char *want = name.m_szContent;
    for (NativeOption &o : gNativeOptions) {
        if (want && strcmp(o.name, want) == 0)
            return reinterpret_cast<Quazal::Option *>(&o);
    }
    return nullptr;
}
// Retail 0x82A8F788: a locked option refuses (returns false); otherwise the
// value word is stored and it returns true.
bool Quazal::Option::Set(bool v) { // ring: native-real
    NativeOption *o = reinterpret_cast<NativeOption *>(this);
    if (o->locked)
        return false;
    o->value = v;
    return true;
}
