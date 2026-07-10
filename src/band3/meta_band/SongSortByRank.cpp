#include "meta_band/SongSortByRank.h"
#include "meta/Sorting.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/SongRecord.h"
#include "net_band/RockCentral.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbols4.h"

RankCmp::RankCmp(int val, const char *name, RankCmp::RankType ty)
    : mVal(val), mSongName(name), mType(ty) {}



int RankCmp::Compare(const SongSortCmp *s, SongNodeType nodeType) const {
    RankCmp *cmp = (RankCmp *)s;
    switch(nodeType) {
        case kNodeShortcut:
        case kNodeHeader:
            if (mType != cmp->mType) {
                return mType - cmp->mType;
            }
            switch (mType) {
                case kRank:
                    return 0;
                case kPercentile:
                    return (cmp->mVal - cmp->mVal % 10) - (mVal - mVal % 10);
                default:
                    MILO_ASSERT(mVal == cmp->mVal, 0x38);
                    return 0;
            }
        case kNodeSong:
        case kNodeStoreSong:
            if (mType != cmp->mType) {
                return mType - cmp->mType;
            }
            switch (mType) {
                case kRank:
                    if (mVal != cmp->mVal) {
                        return mVal - cmp->mVal;
                    }
                    return AlphaKeyStrCmp(mSongName, cmp->mSongName, true);
                case kPercentile:
                    if (mVal != cmp->mVal) {
                        return cmp->mVal - mVal;
                    }
                    return AlphaKeyStrCmp(mSongName, cmp->mSongName, true);
                default:
                    return AlphaKeyStrCmp(mSongName, cmp->mSongName, true);
            }
        default:
            MILO_FAIL("invalid type of node comparison.\n");
            return 0;
    }
}

void SongSortByRank::Clear() {
    mRankings.clear();
    mDataResults.Clear();
}

bool SongSortByRank::IsReady() const {
    return !mRankings.empty();
}

void SongSortByRank::MakeReady() {
    RequestSongRankingInfo();
}

void SongSortByRank::CancelMakeReady() {
    CancelSongRankingRequest();
    mRankings.clear();
    mDataResults.Clear();
}

void SongSortByRank::RequestSongRankingInfo() {
    Profile *prof = TheProfileMgr.GetPrimaryProfile();
    if(prof) {
        ScoreType sType = TheMusicLibrary->ActiveScoreType();
#ifdef HX_NATIVE
        // Host STL has no stlpmtx_std namespace; the matched fork uses STLport's.
        std::vector<int> something;
#else
        stlpmtx_std::vector<int> something;
#endif
        TheSongMgr.GetRankedSongs(something, true, true);
        TheRockCentral.GetMultipleRankingsForPlayer(prof, sType, something, mDataResults, this);
    }
}

void SongSortByRank::CancelSongRankingRequest() {
    TheRockCentral.CancelOutstandingCalls(this);
}

OwnedSongSortNode *SongSortByRank::NewSongNode(SongRecord *record) const {
    MemDoTempAllocations m;
    MILO_ASSERT(!mRankings.empty(), 0x80);
    const BandSongMetadata *data = record->mData;
    int songId = data->ID();
    std::hash_map<int, std::pair<int, bool> >::const_iterator it = mRankings.find(songId);
    RankCmp *pCmp;
    if(it == mRankings.end()) {
        pCmp = new RankCmp(-1, data->Title(), RankCmp::kNoData);
    } else if(it->second.first == 0) {
        pCmp = new RankCmp(0, data->Title(), RankCmp::kUnranked);
    } else if(it->second.second) {
        pCmp = new RankCmp(it->second.first, data->Title(), RankCmp::kPercentile);
    } else {
        pCmp = new RankCmp(it->second.first, data->Title(), RankCmp::kRank);
    }
    MILO_ASSERT(pCmp, 0x9F);
    OwnedSongSortNode *node = new OwnedSongSortNode(pCmp, record);
    return node;
}

StoreSongSortNode *SongSortByRank::NewSongNode(StoreOffer *offer) const {
    MemDoTempAllocations m;
    MILO_ASSERT(!mRankings.empty(), 0xA9);
    RankCmp *cmp = new RankCmp(-1, offer->OfferName(), RankCmp::kNoData);
    StoreSongSortNode *node = new StoreSongSortNode(cmp, offer);
    return node;
}

ShortcutNode *SongSortByRank::NewShortcutNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    RankCmp *other = (RankCmp *)node->Cmp();
    RankCmp::RankType type = other->mType;
    static Symbol rank_ranked("rank_ranked");
    static Symbol rank_unranked("rank_unranked");
    static Symbol rank_nodata("rank_nodata");
    static Symbol percentile_fmt("percentile_fmt");
    int val;
    const char *label = gNullStr;
    switch(type) {
    case RankCmp::kRank:
        val = 1;
        label = Localize(rank_ranked, NULL);
        break;
    case RankCmp::kPercentile:
        val = other->mVal - other->mVal % 10;
        label = MakeString(Localize(percentile_fmt, NULL), 100 - val);
        break;
    case RankCmp::kUnranked:
        val = 0;
        label = Localize(rank_unranked, NULL);
        break;
    case RankCmp::kNoData:
        val = -1;
        label = Localize(rank_nodata, NULL);
        break;
    default:
        MILO_FAIL("Bad RankType in SongSortByRank::NewShortcutNode!");
        val = 0;
        break;
    }
    RankCmp *cmp = new RankCmp(val, NULL, type);
    ShortcutNode *newNode = new ShortcutNode(cmp, label, false);
    return newNode;
}

HeaderSortNode *SongSortByRank::NewHeaderNode(SongSortNode *node) const {
    MemDoTempAllocations m;
    RankCmp *other = (RankCmp *)node->Cmp();
    RankCmp::RankType type = other->mType;
    static Symbol rank_ranked("rank_ranked");
    static Symbol rank_unranked("rank_unranked");
    static Symbol rank_nodata("rank_nodata");
    static Symbol percentile_fmt("percentile_fmt");
    int val;
    const char *label = gNullStr;
    switch(type) {
    case RankCmp::kRank:
        val = 1;
        label = Localize(rank_ranked, NULL);
        break;
    case RankCmp::kPercentile:
        val = other->mVal - other->mVal % 10;
        label = MakeString(Localize(percentile_fmt, NULL), 100 - val);
        break;
    case RankCmp::kUnranked:
        val = 0;
        label = Localize(rank_unranked, NULL);
        break;
    case RankCmp::kNoData:
        val = -1;
        label = Localize(rank_nodata, NULL);
        break;
    default:
        MILO_FAIL("Bad RankType in SongSortByRank::NewShortcutNode!");
        val = 0;
        break;
    }
    RankCmp *cmp = new RankCmp(val, NULL, type);
    HeaderSortNode *newNode = new HeaderSortNode(cmp, label, false);
    return newNode;
}

DataNode SongSortByRank::OnMsg(RockCentralOpCompleteMsg const &msg) {
    if(msg.Success()) {
        mDataResults.Update(NULL);
        MILO_ASSERT(mRankings.empty(), 0x11F);
        int songId, rank;
        bool isPercentile;
        for(int i = 0; i < mDataResults.NumDataResults(); i++) {
            DataNode node;
            DataResult *res = mDataResults.GetDataResult(i);
            res->GetDataResultValue("song_id", node);
            songId = node.Int(NULL);
            res->GetDataResultValue("rank", node);
            rank = node.Int(NULL);
            res->GetDataResultValue("is_percentile", node);
            isPercentile = node.Int(NULL) != 0;
            mRankings[songId] = std::make_pair(rank, isPercentile);
        }
        mDataResults.Clear();
    }
    return DataNode(1);
}

BEGIN_HANDLERS(SongSortByRank)
HANDLE_MESSAGE(RockCentralOpCompleteMsg)
HANDLE_SUPERCLASS(SongSort)
HANDLE_CHECK(0x13E)
END_HANDLERS
