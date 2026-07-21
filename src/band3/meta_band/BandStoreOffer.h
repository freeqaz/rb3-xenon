#pragma once
#include "meta/StoreOffer.h"

class StorePackedOfferBase;

class BandStoreOffer : public StoreOffer {
public:
    BandStoreOffer(const StorePackedOfferBase *, SongMgr *, bool);
    virtual ~BandStoreOffer() {}
    virtual DataNode Handle(DataArray *, bool);
    virtual bool IsCompletelyUnavailable() const;
    virtual bool Cmp(const StoreOffer &, Symbol) const;

    bool mUpgradeAvailable; // 0x7c
};