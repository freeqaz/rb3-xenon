#include "meta_band/SetlistToStorePanel.h"
#include "meta/StorePackedMetadata.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SavedSetlist.h"
#include "meta/StorePanel.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "utl/Std.h"
#include "utl/Symbols3.h"

void SetlistToStorePanel::Enter() {
    UIPanel::Enter();
    unk58.Restart();
}

void SetlistToStorePanel::Load() {
    UIPanel::Load();
    MILO_ASSERT(!mAllMetadata, 0x1F);
    MILO_ASSERT(mLoaders.empty(), 0x20);
}

// Retail X360 wires `load_song_metadata` to a real method (fn_82642B38); the
// rb3-Wii DEV build's HANDLE_ACTION(load_song_metadata, 0) is a stub.  The
// retail body kicks off the metadata net-loaders (fn_826429A0, not yet ported --
// it is outside this unit's pinned span so it is unscored) and then seeds
// mAllMetadata with a one-element `offers` array.
void SetlistToStorePanel::LoadSongMetadata() {
    static Symbol offers("offers");
    mAllMetadata = new DataArray(1);
    mAllMetadata->Node(0) = DataArrayPtr(DataNode(offers));
}

/** Retail's payload message for the setlist-upsell store hand-off.
 *  Poll() builds it as a function-local static (guard bit 0x10) from the same
 *  five DataNode values BandStorePanel::Poll uses for MetadataLoadedMsg --
 *  (metadata, 1, gNullStr, 0, 0) -- but hands them over as raw scalars
 *  (r4..r8), so retail's ctor takes scalars and wraps them in DataNodes
 *  itself.  Its out-of-line body lives in BandStorePanel.cpp (fn_82606020,
 *  between ?SetType@BandStorePanel@@ and ?Instance@BandStorePanel@@), which is
 *  why this is a single `bl` rather than an inlined DataArray build-up.
 *  Decl-only ctor: the match build never links, and `functionRelocDiffs=none`
 *  makes the callee address score-invisible -- only the argument shape scores. */
class SetlistMetadataLoadedMsg : public Message {
public:
    SetlistMetadataLoadedMsg(DataArray *, int, const char *, int, int);
};

/** Retail's timeout-screen lookup (fn_82272308).  It begins exactly where
 *  ?FindSym@DataArray@@ ends, so it is a Find*-family sibling in obj/Data.cpp:
 *  called on a cached global with a literal key, returning the screen to jump
 *  to.  The return type is pinned by the compiler, not by header order --
 *  retail dispatches at UIManager own-vtable offset 0x10, and MSVC emits an
 *  overload set into the vtable in REVERSE declaration order, so 0x10 is the
 *  UIScreen* overload (measured: calling the const char* overload emits 0x14).
 *  That offset is a literal, not a relocation, so it is score-visible.
 *  Decl-only; the exact retail symbol is unidentified. */
class UIScreen;
extern DataArray *gStoreScreenCfg;
UIScreen *FindStoreScreen(DataArray *, const char *, bool);

void SetlistToStorePanel::Poll() {
    UIPanel::Poll();
    unk58.Split();
    if (unk58.Ms() > 58000.0f) {
        // Bound to a named local on purpose: written as a nested call, MSVC
        // hoists the TheUI load and its vptr into callee-saved registers ahead
        // of the lookup.  Retail evaluates the lookup first, then loads TheUI.
        UIScreen *screen =
            FindStoreScreen(gStoreScreenCfg, "setlist_to_store_screen_timeout", true);
        TheUI->GotoScreen(screen, false, false);
        return;
    }
    bool ready = mLoaders.size() != 0;
    for (std::vector<DataNetLoader *>::iterator it = mLoaders.begin();
         it != mLoaders.end();
         ++it) {
        DataNetLoader *loader = *it;
        loader->PollLoading();
        if (!loader->IsLoaded() && !loader->HasFailed()) {
            ready = false;
            break;
        }
    }
    if (!ready)
        return;
    const std::vector<int> &songs = mSongs;
    if (songs.size() != mLoaders.size()) {
        StartMetadataLoaders();
        return;
    }
    for (std::vector<DataNetLoader *>::iterator it = mLoaders.begin();
         it != mLoaders.end();
         ++it) {
        DataNetLoader *loader = *it;
        DataArray *offer = nullptr;
        if (loader->IsLoaded()) {
            static Symbol offers("offers");
            DataArray *found = loader->GetUnk4()->FindArray(offers, false);
            if (found) {
                offer = found->Array(1);
                offer->AddRef();
            }
        }
        if (!offer) {
            static Symbol store("store");
            static Symbol dummy_upsell_offer("dummy_upsell_offer");
            offer = SystemConfig(store, dummy_upsell_offer)->Clone(true, true, 0);
            const String &songName = mSongNames[mAllMetadata->Array(0)->Size() - 1];
            if (!songName.empty()) {
                DataNode nameNode(songName.c_str());
                offer->FindArray(Symbol("name"), true)->Node(1) = nameNode;
            }
        }
        DataArray *offers_arr = mAllMetadata->Array(0);
        {
            DataNode offerNode(offer, kDataArray);
            offers_arr->Insert(offers_arr->Size(), offerNode);
        }
        offer->Release();
    }
    static Symbol setlist_upsell("setlist_upsell");
    StorePanel::Instance()->SetSource(setlist_upsell, true);
    MILO_ASSERT(mAllMetadata->Array(0), 0x62);
    static SetlistMetadataLoadedMsg msg(mAllMetadata, 1, gNullStr, 0, 0);
    {
        DataNode metaNode(mAllMetadata, kDataArray);
        msg[0] = metaNode;
    }
    StorePanel::Instance()->Handle(msg.mData, true);
    DeleteAll(mLoaders);
}

void SetlistToStorePanel::GetSongsFromMusicLibrary() {
    SavedSetlist *setlist = TheMusicLibrary->mCurrentSetlist;
    MILO_ASSERT(setlist, 0x8B);
    NetSavedSetlist *netSetlist = dynamic_cast<NetSavedSetlist *>(setlist);
    const std::vector<int> &songs = setlist->mSongs;
    MILO_ASSERT(!songs.empty(), 0x91);
    // Retail X360 predates the StoreMetadataManager setlist-offer bookkeeping
    // the rb3-Wii dev build added here: neither ClearSetlistOffers() nor the
    // per-song AddSetlistOffer() below exists in the target, and their absence
    // is what frees the callee-save register the rest of the loop is off by.
    MILO_ASSERT(mSongs.empty(), 0x98);
    MILO_ASSERT(mSongNames.empty(), 0x99);
    for (int i = 0; i < songs.size(); i++) {
        int songID = songs[i];
        if (std::find(mSongs.begin(), mSongs.end(), songID) == mSongs.end()) {
            BandSongMetadata *meta = (BandSongMetadata *)TheSongMgr.Data(songID);
            if (!meta || meta->IsDownload()) {
                mSongs.push_back(songID);
                String title(netSetlist ? netSetlist->GetSongTitle(i) : gNullStr);
                mSongNames.push_back(title);
            }
        }
    }
    MILO_ASSERT(mSongs.size() == mSongNames.size(), 0xB2);
}

void SetlistToStorePanel::Unload() {
    mSongs.clear();
    mSongNames.clear();
    DeleteAll(mLoaders);
    if (mAllMetadata) {
        mAllMetadata->Release();
        mAllMetadata = nullptr;
    }
    UIPanel::Unload();
}

BEGIN_HANDLERS(SetlistToStorePanel)
    HANDLE_ACTION(get_songs_from_music_library, GetSongsFromMusicLibrary())
    HANDLE_ACTION(load_song_metadata, LoadSongMetadata())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xE3)
END_HANDLERS
