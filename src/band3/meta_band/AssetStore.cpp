#include "meta_band/AssetStore.h"
#include "meta/StorePurchaser.h"
#include "net/Net.h"
#include "net/Server.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/ContentMgr.h"
#include "os/PlatformMgr.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "utl/MakeString.h"
#include "utl/NetCacheMgr.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "xdk/xapilibi/xbox.h"

// Every body here is read off the retail XEX (TU5); addresses are the retail
// function starts.

/** Retail 0x828406C0; see MusicLibraryStore.cpp. */
extern "C" DWORD XMarketplaceGetDownloadStatus(
    DWORD dwUserIndex, unsigned long long qwOfferID, DWORD *pdwResult
);

// Retail global @0x82C73C74 (a pointer variable in .data): LoadOffers formats it
// with the platform and the platform region.
const char *AssetStore::sIndexFile = "tshirt_index_%s_%s.dta";

// Retail 0x825EC840. Starting the store again (state 4) re-registers for
// content_installed.
void AssetStore::RefreshOffers(LocalBandUser *user) {
    mUser = user;
    if (mState == 4) {
        mState = 0;
        ThePlatformMgr.AddSink(this, Symbol("content_installed"));
    }
}

// Retail 0x825EC8C8.
bool AssetStore::HasAnyAssetOffers() const { return !mOffers.empty(); }

// Retail 0x825EC8E8.
void AssetStore::LoadOffers() {
    Symbol platform = PlatformSymbol(kPlatformXBox);
    Symbol region = PlatformRegionToSymbol(ThePlatformMgr.GetRegion());
    String url(MakeString(sIndexFile, platform.Str(), region.Str()));
    mOfferLoader = new DataNetLoader(url);
}

// Retail 0x825EC9E0: the offer whose bundle holds the asset.
AssetOffer *AssetStore::FindOffer(Symbol asset) {
    for (std::vector<AssetOffer *>::iterator it = mOffers.begin(); it != mOffers.end();
         ++it) {
        AssetOffer *offer = *it;
        if (offer->HasAsset(asset))
            return offer;
    }
    return NULL;
}

// Retail 0x825ECA38.
bool AssetStore::HasAssetOffer(Symbol asset) { return FindOffer(asset) != NULL; }

// Retail 0x825ECA68.
void AssetStore::ShowPurchaseUI(Symbol asset) {
    mPurchaseOffer = FindOffer(asset);
    static Symbol assetStore("assetStore");
    unsigned int flags = 0;
    Server *server = TheNet.GetServer();
    if (server && server->IsConnected()) {
        flags = server->GetPlayerID(mUser->GetPadNum());
    }
    // Retail reads the pad and the offer id before the allocation.
    int pad = mUser->GetPadNum();
    unsigned long long offerID = mPurchaseOffer->SongID();
    mPurchaser = new XboxPurchaser(pad, offerID, 0, 0, assetStore, flags);
    mPurchaser->Initiate();
}

// Retail 0x825ECC00.
void AssetStore::ShowPurchaseError() {
    if (TheUI->InTransition())
        return;
    static Symbol ok_msg("ok_msg");
    static Symbol ml_store_purchase_error("ml_store_purchase_error");
    UIScreen *screen =
        ObjectDir::Main()->Find<UIScreen>("basic_confirm_dialog_screen", true);
    screen->SetProperty(ok_msg, ml_store_purchase_error);
    TheUI->PushScreen(screen);
}

// Retail 0x825ECD58.
AssetStore::AssetStore()
    : mState(4), mUser(NULL), mOfferLoader(NULL), mPurchaser(NULL),
      mPurchaseOffer(NULL), mDownloadOffer(NULL) {}

// Retail 0x825ECDC8.
AssetStore::~AssetStore() {
    DeleteAll(mOffers);
    RELEASE(mPurchaser);
    RELEASE(mOfferLoader);
}

// Retail 0x825ECED0: rebuilds the offer list from the loaded index's
// (offers ...) array.
void AssetStore::ParseOffers() {
    DataArray *data = mOfferLoader->GetUnk4();
    if (data->Size() != 0) {
        DataArray *arr = data->FindArray("offers", true);
        DeleteAll(mOffers);
        for (int i = 1; i < arr->Size(); i++) {
            DataArray *offerData = arr->Array(i);
            AssetOffer *offer = new AssetOffer(offerData);
            mOffers.push_back(offer);
        }
    }
}

DataNode AssetStore::OnMsg(const ContentInstalledMsg &) {
    TheContentMgr.StartRefresh();
    return 1;
}

BEGIN_HANDLERS(AssetStore)
    HANDLE_MESSAGE(ContentInstalledMsg)
    HANDLE_CHECK(0)
END_HANDLERS

// Retail 0x825ED020.
void AssetStore::Poll() {
    switch (mState) {
    case 0:
        TheNetCacheMgr->Load((NetCacheMgr::CacheSize)0);
        mState = 1;
        break;
    case 1:
        if (TheNetCacheMgr->IsReady()) {
            mState = 2;
        } else if (TheNetCacheMgr->GetHasFailed()) {
            TheNetCacheMgr->Unload();
            mState = 4;
        }
        break;
    case 2:
        if (!mOfferLoader)
            LoadOffers();
        mOfferLoader->PollLoading();
        if (mOfferLoader->IsLoaded()) {
            ParseOffers();
            RELEASE(mOfferLoader);
            TheNetCacheMgr->Unload();
            mState = 3;
        } else if (mOfferLoader->HasFailed()) {
            RELEASE(mOfferLoader);
            TheNetCacheMgr->Unload();
            mState = 4;
        }
        break;
    }
    if (mPurchaseOffer) {
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            if (mPurchaser->IsSuccess() && mPurchaser->PurchaseMade()) {
                mDownloadOffer = mPurchaseOffer;
            }
            if (!mPurchaser->IsSuccess()) {
                ShowPurchaseError();
            }
            mPurchaseOffer = NULL;
            RELEASE(mPurchaser);
        }
    }
    if (mDownloadOffer) {
        DWORD status;
        int pad = mUser->GetPadNum();
        XMarketplaceGetDownloadStatus(pad, mDownloadOffer->SongID(), &status);
        if (status == ERROR_SUCCESS) {
            mDownloadOffer = NULL;
            TheContentMgr.StartRefresh();
        } else if (status == ERROR_DISK_FULL) {
            mDownloadOffer = NULL;
        } else if (status == 0x490 || status == 0x4004) {
            mDownloadOffer = NULL;
            TheContentMgr.StartRefresh();
        }
    }
}
