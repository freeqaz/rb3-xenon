#include "VorbisMem.h"

#ifndef HX_NATIVE
// Native builds define OggMalloc/OggCalloc/OggRealloc/OggFree in
// native/src/native_link_glue.cpp (host malloc/free). Keep the Xbox/PPC
// implementations here behind the guard so the native link does not get
// duplicate definitions.
#include "utl/MemMgr.h"
#include <string.h>

// RB3-era shape (rb3-Wii oracle), NOT dc3's. dc3-decomp is newer and adds a
// file-scope `Licenses sLicense(...)` object here; retail RB3's pinned span for
// this TU carries no static-init funclet, so the Licenses object is dc3-only
// drift. rb3-Wii has no Licenses here either.
//
// (CORRECTED, lane W13-CHARINFO: this used to say the pinned span "is exactly 8
// bytes (0x82C30B00-0x82C30B08 = OggMalloc alone)". The 8-byte OggMalloc figure
// is right, but "alone" is not — VorbisMem.s spans 0xC across TWO blocks,
// fn_8249B200 (4 B) and fn_82C30B00 (8 B). The no-static-init conclusion is
// unaffected; only the "8 bytes / alone" span description was wrong.)
//
// The parenthesized `(_MemAllocTemp)` bypasses MemMgr.h's 5-arg debug macro and
// calls the retail 2-arg (size, align) allocator directly, which is what retail
// emits: `li r4, 0x0; b ?_MemAllocTemp@@YAPAXHH@Z`.

void *OggMalloc(int i) { return (_MemAllocTemp)(i, 0); }

void *OggCalloc(int i1, int i2) {
    int mult = i1 * i2;
    void *tmp = (_MemAllocTemp)(mult, 0);
    memset(tmp, 0, mult);
    return tmp;
}

void *OggRealloc(void *v, int i) {
    return MemRealloc(v, i, __FILE__, 0x2B, "Ogg_Internal", 0);
}

void OggFree(void *v) { MemFree(v); }
#endif // HX_NATIVE
