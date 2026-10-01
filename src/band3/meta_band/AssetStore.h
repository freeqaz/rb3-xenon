#pragma once
#include "game/BandUser.h"
#include "meta/StoreOffer.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/Symbol.h"
#include <vector>

class ContentInstalledMsg;
class DataNetLoader;
class StorePurchaser;

/** One purchasable asset bundle from the store's tshirt index. Retail RTTI
    `.?AVAssetOffer@@` (vtable 0x820D8644), 0x50 bytes: StorePurchaseable plus
    the bundle's asset names at +0x40. Its bodies (ctor 0x8266B520, HasAsset
    0x8266B4C8) sit in an unpinned block and are declared only. */
class AssetOffer : public StorePurchaseable {
public:
    AssetOffer(DataArray *);
    bool HasAsset(Symbol) const;

    std::vector<Symbol> mAssets; // 0x40
};

/** Retail-only (Xbox 360) store for premium closet assets, embedded in
    ClosetMgr. RTTI `.?AVAssetStore@@`, vtable 0x820B9874, sizeof 0x4c. Its TU is
    0x825EC840-0x825ED2A0. No surviving source has it; everything here is read
    off the retail bytes, and the method names below that ClosetMgr does not
    call are ours. Handle (and so OnMsg) is ICF-folded with MusicLibraryStore's. */
class AssetStore : public Hmx::Object {
public:
    AssetStore();
    virtual ~AssetStore();
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    bool HasAssetOffer(Symbol);
    bool HasAnyAssetOffers() const;
    void ShowPurchaseUI(Symbol);
    void RefreshOffers(LocalBandUser *);
    bool IsDownloading() const { return mDownloadOffer != 0; }

    static const char *sIndexFile;

    int mState; // 0x28 (0 load cache, 1 waiting, 2 loading index, 3 ready, 4 idle/failed)
    LocalBandUser *mUser; // 0x2c
    DataNetLoader *mOfferLoader; // 0x30
    StorePurchaser *mPurchaser; // 0x34 (ClosetMgr::IsPurchaseUIActive tests it)
    AssetOffer *mPurchaseOffer; // 0x38
    AssetOffer *mDownloadOffer; // 0x3c (is_downloading)
    std::vector<AssetOffer *> mOffers; // 0x40

private:
    void LoadOffers();
    AssetOffer *FindOffer(Symbol);
    void ShowPurchaseError();
    void ParseOffers();
    DataNode OnMsg(const ContentInstalledMsg &);
};
