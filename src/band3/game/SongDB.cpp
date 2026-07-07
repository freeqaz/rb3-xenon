#include "game/SongDB.h"
#include "beatmatch/DrumMap.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/InternalSongParserSink.h"
#include "beatmatch/MasterAudio.h"
#include "beatmatch/Phrase.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/TuningOffsetList.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/PracticeSectionProvider.h"
#include "game/TrainerPanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MetaPerformer.h"
#include "midi/DataEvent.h"
#include "midi/MidiParserMgr.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/TimeConversion.h"

bool ExtentCmp(const Extent &e, int n) { return e.unk4 < n; }

// Force the complete out-of-line vector<VocalNote>/<VocalPhrase> destructors.
// `delete ptr` gets inlined/DCE'd here (-inline noauto); a stack-local vector
// with push_back references the non-inline stlport ~vector() via a real `bl`.
DECOMP_FORCEBLOCK(
    SongDB, (const VocalNote &vn, const VocalPhrase &vp),
    std::vector<VocalNote> notes;
    notes.push_back(vn);
    std::vector<VocalPhrase> phrases;
    phrases.push_back(vp);
)

SongDB::SongDB()
    : mSongData(new SongData()), mSongDurationMs(0), mCodaStartTick(-1),
      mMultiplayerAnalyzer(new MultiplayerAnalyzer(mSongData)), unk24(-1), unk28(-1) {
    mSongData->AddSink(this);
}

SongDB::~SongDB() {
    RELEASE(mSongData);
    RELEASE(mMultiplayerAnalyzer);
}

void SongDB::PostLoad(DataEventList *list) {
    ParseEvents(list);
    SpewAllVocalNotes();
    SpewTrackSizes();
    SetupPhrases();
    DisableCodaGems();
    RunMultiplayerAnalyzer();
    SetupPracticeSections();
}

void SongDB::RunMultiplayerAnalyzer() {
    PlayerTrackConfigList *pList = TheGameConfig->GetConfigList();
    mMultiplayerAnalyzer->Configure(pList);
    for (int i = 0; i < mTrackData.size(); i++) {
        mMultiplayerAnalyzer->AddTrack(i, mTrackData[i].mTrackType);
        if (pList->TrackUsed(i)) {
            const std::vector<GameGem> &gems = GetGems(i);
            for (int j = 0; j < gems.size(); j++) {
                mMultiplayerAnalyzer->AddGem(i, gems[j]);
            }
            Player *p = TheGame->GetPlayerFromTrack(i, false);
            if (p && p->NeedsToOverrideBasePoints()) {
                TrackData &data = mTrackData[i];
                OverrideBasePoints(
                    i,
                    data.mTrackType,
                    p->GetUserGuid(),
                    p->GetBaseMaxPoints(),
                    p->GetBaseMaxStreakPoints(),
                    p->GetBaseBonusPoints()
                );
            }
        }
    }
    mMultiplayerAnalyzer->PostLoad();
}

void SongDB::RebuildPhrases(int i) {
    ClearTrackPhrases(i);
    mSongData->SendPhrases(i);
}

void SongDB::RebuildData() {
    SetupPhrases();
    RunMultiplayerAnalyzer();
}

void SongDB::OverrideBasePoints(
    int i,
    TrackType ty,
    const UserGuid &guid,
    int baseMaxPts,
    int baseMaxStreakPts,
    int baseBonusPts
) {
    mMultiplayerAnalyzer->OverrideBasePoints(
        i, ty, guid, baseMaxPts, baseMaxStreakPts, baseBonusPts
    );
}

int SongDB::TotalBasePoints() { return mMultiplayerAnalyzer->TotalBasePoints(); }
std::vector<PlayerScoreInfo> &SongDB::GetBaseScores() {
    return mMultiplayerAnalyzer->mBaseScores;
}
float SongDB::GetSongDurationMs() const { return mSongDurationMs; }
int SongDB::GetCodaStartTick() const { return mCodaStartTick; }

FORCE_LOCAL_INLINE
bool SongDB::IsInCoda(int tick) const {
    return mCodaStartTick != -1 && tick >= mCodaStartTick;
}
END_FORCE_LOCAL_INLINE

int SongDB::GetNumTracks() const { return mSongData->GetNumTracks(); }
int SongDB::GetNumTrackData() const { return mTrackData.size(); }

int SongDB::GetBaseMaxPoints(const UserGuid &u) const {
    return mMultiplayerAnalyzer->GetMaxPoints(u);
}

int SongDB::GetBaseMaxStreakPoints(const UserGuid &u) const {
    return mMultiplayerAnalyzer->GetMaxStreakPoints(u);
}

int SongDB::GetBaseBonusPoints(const UserGuid &u) const {
    return mMultiplayerAnalyzer->GetBonusPoints(u);
}

const GameGemList *SongDB::GetGemList(int i) const { return mSongData->GetGemList(i); }

GameGemList *SongDB::GetGemListByDiff(int i, int j) const {
    return mSongData->GetGemListByDiff(i, j);
}

FORCE_LOCAL_INLINE
const std::vector<GameGem> &SongDB::GetGems(int i) const {
    return mSongData->GetGemList(i)->mGems;
}
END_FORCE_LOCAL_INLINE

const GameGem &SongDB::GetGem(int i1, int i2) const { return GetGems(i1)[i2]; }
int SongDB::GetTotalGems(int idx) const { return GetGems(idx).size(); }

DrumFillInfo *SongDB::GetDrumFillInfo(int idx) const {
    return mSongData->GetDrumFillInfo(idx);
}

VocalNoteList *SongDB::GetVocalNoteList(int idx) const {
    return mSongData->GetVocalNoteList(idx);
}

int SongDB::GetVocalNoteListCount() const { return mSongData->GetVocalNoteListCount(); }

bool SongDB::GetPhraseExtents(
    BeatmatchPhraseType ty, int track, int i3, int &i4, int &i5
) {
    MILO_ASSERT(mTrackData.size() > track, 0xF1);
    if (ty == kNoPhrase)
        return false;
    else {
        int i1 = i3;
        int i9 = 0;
        int i8 = 0;
        if (TheGame->InTrainer() && TheTrainerPanel) {
            i9 = TheTrainerPanel->GetCurrentStartTick();
            i8 = TheTrainerPanel->GetCurrentEndTick();
            if (i3 >= i8 && i8 != 0) {
                i3 -= ((i3 - i9) / (i8 - i9)) * (i8 - i9);
            }
        }
        std::vector<Extent> &extents = GetExtents(track, ty);
        std::vector<Extent>::iterator it =
            std::lower_bound(extents.begin(), extents.end(), i3, ExtentCmp);
        if (it == extents.end())
            return false;
        else {
            if (TheGame->InTrainer() && TheTrainerPanel
                && it->unk0 < TheTrainerPanel->GetCurrentStartTick()) {
                return false;
            } else {
                i4 = it->unk0;
                i5 = it->unk4;
                if (i1 >= i8 && i8 != 0) {
                    int num = ((i1 - i9) / (i8 - i9)) * (i8 - i9);
                    i4 += num;
                    i5 += num;
                }
                return i4 <= i1 && i1 < i5;
            }
        }
    }
}

bool SongDB::GetCommonPhraseExtent(int i1, int i2, Extent &ext) {
    if (!TheGame->AllowOverdrivePhrases())
        return false;
    else if (mTrackData[i1].unk3c.size() <= i2) {
        return false;
    } else {
        int idx = mTrackData[i1].unk3c[i2];
        if (idx == -1)
            return false;
        else {
            ext = mTrackData[i1].mCommonPhraseExtents[idx];
            return true;
        }
    }
}

bool SongDB::IsInPhrase(BeatmatchPhraseType ty, int i2, int i3) const {
    const std::vector<unsigned char> &states = mTrackData[i2].GetGemStates(ty);
    return states.size() > (unsigned)i3 && (states[i3] & 2);
}

bool SongDB::IsUnisonPhrase(int i) const {
    return mSongData->GetPhraseAnalyzer()->IsUnisonPhrase(i);
}

int SongDB::GetNumOverdrivePhrases(int i) const {
    return mSongData->GetPhraseAnalyzer()->NumPhrases(i);
}

int SongDB::GetNumUnisonPhrases(int x) const {
    PhraseAnalyzer *pa = mSongData->GetPhraseAnalyzer();
    int num = 0;
    int phraseCount = pa->NumPhrases();
    int tracks = TheGame->GetScoringTracks();
    int mask = 1 << x;
    for (int i = 0; i < phraseCount; i++) {
        int phraseTracks = pa->GetPhraseTracks(i);
        if (phraseTracks & mask && (tracks & phraseTracks - mask)) {
            num++;
        }
    }
    return num;
}

void SongDB::DisableCodaGems() {
    if (mCodaStartTick != -1) {
        for (int i = 0; i < mSongData->GetNumTracks(); i++) {
            GameGemList *gems = mSongData->GetGemList(i);
            std::vector<FillExtent> &extents = mSongData->GetDrumFillInfo(i)->mFills;
            if (!extents.empty() && extents.back().start >= mCodaStartTick) {
                int i2 = extents.back().end;
                int markerIdx = gems->ClosestMarkerIdxAtOrAfterTick(extents.back().start);
                if (markerIdx != -1) {
                    for (; markerIdx < gems->NumGems()
                         && gems->GetGem(markerIdx).GetTick() <= i2;
                         markerIdx++) {
                        gems->GetGem(markerIdx).unk18 = 0x80;
                    }
                }
            }
        }
    }
}

int SongDB::GetPhraseID(int track_num, int i2) const {
    MILO_ASSERT(track_num < mTrackData.size(), 0x197);
    if (unk24 < 0) {
        return mTrackData[track_num].unk34[i2];
    } else {
        int num = (unk28 - unk24) + 1;
        i2 %= num;
        return mTrackData[track_num].unk34[i2 + unk24];
    }
}

int SongDB::GetNumPhraseIDs(int track_num) const {
    MILO_ASSERT(track_num < mTrackData.size(), 0x1A7);
    return mTrackData[track_num].unk34.size();
}

int SongDB::NumCommonPhrases() const {
    return mSongData->GetPhraseAnalyzer()->NumPhrases();
}

int SongDB::GetCommonPhraseTracks(int i) const {
    PhraseAnalyzer *a = mSongData->GetPhraseAnalyzer();
    return a->GetPhraseTracks(i) & TheGame->GetScoringTracks();
}

int SongDB::GetCommonPhraseID(int track, int tick) const {
    if (!TheGame->AllowOverdrivePhrases())
        return -1;
    Player *p = TheGame->GetPlayerFromTrack(track, false);
    if (p && p->GetQuarantined())
        return -1;

    PhraseAnalyzer *a = mSongData->GetPhraseAnalyzer();
    const std::vector<RawPhrase> &phrases = a->GetRawPhrases();
    FOREACH (it, phrases) {
        if (it->track == track && it->end_tick > tick) {
            if (it->start_tick <= tick) {
                int ret = it->id;
                int trackBits = a->GetPhraseTracks(ret);
                MILO_ASSERT((trackBits & (1<<track)) == (1<<track), 0x1D1);
                return ret;
            } else
                return -1;
        }
    }
    return -1;
}

DECOMP_FORCEACTIVE(SongDB, "drum_map", "0 <= slot && slot <= 1")

FillInfo *SongDB::GetFillInfo(int i1, int i2) {
    if (IsInCoda(i2)) {
        return mSongData->GetDrumFillInfo(i1);
    } else
        return mSongData->GetFillInfo(i1);
}

MBT SongDB::GetMBT(int tick) const {
    MBT mbt;
    mSongData->GetMeasureMap()->TickToMeasureBeatTick(
        tick, mbt.measure, mbt.beat, mbt.tick
    );
    return mbt;
}

int SongDB::GetBeatsPerMeasure(int i) const {
    int m, b, t, x;
    mSongData->GetMeasureMap()->TickToMeasureBeatTick(i, m, b, t, x);
    return x;
}

void SongDB::SetNumTracks(int num) {
    mTrackData.clear();
    mTrackData.reserve(num);
}

#pragma push
#pragma dont_inline on
void SongDB::AddTrack(int, Symbol, SongInfoAudioType, TrackType ty, bool) {
    mTrackData.push_back(ty);
}
#pragma pop

void SongDB::AddPhrase(BeatmatchPhraseType ty, int i2, const Phrase &phrase) {
    if (ty != kCommonPhrase || TheGame->AllowOverdrivePhrases()) {
        int startTick = phrase.GetTick();
        int endTick = phrase.GetDurationTicks() + startTick;
        TrackData &data = mTrackData[i2];
        if (ty == kCommonPhrase) {
            MetaPerformer::Current();
            if (TheGameConfig->GetConfigList()->TrackUsed(i2)) {
                data.mCommonPhraseExtents.push_back(Extent(startTick, endTick));
            }
        } else if (ty == kSoloPhrase) {
            data.mSoloPhraseExtents.push_back(Extent(startTick, endTick));
        } else if (ty == kArpeggioPhrase) {
            data.mArpeggioPhraseExtents.push_back(Extent(startTick, endTick));
        } else if (ty == kChordMarkupPhrase) {
            std::vector<Extent> &exts = data.mChordMarkupPhraseExtents;
            exts.push_back(
                Extent(phrase.GetTick(), phrase.GetTick() + phrase.GetDurationTicks())
            );
        }
    }
}

void SongDB::SpewAllVocalNotes() const {}
void SongDB::SpewTrackSizes() const {}

void SongDB::ParseEvents(DataEventList *events) {
    for (int i = 0; i < events->Size(); i++) {
        const DataEvent &curEvent = events->Event(i);
        Symbol sym = curEvent.Msg()->Sym(1);
        if (sym == end) {
            if (mSongDurationMs != 0) {
                MILO_FAIL("Duplicate end text event");
            }
            float f5 = BeatToMs(curEvent.start);
            float f6 = TheGameConfig->GetSongLimitMs();
            if (f5 < f6) {
                mSongDurationMs = f5;
            } else
                mSongDurationMs = f6;
            return;
        } else if (sym == coda) {
            mCodaStartTick = BeatToTickInt(curEvent.start);
        }
    }
}

void SongDB::SetupPhrases() {
    for (int i = 0; i < mSongData->GetNumTracks(); i++) {
        SetupTrackPhrases(i);
    }
}

void SongDB::SetupTrackPhrases(int i1) {
    SetupSoloPhrasesForTrack(i1);
    SetupCommonPhrasesForTrack(i1);
}

void SongDB::ClearQuarantinedPhrases(int i1) {
    const std::vector<GameGem> &gems = GetGems(i1);
    TrackData &data = mTrackData[i1];
    data.mSoloPhraseExtents.clear();
    data.unk24.clear();
    data.unk24.reserve(gems.size());
    data.unk24.assign(gems.size(), 0);
    data.mCommonPhraseExtents.clear();
    data.unk2c.clear();
    data.unk2c.reserve(gems.size());
    data.unk2c.assign(gems.size(), 0);
    data.unk34.clear();
    data.unk34.reserve(gems.size());
    data.unk34.assign(gems.size(), -1);
    PhraseAnalyzer *a = mSongData->GetPhraseAnalyzer();
    data.unk3c.clear();
    data.unk3c.reserve(a->NumPhrases());
    data.unk3c.assign(a->NumPhrases(), -1);
}

void SongDB::ClearTrackPhrases(int i1) {
    ClearQuarantinedPhrases(i1);
    TrackData &data = mTrackData[i1];
    data.mArpeggioPhraseExtents.clear();
    data.mChordMarkupPhraseExtents.clear();
}

void SongDB::SetupCommonPhrasesForTrack(int i1) {
    const std::vector<GameGem> &gems = GetGems(i1);
    if (gems.size() != 0) {
        std::vector<int> &data34 = mTrackData[i1].unk34;
        data34.clear();
        data34.reserve(gems.size());

        PhraseAnalyzer *a = mSongData->GetPhraseAnalyzer();
        std::vector<int> &data3c = mTrackData[i1].unk3c;
        data3c.clear();
        data3c.reserve(a->NumPhrases());

        const std::vector<RawPhrase> &rawPhrases = a->GetRawPhrases();
        int i7 = NextPhraseIndexAfter(i1, -1);
        int i11 = 0;
        for (int i2 = 0; i7 != -1; i7 = NextPhraseIndexAfter(i1, i7)) {
            const RawPhrase &curRawPhrase = rawPhrases[i7];
            int phraseID = curRawPhrase.id;
            while (data3c.size() < phraseID) {
                data3c.push_back(-1);
            }
            data3c.push_back(i2++);

            while (gems[i11].GetTick() < curRawPhrase.start_tick) {
                data34.push_back(-1);
                i11++;
            }

            while (gems[i11].GetTick() < curRawPhrase.end_tick) {
                data34.push_back(phraseID);
                i11++;
            }
        }

        for (; i11 < gems.size(); i11++) {
            data34.push_back(-1);
        }
    }
}

void SongDB::SetupSoloPhrasesForTrack(int i1) {
    SetupPhrasesForTrack(i1, mTrackData[i1].mSoloPhraseExtents, mTrackData[i1].unk24);
}

int SongDB::GetSoloGemCount(int track) const {
    int count = 0;
    const std::vector<unsigned char> &states = mTrackData[track].GetGemStates(kSoloPhrase);
    for (std::vector<unsigned char>::const_iterator it = states.begin(); it != states.end(); ++it) {
        if (*it != 0)
            count++;
    }
    return count;
}

int SongDB::GetSustainGemCount(int track) const {
    int count = 0;
    std::vector<GameGem> &gems = mSongData->GetGemList(track)->mGems;
    for (std::vector<GameGem>::iterator it = gems.begin(); it != gems.end(); ++it) {
        if (!it->mIgnoreDuration)
            count++;
    }
    return count;
}

void SongDB::SetupPhrasesForTrack(
    int trackNum, std::vector<Extent> &extents, std::vector<unsigned char> &gemStates
) {
    if (extents.empty())
        return;
    GameGemList *gemList = mSongData->GetGemList(trackNum);
    int phraseIdx = 0;
    int phraseOffset = 0;
    gemStates.clear();
    gemStates.reserve(gemList->mGems.size());
    for (int gemIdx = 0; (unsigned)gemIdx < (unsigned)gemList->NumGems(); gemIdx++) {
        int gemTick = gemList->mGems[gemIdx].mTick;
        unsigned char state = 0;
        if (phraseIdx < extents.size()) {
            if (gemTick >= extents[phraseIdx].unk0) {
                // goto skips the 0x1 bit set while still entering the 0x2 set; rewrite with merged conds shifts branch polarity.
                if (gemIdx != 0) {
                    bool prevInPhrase = gemStates.size() > (unsigned)(gemIdx - 1)
                        && (gemStates[gemIdx - 1] & 0x2);
                    if (prevInPhrase) {
                        bool prevEndOfPhrase = gemStates.size() > (unsigned)(gemIdx - 1)
                            && (gemStates[gemIdx - 1] & 0x4);
                        if (!prevEndOfPhrase) goto skip_start_bit;
                    }
                }
                state |= 0x1;
            skip_start_bit:
                state |= 0x2;
            }
            if ((unsigned)gemIdx == (unsigned)(gemList->NumGems() - 1)
                || gemList->mGems[gemIdx + 1].mTick >= extents[phraseIdx].unk4) {
                state = (unsigned char)(state | 0x4);
                phraseOffset += sizeof(Extent);
                phraseIdx++;
            }
        }
        gemStates.push_back(state);
    }
    MILO_ASSERT(gemStates.size() == gemList->NumGems(), 0x37d);
}

const std::vector<unsigned char> &SongDB::TrackData::GetGemStates(BeatmatchPhraseType ty) const {
    switch (ty) {
    case kSoloPhrase:
        return unk24;
    case kCommonPhrase:
        return unk2c;
    default:
        MILO_FAIL("GetGemStates called with bad type %d", ty);
        return unk24;
    }
}

std::vector<Extent> &SongDB::GetExtents(int i1, BeatmatchPhraseType ty) {
    switch (ty) {
    case kCommonPhrase:
        return mTrackData[i1].mCommonPhraseExtents;
    case kSoloPhrase:
        return mTrackData[i1].mSoloPhraseExtents;
    case kArpeggioPhrase:
        return mTrackData[i1].mArpeggioPhraseExtents;
    case kChordMarkupPhrase:
        return mTrackData[i1].mChordMarkupPhraseExtents;
    default:
        return mTrackData[i1].mCommonPhraseExtents;
    }
}

int SongDB::NextPhraseIndexAfter(int i1, int i2) {
    const std::vector<RawPhrase> &rawPhrases =
        mSongData->GetPhraseAnalyzer()->GetRawPhrases();
    for (int i = i2 + 1; i < rawPhrases.size(); i++) {
        if (i1 == rawPhrases[i].track)
            return i;
    }
    return -1;
}

void SongDB::SetupPracticeSections() {
    DataEventList *events = TheGame->GetBeatMaster()->GetMidiParserMgr()->GetEventsList();
    for (int i = 0; i < events->Size(); i++) {
        const DataEvent &curEvent = events->Event(i);
        DataArray *msg = curEvent.Msg();
        Symbol sym = msg->Sym(1);
        if (sym == section) {
            MemDoTempAllocations m;
            PracticeSection sect;
            sect.unk0 = msg->Sym(2);
            sect.unk4 = BeatToTickInt(curEvent.start);
            sect.unk8 = -1;
            mPracticeSections.push_back(sect);
        } else if (strncmp(sym.Str(), "prc_", 4) == 0) {
            MemDoTempAllocations m;
            PracticeSection sect;
            sect.unk0 = Symbol(sym.Str());
            sect.unk4 = BeatToTickInt(curEvent.start);
            sect.unk8 = -1;
            mPracticeSections.push_back(sect);
        }
    }
    TrimExcess(mPracticeSections);
    int numsections = mPracticeSections.size();
    for (int i = 0; i < numsections; i++) {
        PracticeSection &cur = mPracticeSections[i];
        if (cur.unk8 == -1) {
            if (i < numsections - 1) {
                cur.unk8 = mPracticeSections[i + 1].unk4;
            } else if (mCodaStartTick != -1) {
                cur.unk8 = mCodaStartTick;
            } else
                cur.unk8 = MsToTickInt(mSongDurationMs);
        }
    }
}

void SongDB::SetFakeHitGemsInFill(bool b1) { mSongData->SetFakeHitGemsInFill(b1); }
void SongDB::RecalculateGemTimes(int i1) { mSongData->RecalculateGemTimes(i1); }

float SongDB::GetPitchOffsetForTick(int tick) const {
    const TuningOffsetList *list = mSongData->mTuningOffsetList;
    return list->IteratorAt(tick, true)->mInfo;
}

void SongDB::EnableGems(int i1, float f2, float f3) { mSongData->EnableGems(i1, f2, f3); }

std::vector<RangeSection> &SongDB::GetRangeSections() {
    return mSongData->mRangeSections;
}

void SongDB::ChangeDifficulty(int i, Difficulty diff) {
    mSongData->ChangeTrackDiff(i, diff);
}

void SongDB::GetBandFailCue(String &str) const {
    BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
        TheSongMgr.GetSongIDFromShortName(MetaPerformer::Current()->Song(), true)
    );
    str = data->BandFailCue().Str();
}

void SongDB::SetTrainerGems(int i, int j) {
    unk24 = i;
    unk28 = j;
}