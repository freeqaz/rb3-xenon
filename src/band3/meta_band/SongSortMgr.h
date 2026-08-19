#pragma once
#include "SongSortByRecent.h"
#include "SongSortByReview.h"
#include "beatmatch/TrackType.h"
#include "meta/StoreOffer.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/SetlistSortByLocation.h"
#include "meta_band/SongRecord.h"
#include "meta_band/SongSortByArtist.h"
#include "meta_band/SongSortByDiff.h"
#include "meta_band/SongSortByPlays.h"
#include "meta_band/SongSortByRank.h"
#include "meta_band/SongSortBySong.h"
#include "meta_band/SongSortByStars.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"
#include <vector>
#include <set>

// NB(rb3-xenon, lane INSDEL-5): these values are READ OFF RETAIL, and they
// answer the two questions this comment used to ask -- yes, 2 is has-keys and
// 3 is has-pro-guitar.  Three independent sources agree:
//   1. retail's FilterTypeToSym dispatch chain (mtctr + bdzf, slot N == ft N);
//   2. ViewSettingsProvider::BuildFilters, which sits at mpn 100.0000 and
//      indexes filterSyms[] with RAW INTEGER LITERALS -- a matching row that
//      is completely independent of this enum, i.e. a control;
//   3. MusicLibrary::SetupTaskForTrainer, where a kControllerRealGuitar case
//      passed 3 and a kControllerKeys case passed 2.  The numbers were always
//      right; only the NAMES were wrong.
// The old numbering mislabelled every row of the filter view-settings menu.
enum FilterType {
    kFilterGenre = 0,
    kFilterDecade = 1,
    kFilterKeys = 2,
    kFilterProGuitar = 3,
    kFilterVocalParts = 4,
    kFilterSource = 5,
    kFilterDifficulty = 6,
    kFilterLength = 7,
    kFilterRating = 8,
    // 9 and 10 are still unidentified (kNumFilterTypes is 0xB in retail)
    kNumFilterTypes = 0xB
};

enum SongSortType {
    kSongSortBySong = 0,
    kSongSortByArtist = 1,
    kSongSortByDiff = 2,
    kSongSortByStars = 3,
    kSongSortByRank = 4,
    kSongSortByRecent = 5,
    kSongSortByPlays = 6,
    kSongSortByReview = 7,
    kSetlistSortByLocation = 8,
    kNumSongSortTypes = 9
};

class SongSortMgr {
public:
    class SongFilter {
    public:
        SongFilter() : requiredTrackType(kTrackNone) { filters.resize(kNumFilterTypes); }
        ~SongFilter() {}

        SongFilter &operator=(const SongFilter &rhs) {
            filters = rhs.filters;
            requiredTrackType = rhs.requiredTrackType;
            excludedSongs = rhs.excludedSongs;
            return *this;
        }

        void ClearFilter(int idx) { filters[idx].clear(); }

        void Reset() {
            for (int i = 0; i < 11; i++)
                ClearFilter(i);
            requiredTrackType = kTrackNone;
            excludedSongs.clear();
        }

        void IntersectFilter(SongFilter *);

        bool HasFilter(FilterType type, Symbol s) const {
            MILO_ASSERT_RANGE(type, 0, kNumFilterTypes, 0x56);
            return filters[type].find(s) != filters[type].end();
        }

        bool HasFilterType(FilterType type) const {
            MILO_ASSERT_RANGE(type, 0, kNumFilterTypes, 0x5A);
            return filters[type].size() > 0;
        }

        void AddFilter(FilterType type, Symbol s) {
            MILO_ASSERT_RANGE(type, 0, kNumFilterTypes, 0x5E);
            filters[type].insert(s);
        }

        void RemoveFilter(FilterType type, Symbol s) {
            MILO_ASSERT_RANGE(type, 0, kNumFilterTypes, 0x62);
            filters[type].erase(filters[type].find(s));
        }

        const std::set<Symbol> &GetFilterSet(FilterType type) const {
            MILO_ASSERT_RANGE(type, 0, kNumFilterTypes, 0x66);
            return filters[type];
        }

        std::vector<std::set<Symbol> > filters; // 0x0
        TrackType requiredTrackType; // 0xc
        std::vector<int> excludedSongs; // 0x10
    };

    SongSortMgr();
    virtual ~SongSortMgr();

    bool DoesSongMatchFilter(int, const SongFilter *, Symbol) const;
    bool DoesOfferMatchFilter(StoreOffer *, const SongFilter *, Symbol) const;
    void BuildSortTree(SongSortType);
    void BuildSortList(SongSortType);
    void ClearAllSorts();
    bool InqSongsForSetlist(Symbol, std::vector<Symbol> &);
    void BuildSetlistList();
    void BuildInternalSetlists();
    void BuildFilteredSongList(SongFilter *, Symbol);
    NodeSort *GetSort(SongSortType);
    SongRecord *GetRecord(int);
    bool IsValidNextSortTransition(SongSortType, SongSortType);
    void ClearInternalSetlists();
    bool GetRandomSongs(
        int,
        std::vector<Symbol> *,
        std::vector<int> *,
        std::vector<Symbol> *,
        std::vector<Symbol> *,
        bool,
        bool
    );

    static bool IsSetlistSort(SongSortType);
    static void Init();

    std::map<Symbol, SongRecord> mSongs; // 0x4
    std::map<Symbol, SetlistRecord> mSetlists; // 0x1c
    std::vector<StoreOffer *> unk34; // 0x34
    std::vector<SavedSetlist *> mInternalSetlists; // 0x40
    NodeSort *mSorts[kNumSongSortTypes]; // 0x4c
};

BinStream &operator<<(BinStream &, const SongSortMgr::SongFilter &);
BinStream &operator>>(BinStream &, SongSortMgr::SongFilter &);

extern SongSortMgr *TheSongSortMgr;
