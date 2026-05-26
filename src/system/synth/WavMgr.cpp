#include "synth/WavMgr.h"
#include "obj/Dir.h"
#include "obj/Object.h"

#ifdef HX_NATIVE
WavMgrAllocFunc WavMgr::sAlloc;
WavMgrFreeFunc WavMgr::sFree;
#endif

WavMgr gWavMgr;
WavMgr *TheWavMgr = &gWavMgr;

void WavMgr::SetAllocator(WavMgrAllocFunc allocFunc, WavMgrFreeFunc freeFunc) {
    sAlloc = allocFunc;
    sFree = freeFunc;
}

void WavMgr::OnReleaseResource(void *v) { sFree(v, __FILE__, 0x2F, "WavMgr"); }

void InitWavMgr() { gWavMgr.SetName("wavmgr", ObjectDir::Main()); }

BEGIN_HANDLERS(WavMgr)
    HANDLE_ACTION(dump_stats, Dump())
END_HANDLERS

BEGIN_PROPSYNCS(WavMgr)
END_PROPSYNCS

WavMgr::WavMgr() {}

bool WavMgr::CreateSample(Hmx::CRC crc, void *&v, int size) {
    v = TheWavMgr->Get(crc);
    if (v)
        return true;
    else {
        v = sAlloc(size, __FILE__, 0x38, "WavMgr", 0);
        TheWavMgr->ReserveRes(crc, v);
        return false;
    }
}
