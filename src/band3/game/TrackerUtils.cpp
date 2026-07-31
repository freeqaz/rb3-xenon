#include "game/TrackerUtils.h"
#include "game/GemPlayer.h"
#include "game/SongDB.h"
#include "game/TrackerSource.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/VocalNote.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"

TrackerMultiplierMap::TrackerMultiplierMap() : unk0(-1.0f) {}

TrackerMultiplierMap::~TrackerMultiplierMap() {}

void TrackerMultiplierMap::InitFromDataArray(const DataArray *iDataArray) {
    unk4.clear();
    int idx = 0;
    MultiplierEntry &entry = unk4[0.0f];
    entry.unk0 = 1.0f;
    entry.unk4 = 0;
    if (iDataArray) {
        MILO_ASSERT(!iDataArray->Sym(0).Null(), 0x2B);
        for (int i = 1; i < iDataArray->Size(); i++) {
            DataArray *arr = iDataArray->Array(i);
            float f10 = arr->Float(0);
            float f11 = arr->Float(1);
            idx++;
            MultiplierEntry &curentry = unk4[f10];
            curentry.unk0 = f11;
            curentry.unk4 = idx;
            if (unk0 < f10) {
                unk0 = f10;
            }
        }
    }
}

float TrackerMultiplierMap::GetMultiplier(float f) const { return FindEntry(f).unk0; }

int TrackerMultiplierMap::GetMultiplierIndex(float f) const { return FindEntry(f).unk4; }

float TrackerMultiplierMap::GetPercentOfMaxMultiplier(float f) const {
    return std::min(f / unk0, 1.0f);
}

const TrackerMultiplierMap::MultiplierEntry &TrackerMultiplierMap::FindEntry(float f) const {
    std::map<float, MultiplierEntry>::const_iterator it = unk4.lower_bound(f);
    if (it != unk4.begin())
        --it;
    return it->second;
}

TrackerSectionManager::TrackerSectionManager() {}

TrackerSectionManager::~TrackerSectionManager() {}

void TrackerSectionManager::Init() { GatherSections(); }
int TrackerSectionManager::GetSectionCount() const { return mSections.size(); }

int TrackerSectionManager::CountNonEmptySections(const TrackerSource *source, bool b) const {
    int count = 0;
    for (int i = 0; i < (int)mSections.size(); i++) {
        for (TrackerPlayerID id = source->GetFirstPlayer(); id.NotNull();
             id = source->GetNextPlayer(id)) {
            Player *pPlayer = source->GetPlayer(id);
            MILO_ASSERT(pPlayer, 0x8A);
            if (CountGemsInSection(pPlayer, i) > 0) {
                count++;
                if (!b)
                    break;
            }
        }
    }
    return count;
}

int TrackerSectionManager::GetSectionStartTick(int idx) const {
    return mSections[idx].mStartTick;
}
int TrackerSectionManager::GetSectionEndTick(int idx) const {
    return mSections[idx].mEndTick;
}

int TrackerSectionManager::FindSectionContainingTick(int tick) const {
    int idx;
    for (idx = 0; idx < (int)mSections.size(); idx++) {
        if (mSections[idx].mEndTick > tick)
            break;
    }
    if (idx == (int)mSections.size())
        return -1;
    return idx;
}

bool TrackerSectionManager::TickInSection(int tick, int section) const {
    const Section &sect = mSections[section];
    return tick >= sect.mStartTick && tick <= sect.mEndTick;
}

bool TrackerSectionManager::TickAfterSection(int tick, int section) const {
    return tick > mSections[section].mEndTick;
}

int TrackerSectionManager::CountGemsInSection(const Player *iPlayer, int iSection) const {
    const Section &sect = mSections[iSection];
    int startTick = sect.mStartTick;
    int endTick = sect.mEndTick;
    if (iPlayer->GetTrackType() == kTrackVocals) {
        VocalNoteList *pNoteList = TheSongDB->GetVocalNoteList(0);
        MILO_ASSERT(pNoteList, 224);
        std::vector<VocalPhrase> phrases;
        pNoteList->GetPracticePhrases(phrases, startTick, endTick);
        int count = 0;
        for (std::vector<VocalPhrase>::iterator it = phrases.begin();
             it != phrases.end(); ++it) {
            if (it->unk8 + it->unkc < endTick && it->unk14 > it->unk10) {
                count++;
            }
        }
        return count;
    } else {
        int startGemID;
        int endGemID;
        if (GetGemIDsForRange(iPlayer, startTick, endTick, startGemID, endGemID)) {
            return endGemID - startGemID + 1;
        }
        return 0;
    }
}

void TrackerSectionManager::GetGemStatsInRange(
    const Player *iPlayer, int iStartTick, int iEndTick, int &oNumHit,
    int &oNumMissed
) const {
    if (iPlayer->GetTrackType() == kTrackVocals) {
        oNumMissed = 0;
        oNumHit = 0;
        return;
    }
    int startGemID;
    int endGemID;
    if (!GetGemIDsForRange(iPlayer, iStartTick, iEndTick, startGemID, endGemID)) {
        oNumMissed = 0;
        oNumHit = 0;
        return;
    }
    oNumMissed = 0;
    oNumHit = 0;
    TheSongDB->GetGemList(iPlayer->mTrackNum);
    const GemPlayer *pGemPlayer = dynamic_cast<const GemPlayer *>(iPlayer);
    MILO_ASSERT(pGemPlayer, 285);
    GemStatus *pGemStatus = pGemPlayer->GetGemStatus();
    MILO_ASSERT(pGemStatus, 287);
    for (int i = startGemID; i <= endGemID; i++) {
        if (pGemStatus->GetHit(i)) {
            oNumHit++;
        } else if (pGemStatus->GetIgnored(i)) {
            oNumMissed++;
        }
    }
}

bool TrackerSectionManager::GetGemIDsForRange(
    const Player *iPlayer, int iStartTick, int iEndTick, int &oStartGemID,
    int &oEndGemID
) const {
    MILO_ASSERT(iPlayer->GetTrackType() != kTrackVocals, 309);
    const GameGemList *pGems = TheSongDB->GetGemList(iPlayer->mTrackNum);
    if (pGems->NumGems() == 0) {
        oEndGemID = -1;
        oStartGemID = -1;
        return 0;
    }
    oStartGemID = pGems->ClosestMarkerIdxAtOrAfterTick(iStartTick);
    if (oStartGemID == -1) {
        oEndGemID = -1;
        oStartGemID = -1;
        return 0;
    }
    if (pGems->GetGem(oStartGemID).mTick >= iEndTick) {
        oEndGemID = -1;
        oStartGemID = -1;
        return 0;
    }
    oEndGemID = pGems->ClosestMarkerIdxAtOrAfterTick(iEndTick);
    if (oEndGemID == -1) {
        oEndGemID = pGems->NumGems() - 1;
    }
    while (oEndGemID >= 0 && pGems->GetGem(oEndGemID).mTick >= iEndTick) {
        oEndGemID--;
    }
    if (oEndGemID == -1) {
        oEndGemID = -1;
        oStartGemID = -1;
        return 0;
    }
    MILO_ASSERT(oEndGemID >= oStartGemID, 354);
    return 1;
}

void TrackerSectionManager::GatherSections() {
    SongDB *songDB = TheSongDB;
    mSections.clear();
    mSections.reserve(songDB->mPracticeSections.size());
    std::vector<PracticeSection>::iterator it = songDB->mPracticeSections.begin();
    for (; it != songDB->mPracticeSections.end(); ++it) {
        if (it->unk4 != it->unk8) {
            int mStr = (int)it->unk0;
            Section section;
            section.mStartTick = it->unk4;
            section.mEndTick = it->unk8;
            section.unk8 = mStr;
            mSections.push_back(section);
        }
    }
    if (mSections.size() == 0) {
        MILO_WARN("No practice sections found in song!");
    }
}

int TrackerUtils::CountVocalPhrasesInSong(int iPlayer) {
    VocalNoteList *pPlayer = TheSongDB->GetVocalNoteList(0);
    MILO_ASSERT(pPlayer, 0x193);
    int count = 0;
    for (std::vector<VocalPhrase>::iterator it = pPlayer->mPhrases.begin();
         it != pPlayer->mPhrases.end(); ++it) {
        if (it->unk14 > it->unk10 && !it->mTambourinePhrase) {
            count++;
        }
    }
    return count;
}

int TrackerUtils::CountGemsInSong(int iPlayer, TrackType iTrackType) {
    MILO_ASSERT(kTrackVocals != iTrackType, 0x1AC);
    int count = TheSongDB->GetTotalGems(iPlayer);
    if (iTrackType != kTrackDrum) {
        return count;
    }
    DrumFillInfo *pFillInfo = TheSongDB->GetDrumFillInfo(iPlayer);
    const GameGemList *pGemList = TheSongDB->GetGemList(iPlayer);
    std::vector<FillExtent>::iterator fillIt = pFillInfo->mFills.begin();
    std::vector<FillExtent>::iterator fillEnd = pFillInfo->mFills.end();
    std::vector<GameGem>::const_iterator gemIt = pGemList->mGems.begin();
    for (; fillIt != fillEnd; ++fillIt) {
        while (gemIt != pGemList->mGems.end() && gemIt->mTick < fillIt->start) {
            ++gemIt;
        }
        while (gemIt != pGemList->mGems.end() && gemIt->mTick <= fillIt->end) {
            count--;
            ++gemIt;
        }
    }
    return count;
}

float TrackerUtils::GetNextNoteMs(int iPlayer, TrackType iTrackType, float f) {
    float nextMs = TheSongDB->GetSongDurationMs();
    if (iTrackType == kTrackVocals) {
        for (int i = 0; i < TheSongDB->GetVocalNoteListCount(); i++) {
            VocalNote *pNote = TheSongDB->GetVocalNoteList(i)->NextNote(f);
            if (pNote) {
                float ms = pNote->mMs;
                if (ms < nextMs) {
                    nextMs = ms;
                }
            }
        }
    } else {
        const GameGemList *pGemList = TheSongDB->GetGemList(iPlayer);
        int idx = pGemList->ClosestMarkerIdxAtOrAfter(f);
        if (idx != -1) {
            nextMs = pGemList->GetGem(idx).mMs;
        }
    }
    return nextMs;
}
