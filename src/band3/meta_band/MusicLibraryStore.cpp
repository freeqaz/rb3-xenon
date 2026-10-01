#include "meta_band/MusicLibraryStore.h"
#include "meta/StoreOffer.h"
#include "meta/StorePreviewMgr.h"
#include "meta/StorePurchaser.h"
#include "meta_band/BandProfile.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/Net.h"
#include "net/Server.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/ContentMgr.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "os/User.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "utl/BufStream.h"
#include "utl/MakeString.h"
#include "utl/NetCacheLoader.h"
#include "utl/NetCacheMgr.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "xdk/xapilibi/xbox.h"

/** Retail 0x828406C0, the import thunk next to XBackgroundDownloadSetMode's
    (0x828406B8). Only its shape is load-bearing, and Poll shows it:
    (dwUserIndex, 64-bit offer id, DWORD *status), the status then compared
    against ERROR_SUCCESS / ERROR_IO_PENDING and three error codes. The name is
    the XDK API with that signature; the address is unnamed in our map. */
extern "C" DWORD XMarketplaceGetDownloadStatus(
    DWORD dwUserIndex, unsigned long long qwOfferID, DWORD *pdwResult
);

// Retail global @0x82C73534 (a pointer variable in .data, loaded with lwz):
// LoadOffers formats it with the platform region, the platform and the system
// language.
const char *MusicLibraryStore::sUpsellIndexFile = "dlc_store/%s/dlc_upsell_%s_%s.dta";

MusicLibraryStore::MusicLibraryStore()
    : mState(2), mOfferLoader(NULL), mOfferData(NULL), mArtLoader(NULL),
      mArtReceiver(NULL), mStoreArt(Hmx::Object::New<RndTex>()), mPreviewMgr(NULL),
      mPendingPurchase(NULL), mPurchaser(NULL) {
    mState = 0;
    TheNetCacheMgr->Load((NetCacheMgr::CacheSize)0);
    mPreviewMgr = new StorePreviewMgr();
    // Retail builds Symbol("content_installed") into a stack temp and calls
    // MsgSource::AddSink on lbl_82CC9D1C+4. lbl_82CC9D1C is ThePlatformMgr (its
    // MsgSource base is at +4), which is what sends ContentInstalledMsg
    // (PlatformMgr_Xbox.cpp). The third arg is gNullStr (lbl_82C71838), i.e. a
    // defaulted Symbol(), not a named handler.
    ThePlatformMgr.AddSink(this, Symbol("content_installed"));
}


/* Retail 0x825BC900: a frameless 2-instruction tail-jump
   `lwz r3,0x4c(r3); b 0x827B1B78`. */
void MusicLibraryStore::ClearCurrentPreview() { mPreviewMgr->ClearCurrentPreview(); }

void MusicLibraryStore::ClearPreview() {
    if (mOfferLoader) {
        delete mOfferLoader;
    }
    mOfferLoader = NULL;
    if (mPreviewMgr) {
        delete mPreviewMgr;
    }
    mPreviewMgr = NULL;
    if (mArtLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mArtLoader);
        mArtLoader = NULL;
    }
    TheNetCacheMgr->Unload();
    // Retail 0x825BC908: MsgSource::RemoveSink on ThePlatformMgr+4, mirroring
    // the ctor's AddSink.
    ThePlatformMgr.RemoveSink(this, Symbol("content_installed"));
    mState = 3;
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
}

void MusicLibraryStore::LoadOffers() {
    if (mState == 1)
        return;
    delete mOfferLoader;
    Symbol region = PlatformRegionToSymbol(ThePlatformMgr.GetRegion());
    Symbol platform = PlatformSymbol(kPlatformXBox);
    Symbol language = SystemLanguage();
    String url(MakeString(sUpsellIndexFile, region, platform.Str(), language.Str()));
    Server *server = TheNet.GetServer();
    BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
    if (server && server->IsConnected() && profile) {
        url += MakeString("?pid=%u", server->GetPlayerID(profile->GetPadNum()));
    }
    mOfferLoader = new DataNetLoader(url);
}

bool MusicLibraryStore::IsDownloading(int songID) const {
    for (std::vector<OverlappedIO>::const_iterator it = mDownloads.begin();
         it != mDownloads.end();
         ++it) {
        if (it->mSongID == songID)
            return true;
    }
    return false;
}

void MusicLibraryStore::LoadStoreArt(int songID, Hmx::Object *receiver) {
    StoreOffer *offer = FindOfferBySongID(songID);
    if (mArtLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mArtLoader);
    }
    static Symbol preview_art("preview_art");
    String path("dlc_store");
    path += offer->GetData(DataArrayPtr(preview_art), false).Str();
    mArtLoader = TheNetCacheMgr->AddNetCacheLoader(path.c_str(), (NetLoaderPos)1);
    mArtReceiver = receiver;
}

void MusicLibraryStore::SetStorePreview(int songID) {
    static Symbol preview_audio("preview_audio");
    StoreOffer *offer = FindOfferBySongID(songID);
    if (offer->HasData(preview_audio)) {
        String path("dlc_store");
        path += offer->GetData(DataArrayPtr(preview_audio), false).Str();
        if (mPreviewMgr->AllowPreviewDownload(path)) {
            mPreviewMgr->DownloadPreviewFile(path);
        }
        mPreviewMgr->SetCurrentPreviewFile(path);
    } else {
        mPreviewMgr->ClearCurrentPreview();
    }
}

void MusicLibraryStore::ShowPurchaseError(int signedIn) {
    if (TheUI->InTransition())
        return;
    static Symbol ok_msg("ok_msg");
    static Symbol ml_store_purchase_error("ml_store_purchase_error");
    static Symbol ml_store_signin("ml_store_signin");
    UIScreen *screen =
        ObjectDir::Main()->Find<UIScreen>("basic_confirm_dialog_screen", true);
    if (!signedIn) {
        screen->SetProperty(ok_msg, ml_store_signin);
    } else {
        screen->SetProperty(ok_msg, ml_store_purchase_error);
    }
    TheUI->PushScreen(screen);
}

DataNode MusicLibraryStore::OnMsg(const ContentInstalledMsg &) {
    TheContentMgr.StartRefresh();
    return 1;
}

// Retail 0x825BD260; AssetStore's vtable points at the same body (ICF). No
// superclass forward: an unhandled message only warns (PathName(this)).
BEGIN_HANDLERS(MusicLibraryStore)
    HANDLE_MESSAGE(ContentInstalledMsg)
    HANDLE_CHECK(0)
END_HANDLERS

void MusicLibraryStore::ParseOffers(DataArray *data, std::vector<StoreOffer *> &offers) {
    data->AddRef();
    DataArray *arr = data->FindArray("offers", true);
    DeleteAll(offers);
    for (int i = 1; i < arr->Size(); i++) {
        StoreOffer *offer = new StoreOffer(arr->Array(i), &TheSongMgr);
        // Residual (88.9): retail lays the single merged `delete` block out right
        // after the IsTest test and the ValidTitle test after it; ours lands
        // between ValidTitle and push_back. `||`, `&&` and if/else-if spellings
        // were tried; this one is closest.
        if (offer->IsTest()) {
            delete offer;
        } else if (!offer->ValidTitle()) {
            delete offer;
        } else {
            offers.push_back(offer);
        }
    }
    data->Release();
}

void MusicLibraryStore::PurchaseSongs(LocalUser *user, const std::vector<int> &songIDs) {
    if (!ThePlatformMgr.IsUserSignedIntoLive(user)) {
        ShowPurchaseError(0);
        return;
    }
    // Retail reads lbl_82DFEB88 (TheSessionMgr) and vcalls slot 9 of its
    // primary (Synchronizable-hoisted) vtable, IsLocal(), testing the byte.
    if (!TheSessionMgr->IsLocal()) {
        ShowPurchaseError(1);
        return;
    }
    std::vector<int> ids(songIDs);
    for (std::vector<int>::iterator it = ids.begin(); it != ids.end();) {
        if (IsDownloading(*it)) {
            it = ids.erase(it);
        } else {
            ++it;
        }
    }
    if (ids.empty())
        return;
    if (ids.size() > 1) {
        ids.resize(1);
    }
    StoreOffer *offer = FindOfferBySongID(ids.front());
    static Symbol music_library("music_library");
    unsigned int flags = 0;
    Server *server = TheNet.GetServer();
    if (server && server->IsConnected()) {
        flags = server->GetPlayerID(user->GetPadNum());
    }
    mPurchaser = new XboxPurchaser(
        user->GetPadNum(),
        offer->SongID(),
        offer->mAlbum.Exists() ? offer->mAlbum.SongID() : 0,
        offer->mPack.Exists() ? offer->mPack.SongID() : 0,
        music_library,
        flags
    );
    mPendingPurchase = new PendingPurchase();
    mPendingPurchase->mSongIDs = ids;
    mPendingPurchase->mUser = user;
    mPurchaser->Initiate();
}

void MusicLibraryStore::Poll() {
    for (std::vector<OverlappedIO>::iterator it = mDownloads.begin(); it != mDownloads.end();) {
        StoreOffer *offer = FindOfferBySongID(it->mSongID);
        DWORD status;
        XMarketplaceGetDownloadStatus(it->mUser->GetPadNum(), offer->SongID(), &status);
        if (status == ERROR_SUCCESS) {
            it = mDownloads.erase(it);
            TheContentMgr.StartRefresh();
        } else if (status == ERROR_DISK_FULL) {
            it = mDownloads.erase(it);
            TheMusicLibrary->RefreshStoreDisplay();
        } else if (status == 0x490 || status == 0x4004) {
            it = mDownloads.erase(it);
            TheContentMgr.StartRefresh();
        } else if (status == ERROR_IO_PENDING) {
            ++it;
        }
    }
    if (mPendingPurchase) {
        mPurchaser->Poll();
        if (!mPurchaser->IsPurchasing()) {
            if (mPurchaser->IsSuccess()) {
                if (mPurchaser->PurchaseMade()) {
                    std::vector<int> &ids = mPendingPurchase->mSongIDs;
                    for (std::vector<int>::iterator it = ids.begin(); it != ids.end(); ++it) {
                        OverlappedIO d;
                        d.mSongID = 0;
                        d.mUser = 0;
                        mDownloads.push_back(d);
                        mDownloads.back().mSongID = *it;
                        mDownloads.back().mUser = mPendingPurchase->mUser;
                    }
                    TheMusicLibrary->RefreshStoreDisplay();
                }
            } else {
                ShowPurchaseError(1);
            }
            delete mPendingPurchase;
            mPendingPurchase = NULL;
            delete mPurchaser;
            mPurchaser = NULL;
        }
    }
    if (mArtLoader) {
        if (mArtLoader->IsLoaded()) {
            int size = mArtLoader->GetSize();
            char *buffer = mArtLoader->GetBuffer();
            RndBitmap bmp;
            BufStream bs(buffer, size, true);
            bmp.Load(bs);
            bmp.SetMip(NULL);
            TheNetCacheMgr->DeleteNetCacheLoader(mArtLoader);
            mStoreArt->SetBitmap(bmp, NULL, false);
            static Message art_loaded("art_loaded");
            mArtReceiver->Handle(art_loaded, false);
            mArtLoader = NULL;
            mArtReceiver = NULL;
        } else if (mArtLoader->HasFailed()) {
            MILO_WARN("Failed to load store art %s", mArtLoader->GetRemotePath());
            TheNetCacheMgr->DeleteNetCacheLoader(mArtLoader);
            mArtLoader = NULL;
            mArtReceiver = NULL;
        }
    }
    if (mPreviewMgr) {
        mPreviewMgr->Poll();
    }
    switch (mState) {
    case 0:
        if (TheNetCacheMgr->IsReady()) {
            mState = 2;
        } else if (TheNetCacheMgr->GetHasFailed()) {
            mState = 1;
        }
        break;
    case 2:
        if (mOfferLoader) {
            mOfferLoader->PollLoading();
            if (mOfferLoader->IsLoaded()) {
                if (mOfferData) {
                    mOfferData->Release();
                    mOfferData = NULL;
                }
                mOfferData = mOfferLoader->GetUnk4();
                mOfferData->AddRef();
                delete mOfferLoader;
                mOfferLoader = NULL;
                if (mOfferData->Size() != 0) {
                    ParseOffers(mOfferData, mOffers);
                    static Symbol song("song");
                    for (std::vector<StoreOffer *>::iterator it = mOffers.begin();
                         it != mOffers.end();) {
                        if ((*it)->OfferType() == song) {
                            ++it;
                        } else {
                            it = mOffers.erase(it);
                        }
                    }
                    TheMusicLibrary->RefreshSongLists();
                }
                XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
            } else if (mOfferLoader->HasFailed()) {
                delete mOfferLoader;
                mOfferLoader = NULL;
            }
        }
        break;
    case 3:
        if (TheNetCacheMgr->IsUnloaded()
            && (!mPurchaser || !mPurchaser->IsPurchasing())) {
            delete mPurchaser;
            mPurchaser = NULL;
            mState = 4;
        }
        break;
    }
}

MusicLibraryStore::~MusicLibraryStore() {
    DeleteAll(mOffers);
    if (mOfferData) {
        mOfferData->Release();
        mOfferData = NULL;
    }
    mDownloads.clear();
    delete mStoreArt;
}

StoreOffer *MusicLibraryStore::FindOfferBySongID(int id) const {
    for (std::vector<StoreOffer *>::const_iterator it = mOffers.begin();
         it != mOffers.end();
         ++it) {
        StoreOffer *offer = *it;
        if (offer->GetSingleSongID() == id)
            return offer;
    }
    return NULL;
}

// sw2 scatter-include (default/MusicLibraryStore <- band3/meta_band/Utl.cpp)
#define gRev gRev_Utl
#define gAltRev gAltRev_Utl
#include "band3/meta_band/Utl.cpp"
#undef gRev
#undef gAltRev
