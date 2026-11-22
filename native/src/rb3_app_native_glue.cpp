// rb3-app native glue (lane A1, 2026-11-22). Native only, rb3-app only.
//
// Real, minimal native definitions for globals rb3-app used to get from
// dta_link_stubs.s, where every data symbol was 128 weak ZERO bytes. That is
// harmless for rb3-dta (nothing on its parse path reads them) and a silent
// crash for the real boot: two of these are C++ references, and zero storage
// for a reference is a null reference. Each definition below states the
// semantics it gives natively. Aborting function stubs are NOT here; they are
// in rb3_app_link_stubs.s/.cpp.
#include "net/WebSvcMgr.h"
#include "os/ContentMgr.h"
#include "os/CritSec.h"
#include "os/Debug.h"

// Retail: os/Debug.cpp defines it; no native target compiles that TU. The
// class is a 0x100-byte char buffer with no constructor, so static zero
// storage IS the retail initial state ("no message printed yet").
DebugNotifyOncePrinter TheDebugNotifyOncePrinter; // ring: native-real

// Retail: MemInit() (MemMgr.cpp, `#ifndef HX_NATIVE`) does
// `gMemStackLock = new CriticalSection()`. Natively the engine owns the heap
// and MemMgr.cpp's HX_NATIVE arm defines gMemLock but not this one, while the
// shared ThreadMemStack/MemTemp bodies still name it. Null = no lock:
// CritSecTracker (os/CritSec.h) skips a null section, the same state those
// bodies see on retail before MemInit runs.
CriticalSection *gMemStackLock = nullptr; // ring: native-real

// Retail: ContentMgr_Xbox.cpp binds TheContentMgr to the Xbox content manager
// (DLC packages on the hard drive / marketplace). Natively there is no
// content storage: a base ContentMgr means no content packages, no refresh in
// flight, an on-disc-only session. Same binding bandtrack_link_stubs.cpp
// (rb3-render's phase stubs) has used since W16-TJ.
static ContentMgr gRb3AppContentMgr;
ContentMgr &TheContentMgr = gRb3AppContentMgr; // ring: native-real

// Retail: WebSvcMgrCurl.cpp binds TheWebSvcMgr to the curl web-service
// manager; rb3-app excludes it (online ring, curl). An offline WebSvcMgr: the
// base class (net/WebSvcMgr.cpp, compiled here) with an empty request list, so
// SystemPoll's TheWebSvcMgr.Poll() walks nothing. The three transport hooks
// the base leaves pure are the online ring and abort if anything reaches them.
extern "C" [[noreturn]] void rb3_app_stub_reached(const char *ring, const char *fn);
namespace {
class Rb3AppOfflineWebSvcMgr : public WebSvcMgr {
public:
    bool DoRequest(ReqType, unsigned int, unsigned short, const char *, const char *,
                   unsigned int, const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::DoRequest"); // ring: online-websvc
    }
    bool InitRequest(WebSvcRequest *, ReqType, const char *, unsigned short,
                     const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::InitRequest(host)"); // ring: online-websvc
    }
    bool InitRequest(WebSvcRequest *, ReqType, unsigned int, unsigned short,
                     const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::InitRequest(ip)"); // ring: online-websvc
    }
};
Rb3AppOfflineWebSvcMgr gRb3AppWebSvcMgr;
} // namespace
WebSvcMgr &TheWebSvcMgr = gRb3AppWebSvcMgr; // ring: native-real
