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

/* THERE IS NO SEPARATE SetlistMetadataLoadedMsg -- it is MetadataLoadedMsg.
 * This file used to declare a local decl-only class of that name, on the
 * correct observation that retail's ctor here takes RAW SCALARS (r4..r8) and
 * that its out-of-line body is fn_82606020 over in BandStorePanel.cpp.  Both
 * observations were right; the conclusion that it was a DIFFERENT class was
 * not.  BandStorePanel::Poll's two message sites call THE SAME retail function
 * fn_82606020, and one function cannot be two classes' constructors, so there
 * is exactly one message type and MetadataLoadedMsg (BandSongMetadata.h, which
 * this file already includes) is it.
 *
 * This mattered METRICALLY, not just tidily.  Once fn_82606020 is named in
 * scripts/target_symbol_map.json, name_check stops forgiving it as a
 * placeholder and starts comparing the callee NAME -- at which point spelling
 * it SetlistMetadataLoadedMsg here charged this file's Poll (1,196 B) and
 * knocked it off 100%.  Measured: -1,196 B for the map edit alone, recovered
 * in full by this change.
 *
 * Also note the retired comment's claim that "`functionRelocDiffs=none` makes
 * the callee address score-invisible" is STALE: name_check has been the
 * shipped ruler since 2026-08-12, so the callee name is score-VISIBLE and is
 * precisely what the paragraph above is about.
 *
 * The three scalar parameters are bool, not int: fn_82606020 applies
 * `clrlwi rN, rN, 24` to r5/r7/r8 before storing them into the DataNode
 * integer, which is the bool->int widening and would not be emitted for int.
 * A caller cannot tell the two apart -- only that callee body can -- which is
 * why this file previously spelled them int. */

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
    static MetadataLoadedMsg msg(mAllMetadata, true, gNullStr, false, false);
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
