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
    // laneCN-3: the ctor was missing the AddSink paired with the RemoveSink that
    // ClearPreview() already performs. Recovered from retail asm (15 pure deletes
    // at ctor idx 49-63): Symbol("content_installed") is built into a stack temp
    // then MsgSource::AddSink is called on lbl_82CC9D1C+4 = 0x82CC9D20 =
    // TheContentMgr (the same global a prior lane already identified for the
    // RemoveSink site). The third arg is loaded from the global lbl_82C71838,
    // which is gNullStr -- the SAME global RockCentral's `RemoveSink(this)` calls
    // pass when they supply no Symbol -- so it is a defaulted Symbol(), not a
    // named handler.
    TheContentMgr.AddSink(this, Symbol("content_installed"));
    // Residual (94.6%, 4 mismatches): retail takes TheContentMgr's address
    // DIRECTLY (lis/addi lbl_82CC9D1C) while we emit a pointer load, because
    // src/system/os/ContentMgr.h:191 declares `extern ContentMgr &TheContentMgr`
    // -- a REFERENCE where retail's is a plain object.
    // ⛔ DRAINED, MEASURED: flipping that decl to `extern ContentMgr TheContentMgr`
    // (26 TUs) is a whole-binary REGRESSION -- matched_functions 43149 -> 43112
    // (-37) and matched_code 4050980 -> 4033668 (-17,312 B). Reverted; baseline
    // restored exactly. So the reference decl is RIGHT for the rest of the tree
    // and this one call site is not worth 37 functions. Do not retry the flip;
    // if this site is ever revisited it needs a site-local lever, not a header
    // change.
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
