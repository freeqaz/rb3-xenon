#pragma once
#include "meta/StoreOffer.h"

class StorePackedOfferBase;

// Retail RB3-360 layout, read off ??0BandStoreOffer (fn_8266E548) and
// ??1BandStoreOffer (fn_8266E7A0): the ctor runs ??0StorePurchaseable twice, on
// this+0xe0 and this+0x120, then zeroes a bool at this+0x160; the dtor runs
// ??1Object@Hmx@@ on this+0x120 and this+0xe0 before ??1StoreOffer.  sizeof
// StorePurchaseable is 0x40 and StoreOffer ends at 0xe0 (mSongsInOffer @0xd4).
class BandStoreOffer : public StoreOffer {
public:
    BandStoreOffer(DataArray *, SongMgr *);
    // REMOVED (lane STOREPANEL, 2026-08-22): the rb3-Wii dev-build 3-arg ctor
    // (const StorePackedOfferBase *, SongMgr *, bool).  It was declared-but-never-
    // defined solely so the old BandStorePanel::MakeNewOffer(ptr, bool) would
    // compile.  Retail's BandStorePanel::MakeNewOffer (0x82605778) calls
    // ??0BandStoreOffer@@QAA@PAVDataArray@@PAVSongMgr@@@Z (0x8266e548) with r4 =
    // its single DataArray * parameter and r5 = TheSongMgrPtr, and sets no r6 --
    // so retail has only the 2-arg form and nothing references the 3-arg one now.
    // NOTE(laneCD8): destructor deliberately NOT declared -- see SyncStore.h. An
    // explicit `virtual ~BandStoreOffer() {}` adds a 3-instruction derived-vptr
    // store at dtor entry that retail does not have. Implicit member-destruction
    // order (mUpgrade@0x120 then mDemo@0xe0, then ~StoreOffer) matches retail.
    virtual DataNode Handle(DataArray *, bool);
    virtual bool IsCompletelyUnavailable() const;
    virtual bool Cmp(const StoreOffer &, Symbol) const;

    StorePurchaseable mDemo; // 0xe0
    StorePurchaseable mUpgrade; // 0x120
    bool mUpgradeAvailable; // 0x160
};
