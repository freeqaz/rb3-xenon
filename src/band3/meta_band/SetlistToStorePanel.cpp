#include "meta_band/SetlistToStorePanel.h"
#include "meta/StorePackedMetadata.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SavedSetlist.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
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

void SetlistToStorePanel::Poll() { UIPanel::Poll(); }

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
    HANDLE_ACTION(load_song_metadata, 0)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xE3)
END_HANDLERS
