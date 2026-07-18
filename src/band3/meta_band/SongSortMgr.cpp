#include "meta_band/SongSortMgr.h"
#include "SongSortByRecent.h"
#include "SongSortByReview.h"
#include "SongSortMgr.h"
#include "beatmatch/TrackType.h"
#include "decomp.h"
#include "math/Rand.h"
#include "meta/StoreOffer.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SetlistSortByLocation.h"
#include "meta_band/SongRecord.h"
#include "meta_band/SongSort.h"
#include "meta_band/SongSortByArtist.h"
#include "meta_band/SongSortByDiff.h"
#include "meta_band/SongSortByPlays.h"
#include "meta_band/SongSortByRank.h"
#include "meta_band/SongSortBySong.h"
#include "meta_band/SongSortByStars.h"
#include "meta_band/Utl.h"
#include "game/BandUserMgr.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "stl/_pair.h"
#include "utl/BinStream.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include <algorithm>

SongSortMgr *TheSongSortMgr;

void SavedSetlist::SetTitle(const char *title) { mTitle = title; }
void SavedSetlist::SetDescription(const char *desc) { mDescription = desc; }

void SongSortMgr::Init() {
    MILO_ASSERT(!TheSongSortMgr, 0x29);
    TheSongSortMgr = new SongSortMgr();
}

DECOMP_FORCEACTIVE(SongSortMgr, "TheSongSortMgr")

SongSortMgr::SongSortMgr() {
    mSorts[kSongSortByDiff] = new SongSortByDiff();
    mSorts[kSongSortBySong] = new SongSortBySong();
    mSorts[kSongSortByArtist] = new SongSortByArtist();
    mSorts[kSongSortByStars] = new SongSortByStars();
    mSorts[kSongSortByRank] = new SongSortByRank();
    mSorts[kSongSortByRecent] = new SongSortByRecent();
    mSorts[kSongSortByPlays] = new SongSortByPlays();
    mSorts[kSongSortByReview] = new SongSortByReview();
    mSorts[kSetlistSortByLocation] = new SetlistSortByLocation();
}

SongSortMgr::~SongSortMgr() {
    for (int i = 0; i < kNumSongSortTypes; i++) {
        RELEASE(mSorts[i]);
    }
}

void SongSortMgr::SongFilter::IntersectFilter(SongSortMgr::SongFilter *filter) {
    MILO_ASSERT(filter, 0x51);
    std::vector<int> &excl = filter->excludedSongs;
    FOREACH (it, excl) {
        int song = *it;
        excl.push_back(song);
    }
    for (int i = 0; i < kNumFilterTypes; i++) {
        std::set<Symbol> &curSet = filter->filters[i];
        bool otherHasFilt = filter->HasFilterType((FilterType)i);
        if (HasFilterType((FilterType)i) && otherHasFilt) {
            std::vector<Symbol> v20;
            FOREACH_CONST_POST (it, filters[i]) {
                Symbol cur = *it;
                if (!filter->HasFilter((FilterType)i, cur)) {
                    v20.push_back(cur);
                }
            }
            FOREACH (it, v20) {
                RemoveFilter((FilterType)i, *it);
            }
            if (!HasFilterType((FilterType)i)) {
                MILO_WARN("Intersecting filters has resulted in an empty filter type!");
            }
        } else if (otherHasFilt) {
            FOREACH_CONST_POST (it, curSet) {
                AddFilter((FilterType)i, *it);
            }
        }
    }
}

void SongSortMgr::BuildSetlistList() {
    mSetlists.clear();
    if (mInternalSetlists.empty()) {
        BuildInternalSetlists();
    }
    FOREACH (it, mInternalSetlists) {
        SetlistRecord record(*it);
        std::pair<Symbol, SetlistRecord> p(record.GetToken(), record);
        mSetlists.insert(std::pair<const Symbol, SetlistRecord>(p));
    }
    std::vector<BandProfile *> profiles = TheProfileMgr.GetSignedInProfiles();
    FOREACH (pit, profiles) {
        const std::vector<LocalSavedSetlist *> &setlists = (*pit)->GetSavedSetlists();
        FOREACH (it, setlists) {
            SetlistRecord record(*it);
            std::pair<Symbol, SetlistRecord> p(record.GetToken(), record);
            mSetlists.insert(std::pair<const Symbol, SetlistRecord>(p));
        }
    }
    if (TheMusicLibrary->NetSetlistsSucceeded()) {
        std::vector<NetSavedSetlist *> setlists;
        TheMusicLibrary->GetNetSetlists(setlists);
        FOREACH (it, setlists) {
            SetlistRecord record(*it);
            std::pair<Symbol, SetlistRecord> p(record.GetToken(), record);
            mSetlists.insert(std::pair<const Symbol, SetlistRecord>(p));
        }
    }
#ifdef HX_NATIVE
    // mInternalSetlists is built only for internal setlists whose songs resolve in
    // TheSongMgr (BuildInternalSetlists). The 360-ARK extract's song_select
    // internal_setlists config references song shortnames that don't all match the
    // extract's songs.dta, and there are no signed-in-profile / net setlists
    // offline → mSetlists can be empty. The song-by-song browse (kNodeSong) works
    // without setlists; tolerate the empty setlist (playlist) view rather than
    // aborting. Reached from the song_select_enter music_library setup.
    if (mSetlists.empty()) {
        MILO_WARN("SongSortMgr: no setlists (360-ARK internal_setlists song "
                  "shortnames don't match extract songs.dta) — empty setlist view");
        return;
    }
#endif
    MILO_ASSERT(mSetlists.size(), 0xF0);
}

void SongSortMgr::BuildSortTree(SongSortType ty) {
    if (ty == kSetlistSortByLocation) {
        SetlistSort *sort = dynamic_cast<SetlistSort *>(mSorts[ty]);
        MILO_ASSERT(sort, 0xF8);
        sort->BuildSetlistTree(mSetlists);
    } else {
        SongSort *sort = dynamic_cast<SongSort *>(mSorts[ty]);
        MILO_ASSERT(sort, 0xFE);
        sort->BuildSongTree(mSongs, unk34);
    }
}

void SongSortMgr::BuildSortList(SongSortType ty) {
    if (ty == kSetlistSortByLocation) {
        SetlistSort *sort = dynamic_cast<SetlistSort *>(mSorts[ty]);
        MILO_ASSERT(sort, 0x108);
        sort->BuildSetlistList();
    } else {
        SongSort *sort = dynamic_cast<SongSort *>(mSorts[ty]);
        MILO_ASSERT(sort, 0x10E);
        sort->BuildSongList();
    }
}

void SongSortMgr::ClearAllSorts() {
    for (int i = 0; i < kNumSongSortTypes; i++) {
        mSorts[i]->DeleteList();
        mSorts[i]->DeleteTree();
        mSorts[i]->Clear();
    }
}

bool SongSortMgr::InqSongsForSetlist(Symbol s, std::vector<Symbol> &songVector) {
    static Symbol song_select("song_select");
    static Symbol internal_setlists("internal_setlists");
    static Symbol songs("songs");
    MILO_ASSERT(songVector.empty(), 0x120);
    DataArray *cfg = SystemConfig(song_select, internal_setlists);
    for (int i = 1; i < cfg->Size(); i++) {
        DataArray *arr = cfg->Array(i);
        if (arr->Sym(0) == s) {
            DataArray *songsArr = arr->FindArray(songs);
            for (int j = 1; j < songsArr->Size(); j++) {
                Symbol curSym = songsArr->Sym(j);
                songVector.push_back(curSym);
            }
            return true;
        }
    }
    return false;
}

void SongSortMgr::BuildInternalSetlists() {
    static Symbol song_select("song_select");
    static Symbol internal_setlists("internal_setlists");
    static Symbol music_library_visible("music_library_visible");
    static Symbol desc("desc");
    static Symbol date("date");
    static Symbol songs("songs");
    DataArray *cfg = SystemConfig(song_select, internal_setlists);
    for (int i = 1; i < cfg->Size(); i++) {
        DataArray *curArr = cfg->Array(i);
        bool visible = curArr->FindInt(music_library_visible);
        if (visible) {
            Symbol titleSym = curArr->Sym(0);
            Symbol descSym = curArr->FindSym(desc);
            SavedSetlist *setlist = new InternalSavedSetlist(titleSym, descSym);
            DataArray *dateArr = curArr->FindArray(date);
            setlist->SetDateTime(
                DateTime(dateArr->Int(1), dateArr->Int(2), dateArr->Int(3), 0, 0, 0)
            );
            DataArray *songsArr = curArr->FindArray(songs);
            for (int j = 1; j < songsArr->Size(); j++) {
                setlist->AddSong(
                    TheSongMgr.GetSongIDFromShortName(songsArr->Sym(j), true)
                );
            }
            mInternalSetlists.push_back(setlist);
        }
    }
}

void SongSortMgr::ClearInternalSetlists() {
    FOREACH (it, mInternalSetlists) {
        delete *it;
    }
    mInternalSetlists.clear();
}

#pragma push
#pragma pool_data off
void SongSortMgr::BuildFilteredSongList(SongFilter *filter, Symbol partSym) {
    std::vector<int> songs;
    TheSongMgr.GetRankedSongs(songs, true, true);
    mSongs.clear();
    FOREACH (it, songs) {
        int songID = *it;
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
        if (data) {
            SongRecord record(data);
            if (!DoesSongMatchFilter(songID, filter, partSym)) {
                continue;
            }
            std::pair<Symbol, SongRecord> p(record.mShortName, record);
            mSongs.insert(std::pair<const Symbol, SongRecord>(p));
        }
    }
    unk34.clear();
    if (TheProfileMgr.unk58a && TheSessionMgr->IsLocal()) {
        std::vector<StoreOffer *> offers;
        TheMusicLibrary->GetStoreOffers(offers);
        FOREACH (it, offers) {
            StoreOffer *offer = *it;
            if (DoesOfferMatchFilter(offer, filter, partSym)) {
                unk34.push_back(offer);
            }
        }
    }
}
#pragma pop

bool SongSortMgr::DoesSongMatchFilter(int songID, const SongFilter *filter, Symbol partSym)
    const {
    if (!filter)
        return true;
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
    MILO_ASSERT(data, 0x18B);
    if (std::find(filter->excludedSongs.begin(), filter->excludedSongs.end(), songID)
        != filter->excludedSongs.end()) {
        return false;
    }
    if (filter->requiredTrackType != kTrackNone) {
        if (!data->HasPart(TrackTypeToSym(filter->requiredTrackType)))
            return false;
    }
    bool found = true;
    for (int i = 0; i < kNumFilterTypes; i++) {
        const std::set<Symbol> &curSet = filter->filters[i];
        if (curSet.empty())
            continue;
        switch (i) {
        case 0:
            found = curSet.find(data->Genre()) != curSet.end();
            break;
        case 1:
            found = curSet.find(data->Decade()) != curSet.end();
            break;
        case 9:
            found = curSet.find(Symbol(data->Artist())) != curSet.end();
            break;
        case 6: {
            MILO_ASSERT(partSym != "", 0x1B5);
            if (!data->HasPart(partSym)) {
                found = false;
                break;
            }
            int tier = TheSongMgr.RankTier(data->Rank(partSym), partSym);
            Symbol tierTok = TheSongMgr.RankTierToken(tier);
            found = curSet.find(tierTok) != curSet.end();
            break;
        }
        case 7:
            found = curSet.find(data->RatingSym()) != curSet.end();
            break;
        case 8:
            found = curSet.find(data->VocalPartsSym()) != curSet.end();
            break;
        case 5:
            found = curSet.find(data->SourceSym()) != curSet.end();
            break;
        case 4:
            found = curSet.find(data->LengthSym()) != curSet.end();
            break;
        case 3:
            found = curSet.find(data->HasProGuitarSym()) != curSet.end();
            break;
        case 2:
            found = curSet.find(data->HasKeysSym()) != curSet.end();
            break;
        case 10:
            found = curSet.find(data->HasSoloSym(partSym)) != curSet.end();
            break;
        }
        if (!found)
            break;
    }
    return found;
}

bool SongSortMgr::DoesOfferMatchFilter(
    StoreOffer *offer, const SongFilter *filter, Symbol partSym
) const {
    if (!filter)
        return true;
    int songID = (int)offer->GetSingleSongID();
    const std::vector<int> &excludedSongs = filter->excludedSongs;
    if (std::find(excludedSongs.begin(), excludedSongs.end(), songID)
        != excludedSongs.end()) {
        return false;
    }
    if (filter->requiredTrackType != kTrackNone) {
        if (offer->PartRank(TrackTypeToSym(filter->requiredTrackType)) == 0.0f)
            return false;
    }
    static Symbol artist("artist");
    static Symbol real_guitar("real_guitar");
    static Symbol real_bass("real_bass");
    static Symbol real_keys("real_keys");
    static Symbol keys("keys");
    static Symbol has_part_no("has_part_no");
    static Symbol has_part_yes("has_part_yes");
    bool found = true;
    for (int i = 0; i < kNumFilterTypes; i++) {
        const std::set<Symbol> &curSet = filter->filters[i];
        if (curSet.empty())
            continue;
        switch (i) {
        case 0:
            found = curSet.find(offer->Genre()) != curSet.end();
            break;
        case 1:
            found = curSet.find(offer->Decade()) != curSet.end();
            break;
        case 9:
            found = curSet.find(Symbol(offer->GetData(DataArrayPtr(artist), false).Str(0)))
                != curSet.end();
            break;
        case 6: {
            MILO_ASSERT(partSym != "", 0x219);
            if (offer->PartRank(partSym) == 0.0f) {
                found = false;
                break;
            }
            int tier = TheSongMgr.RankTier(offer->PartRank(partSym), partSym);
            Symbol tierTok = TheSongMgr.RankTierToken(tier);
            found = curSet.find(tierTok) != curSet.end();
            break;
        }
        case 7:
            found = curSet.find(offer->RatingSym()) != curSet.end();
            break;
        case 8:
            found = curSet.find(offer->VocalPartsSym()) != curSet.end();
            break;
        case 5: {
            static Symbol author("author");
            static Symbol ugc("ugc");
            static Symbol dlc("dlc");
            Symbol key = offer->HasData(author) ? ugc : dlc;
            found = curSet.find(key) != curSet.end();
            break;
        }
        case 4:
            found = curSet.find(offer->LengthSym()) != curSet.end();
            break;
        case 3: {
            found = offer->PartRank(real_guitar) == 0.0f
                && offer->PartRank(real_bass) == 0.0f;
            Symbol key = found ? has_part_no : has_part_yes;
            found = curSet.find(key) != curSet.end();
            break;
        }
        case 2: {
            found = offer->PartRank(real_keys) == 0.0f
                && offer->PartRank(keys) == 0.0f;
            Symbol key = found ? has_part_no : has_part_yes;
            found = curSet.find(key) != curSet.end();
            break;
        }
        case 10: {
            Symbol key = offer->HasSolo() ? has_part_yes : has_part_no;
            found = curSet.find(key) != curSet.end();
            break;
        }
        }
        if (!found)
            break;
    }
    static Symbol rating("rating");
    int ratingVal = offer->GetData(DataArrayPtr(rating), false).Int(0);
    return found && AllowedToAccessContent(ratingVal);
}

bool SongSortMgr::GetRandomSongs(
    int count,
    std::vector<Symbol> *randomSongs,
    std::vector<int> *randomSongIDs,
    std::vector<Symbol> *excludedSyms,
    std::vector<Symbol> *availableParts,
    bool b1,
    bool b2
) {
    MILO_ASSERT(!randomSongs || randomSongs->empty(), 0x27F);
    MILO_ASSERT(!randomSongIDs || randomSongIDs->empty(), 0x280);
    MILO_ASSERT(randomSongs || randomSongIDs, 0x281);

    Symbol curName;
    int numAdded = 0;
    std::vector<int> bucket4;
    std::vector<int> bucket3;
    std::vector<int> bucket2;
    std::vector<int> bucket1;
    std::vector<int> bucket0;
    std::vector<int> *songPools[5];
    songPools[0] = &bucket4;
    songPools[1] = &bucket3;
    songPools[2] = &bucket2;
    songPools[3] = &bucket1;
    songPools[4] = &bucket0;
    std::vector<int> excludeList;
    std::vector<int> validSongs;
    if (excludedSyms) {
        FOREACH (it, *excludedSyms) {
            Symbol s = *it;
            int id = TheSongMgr.GetSongIDFromShortName(s, true);
            excludeList.push_back(id);
        }
    }
    TheSongMgr.GetValidSongs(
        excludeList, *TheBandUserMgr, validSongs, -1.0f, -1.0f, b1, b2
    );
    std::map<Symbol, SongRecord>::const_iterator it = mSongs.begin();
    for (; it != mSongs.end(); ++it) {
        curName = it->first;
        const SongRecord &rec = it->second;
        int id = rec.GetData()->ID();
        if (std::find(validSongs.begin(), validSongs.end(), id) == validSongs.end())
            continue;
        if (availableParts) {
            bool partOk = false;
            FOREACH (pit, *availableParts) {
                Symbol sym = *pit;
                if (!rec.GetData()->HasPart(sym)) {
                    partOk = true;
                    break;
                }
            }
            if (partOk)
                continue;
        }
        int reviewIdx = rec.GetReview() - 1;
        if (reviewIdx < 0)
            reviewIdx = 2;
        songPools[reviewIdx]->push_back(id);
        numAdded++;
    }
    if (count == 0) {
        count = numAdded;
    } else if (numAdded < count) {
        MILO_WARN("Not enough valid random songs!");
        return false;
    }
    DataArray *cfg = SystemConfig(Symbol("song_select"), Symbol("review_weights"));
    int weights[5] = { 0, 0, 0, 0, 0 };
    for (int i = 1; i < 5; i++) {
        weights[i] = cfg->FindInt(MakeString("review_%i", i + 1));
    }
    for (int i = 0; i < count; i++) {
        int total = 0;
        for (int j = 1; j < 5; j++) {
            total += songPools[j]->size() * weights[j];
        }
        std::vector<int> *chosen;
        if (total == 0) {
            chosen = songPools[0];
        } else {
            int r = RandomInt(0, total);
            for (int j = 1; j < 5; j++) {
                chosen = songPools[j];
                r -= songPools[j]->size() * weights[j];
                if (r < 0)
                    break;
            }
        }
        MILO_ASSERT(chosen->size(), 0x2F9);
        int idx = RandomInt(0, chosen->size());
        std::vector<int>::iterator it = chosen->begin();
        for (int j = 0; j < idx; j++) ++it;
        if (randomSongs) {
            randomSongs->push_back(TheSongMgr.GetShortNameFromSongID(*it, true));
        }
        if (randomSongIDs) {
            randomSongIDs->push_back(*it);
        }
        chosen->erase(it);
    }
    return true;
}

NodeSort *SongSortMgr::GetSort(SongSortType ty) { return mSorts[ty]; }

SongRecord *SongSortMgr::GetRecord(int songID) {
    if (TheSongMgr.Data(songID)) {
        Symbol theShortname = TheSongMgr.GetShortNameFromSongID(songID, false);
        if (!(theShortname == gNullStr)) {
            std::map<Symbol, SongRecord>::iterator it = mSongs.find(theShortname);
            if (it != mSongs.end()) {
                return &it->second;
            }
        }
    }
    return nullptr;
}

bool SongSortMgr::IsSetlistSort(SongSortType ty) { return ty == kSetlistSortByLocation; }

bool SongSortMgr::IsValidNextSortTransition(
    SongSortType ty1, SongSortType ty2
) {
    if (ty1 == ty2)
        return true;
    else if ((ty1 == kSetlistSortByLocation && ty2 != kSetlistSortByLocation)
             || (ty1 != kSetlistSortByLocation && ty2 == kSetlistSortByLocation))
        return false;
    else
        return true;
}

BinStream &operator<<(BinStream &bs, const SongSortMgr::SongFilter &filt) {
    bs << filt.excludedSongs;
    bs << filt.requiredTrackType;
    for (int i = 0; i < kNumFilterTypes; i++) {
        const std::set<Symbol> &curSet = filt.filters[i];
        bs << curSet.size();
        FOREACH (it, curSet) {
            Symbol cur = *it;
            bs << cur;
        }
    }
    return bs;
}

BinStream &operator>>(BinStream &bs, SongSortMgr::SongFilter &filt) {
    bs >> filt.excludedSongs;
    int t = 0;
    bs >> t;
    filt.requiredTrackType = (TrackType)t;
    for (int i = 0; i < kNumFilterTypes; i++) {
        int curSize = 0;
        bs >> curSize;
        for (int j = 0; j < curSize; j++) {
            Symbol s = gNullStr;
            bs >> s;
            filt.AddFilter((FilterType)i, s);
        }
    }
    return bs;
}