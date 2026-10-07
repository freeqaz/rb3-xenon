// w16tr_link_stubs.cpp -- the Xbox Live Marketplace edge of a live StorePanel
// (W16-TR). rb3-render only; the X360 build never compiles this file.
//
// w16tr_phase.cpp constructs a StorePanel subclass, which makes StorePanel's
// vtable reachable under --gc-sections. Two of its bodies construct the Xbox
// Live Marketplace backend: EnumerateOffers builds an XboxEnumeration
// (meta/StoreEnumeration.cpp: XMarketplaceCreateOfferEnumerator / XEnumerate)
// and CheckOut builds an XboxPurchaser (meta/StorePurchaser.cpp:
// XShowMarketplaceDownloadItemsUI). Native replaces that backend by rule
// (W16-TM §3, StoreEnumeration.cpp) and has no Marketplace to talk to, so
// neither TU is compiled and no rb3-render path reaches either ctor.
//
// Every one FAILS LOUDLY, as bandtrack_link_stubs.cpp's do: a stub that returned a
// plausible object would silently stand in for the platform; one that aborts
// can only turn a run red.

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "meta/StoreEnumeration.h"
#include "meta/StorePurchaser.h"

[[noreturn]] static void W16TRUnreached(const char *fn) {
    fprintf(stderr, "w16tr_link_stubs: UNREACHED stub called: %s\n", fn);
    fflush(stderr);
    abort();
}
#define UNREACHED() W16TRUnreached(__PRETTY_FUNCTION__)

// The ctors store each class's vptr, so the vtables (keyed on the out-of-line
// destructors) are emitted here too: every virtual aborts as well.
XboxEnumeration::XboxEnumeration(int, std::vector<unsigned long long> *) { UNREACHED(); }
XboxEnumeration::~XboxEnumeration() { UNREACHED(); }
void XboxEnumeration::Start() { UNREACHED(); }
bool XboxEnumeration::IsEnumerating() const { UNREACHED(); }
bool XboxEnumeration::IsSuccess() const { UNREACHED(); }
void XboxEnumeration::Poll() { UNREACHED(); }

XboxPurchaser::XboxPurchaser(
    int, unsigned long long, unsigned long long, unsigned long long, Symbol s, unsigned int i
)
    : StorePurchaser(s, i) {
    UNREACHED();
}
XboxPurchaser::~XboxPurchaser() { UNREACHED(); }
void XboxPurchaser::Initiate() { UNREACHED(); }
bool XboxPurchaser::IsPurchasing() const { UNREACHED(); }
bool XboxPurchaser::IsSuccess() const { UNREACHED(); }
bool XboxPurchaser::PurchaseMade() const { UNREACHED(); }
void XboxPurchaser::Poll() { UNREACHED(); }
