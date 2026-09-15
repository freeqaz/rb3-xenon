#include "net/NetSearchResult.h"
#include "net/MatchmakingSettings.h"
#include "net/NetSession.h"
#include "obj/ObjMacros.h"
#include "utl/BinStream.h"
#include "utl/MemStream.h"

// Ported from rb3-Wii src/network/net/NetSearchResult.cpp (lane W16-BN,
// 2026-09-15). Retail 360 bytes live at 0x823F56F8-0x823F5C98 (15 functions,
// 1,396 function bytes) -- a span that was mis-pinned under `UI.cpp:` and is
// re-homed to this TU in the same commit series.
//
// Wii -> 360 deltas, all established from retail bytes rather than assumed:
//  * member offsets. The Wii oracle header documents mSessionData at 0x1c;
//    retail 360 stores it at 0x28 (`stw r3, 0x28(r30)` in the ctor) because
//    Hmx::Object is 12 bytes larger here. src/network/net/NetSearchResult.h
//    already carries the corrected 0x28/0x2c/0x30/0x34 offsets, and all four
//    are witnessed by the retail ctor.
//  * handler dialect. Retail's Handle (0x823F59B0) builds its dispatch Symbol
//    as a FUNCTION-LOCAL STATIC -- guard word at 0x82CC001C with the
//    lis/lwz/clrlwi. bit test, `ori r11,r11,1` to set it, an inline
//    ??0Symbol@@QAA@PBD@Z on "get_mode_name", and a ??__F atexit funclet at
//    0x823F5A68 that clears the same bit. That is the RB3_HANDLE_LOCAL_STATIC
//    spelling, so this object is compiled with /DRB3_HANDLE_LOCAL_STATIC (see
//    config/45410914/objects.json) exactly like its SessionSearcher neighbour.
//  * sizeof corroboration. The factory allocates 0x40 (= 0x34 + 12-byte
//    String) and the ctor's inner allocation is 0x28, which is exactly
//    sizeof(MatchmakingSettings) under our header.

NetSearchResult *NetSearchResult::New() { return new NetSearchResult(); }

NetSearchResult::NetSearchResult() {
    mSessionData = SessionData::New();
    mSettings = new MatchmakingSettings();
    MemStream stream(false);
    TheNetSession->mSettings->Save(stream);
    stream.Seek(0, BinStream::kSeekBegin);
    mSettings->Load(stream);
    mNumOpenSlots = TheNetSession->NumOpenSlots();
}

NetSearchResult::~NetSearchResult() {
    delete mSessionData;
    delete mSettings;
}

void NetSearchResult::Save(BinStream &bs) const {
    mSessionData->Save(bs);
    mSettings->Save(bs);
    bs << (unsigned char)mNumOpenSlots;
    bs << mHostName;
}

void NetSearchResult::Load(BinStream &bs) {
    mSessionData->Load(bs);
    mSettings->Load(bs);
    unsigned char slots;
    bs >> slots;
    mNumOpenSlots = slots;
    bs >> mHostName;
}

bool NetSearchResult::Equals(const NetSearchResult *res) const {
    return mSessionData->Equals(res->mSessionData) && mNumOpenSlots == res->mNumOpenSlots
        && mHostName == res->mHostName;
}

BEGIN_HANDLERS(NetSearchResult)
    HANDLE_EXPR(get_mode_name, mSettings->mModeName)
    HANDLE_CHECK(0x4F)
END_HANDLERS
