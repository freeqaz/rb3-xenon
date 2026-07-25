#include "meta_band/MusicLibraryStore.h"
#include "meta/StoreOffer.h"
#include "meta/StorePreviewMgr.h"
#include "obj/Data.h"
#include "os/ContentMgr.h"
#include "rndobj/Tex.h"
#include "utl/NetCacheMgr.h"
#include "utl/NetLoader.h"
#include "utl/Std.h"
#include "xdk/xapilibi/xbox.h"

MusicLibraryStore::MusicLibraryStore()
    : mState(2), mPreviewLoader(NULL), mPreviewData(NULL), mCacheLoader(NULL),
      mCacheStream(NULL), mPreviewTex(Hmx::Object::New<RndTex>()), mPreviewMgr(NULL),
      mResults(NULL), mUnk60(0) {
    mState = 0;
    TheNetCacheMgr->Load((NetCacheMgr::CacheSize)0);
    mPreviewMgr = new StorePreviewMgr();
}

void MusicLibraryStore::ClearPreview() {
    if (mPreviewLoader) {
        delete mPreviewLoader;
    }
    mPreviewLoader = NULL;
    if (mPreviewMgr) {
        delete mPreviewMgr;
    }
    mPreviewMgr = NULL;
    if (mCacheLoader) {
        TheNetCacheMgr->DeleteNetCacheLoader(mCacheLoader);
        mCacheLoader = NULL;
    }
    TheNetCacheMgr->Unload();
    // Retail 0x825BC908 constructs Symbol("content_installed") inline and calls
    // MsgSource::RemoveSink on the global ContentMgr @0x82CC9D20.
    TheContentMgr.RemoveSink(this, Symbol("content_installed"));
    mState = 3;
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_AUTO);
}

MusicLibraryStore::~MusicLibraryStore() {
    DeleteAll(mOffers);
    if (mPreviewData) {
        mPreviewData->Release();
        mPreviewData = NULL;
    }
    mOverlapped.clear();
    delete mPreviewTex;
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
