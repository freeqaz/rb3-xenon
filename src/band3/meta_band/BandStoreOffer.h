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
    // Wii-dev signature.  BandStorePanel::MakeNewOffer still calls it; that TU
    // has not been re-derived against retail, so the declaration stays (there is
    // no such ctor in the retail binary and it is intentionally undefined).
    BandStoreOffer(const StorePackedOfferBase *, SongMgr *, bool);
    virtual ~BandStoreOffer() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool IsCompletelyUnavailable() const;
    virtual bool Cmp(const StoreOffer &, Symbol) const;

    StorePurchaseable mDemo; // 0xe0
    StorePurchaseable mUpgrade; // 0x120
    bool mUpgradeAvailable; // 0x160
};
