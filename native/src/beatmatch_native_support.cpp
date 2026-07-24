// rb3-xenon native — M3b beatmatch hit-detection support TU.
//
// Provides the two link-only pieces the hit-detection path (TrackWatcher +
// TrackWatcherImpl + GuitarTrackWatcherImpl) needs but that have no compiled
// definition in the native subset:
//
//   1. A MINIMAL SongData stand-in. TrackWatcher/TrackWatcherImpl call SongData
//      *non-virtually* (GetGemList / GetRollingSlotsAtTick / GetTrillSlotsAtTick
//      / GetNextRoll / GetFillInfo) and virtually (GetTempoMap, the sink and
//      GemListInterface methods), so the object must have the real SongData
//      vtable — which forces every SongData virtual to be defined. Compiling the
//      real SongData.cpp would drag in DrumFillInfo/DrumMap/DrumMixDB/PhraseDB/
//      VocalNoteList/PlayerTrackConfigList/TuningOffsetList/PhraseAnalyzer, so
//      per the M3b brief we build a minimal stand-in instead: it is itself the
//      SongParser sink (AddTrack/AddMultiGem/OnEndOfTrack populate GameGemDBs,
//      exactly as M3a's NativeGemSink did), stores gems per track, and answers
//      the five query methods (rolls/trills/fills report "none" — a chart with
//      no roll/trill/fill lanes, which is all the deterministic scenario needs).
//      NOTE for M4: full SongData.cpp is required for phrases/solos/fills/coda/
//      drum-mix/vocals/disco-flip/backup-tracks and PlayerTrackConfigList-driven
//      SetUpTrackDifficulties (per-player difficulty). See report.
//
//   2. A LogFile stub + the `TheBeatMatchOutput` global. TrackWatcherImpl.cpp
//      references TheBeatMatchOutput.Print inside `if (IsActive())` guards, but
//      there is no LogFile.cpp in-tree and no definition of the extern global.
//      We define both here, inactive, so the guarded prints are never taken and
//      the engine's own beatmatch judgments come purely through the BeatMatchSink.
//
// This TU is native-only (native/CMakeLists) and never enters the X360 build.

#include "beatmatch/SongData.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/GameGemList.h"
#include "utl/LogFile.h"
#include "math/Utl.h"

// ----------------------------------------------------------- LogFile shim ----
LogFile::LogFile(const char *pattern)
    : mFilePattern(pattern), mSerialNumber(0), mDirty(false), mFile(0), mActive(false) {}
LogFile::~LogFile() {}
void LogFile::Print(const char *) {}
void LogFile::Reset() {}
void LogFile::AdvanceFile() {}

// The global the beatmatch code logs through; kept inactive (IsActive()==false)
// so every `if (TheBeatMatchOutput.IsActive())` branch is skipped.
LogFile TheBeatMatchOutput("beatmatch");

// --------------------------------------------------- SongData::TrackInfo -----
// Minimal: only mType / mIndependentSlots (read by TrackTypeAt /
// TrackHasIndependentSlots) and mName matter to the hit path. mLyrics is left
// null (the real ctor news a TickedInfoCollection<String>; not on this path).
SongData::TrackInfo::TrackInfo(
    Symbol sym, SongInfoAudioType audioty, AudioTrackNum num, TrackType trackty, bool indep
)
    : mName(sym), mLyrics(0), mAudioType((BeatmatchAudioType)audioty),
      mAudioTrackNum(num), mType(trackty), mIndependentSlots(indep) {}

SongData::TrackInfo::~TrackInfo() {}

// -------------------------------------------------------- SongData (min) -----
SongData::SongData()
    : mNumTracks(0), mNumDifficulties(0), mLoaded(0), mSongInfo(0), mSectionStartTick(-1),
      mSectionEndTick(-1), mFakeHitGemsInFill(0), mPhraseAnalyzer(0),
      mLoadingVocalNoteListIndex(0), mTempoMap(0), mMeasureMap(0), mBeatMap(0),
      mTuningOffsetList(0), mLastGemTime(0), mMemStream(0), mSongParser(0),
      mPlayerTrackConfigList(0), mGems(0), mHopoThreshold(0), mDetailedGrid(0) {
    // Minimal: skip the real ctor's `new VocalNoteList(this)` (would pull in
    // VocalNoteList.cpp); vocals are off the guitar/bass hit path.
}

SongData::~SongData() {
    for (size_t i = 0; i < mTrackInfos.size(); i++) {
        delete mTrackInfos[i];
        if (i < mGemDBs.size())
            delete mGemDBs[i];
    }
}

// --- InternalSongParserSink (the SongParser feeds gems in through these) ---
void SongData::AddTrack(
    int, AudioTrackNum a, Symbol s, SongInfoAudioType songTy, TrackType trackTy, bool indep
) {
    mNumTracks++;
    mTrackInfos.push_back(new TrackInfo(s, songTy, a, trackTy, indep));
    // Default every track to the top difficulty (Expert). The real engine sets
    // this per-player via SetUpTrackDifficulties(PlayerTrackConfigList); the
    // native scenario is single-player Expert, so pin it directly.
    mTrackDifficulties.push_back(mNumDifficulties - 1);
    mGemDBs.push_back(new GameGemDB(mNumDifficulties, mHopoThreshold));
}

void SongData::ClearTrack(int track) {
    if (track >= 0 && track < (int)mGemDBs.size())
        mGemDBs[track]->Clear();
}

void SongData::OnEndOfTrack(int i, bool merge) {
    if (merge && i >= 0 && i < (int)mGemDBs.size())
        mGemDBs[i]->MergeChordGems();
}

void SongData::AddMultiGem(int diff, const MultiGemInfo &info) {
    mLastGemTime = Max(mLastGemTime, info.ms + info.duration_ms);
    int trk = info.track;
    if (trk >= 0 && trk < (int)mGemDBs.size())
        mGemDBs[trk]->AddMultiGem(diff, info);
}

void SongData::AddRGGem(int diff, const RGGemInfo &info) {
    int trk = info.track;
    if (trk >= 0 && trk < (int)mGemDBs.size())
        mGemDBs[trk]->AddRGGem(diff, info);
}

// Off the guitar/bass gem path — no-ops (vocals / phrases / fills / mixes / rolls).
void SongData::AddVocalNote(const VocalNote &) {}
void SongData::AddPitchOffset(int, float) {}
void SongData::AddLyricShift(int) {}
void SongData::OnTambourineGem(int) {}
void SongData::StartVocalPlayerPhrase(int, int) {}
void SongData::EndVocalPlayerPhrase(int, int) {}
void SongData::AddPhrase(BeatmatchPhraseType, int, int, float, int, float, int) {}
void SongData::AddDrumFill(int, int, int, int, bool) {}
void SongData::AddRoll(int, int, unsigned int, int, int) {}
void SongData::AddTrill(int, int, int, int, int, int) {}
void SongData::AddRGRoll(int, int, const RGRollChord &, int, int) {}
void SongData::AddRGTrill(int, int, const RGTrill &, int, int) {}
void SongData::AddMix(int, int, int, const char *) {}
void SongData::AddLyricEvent(int, int, const char *) {}
void SongData::DrumMapLane(int, int, int, bool) {}
void SongData::AddBeat(int, int) {}
void SongData::SetDetailedGrid(bool b) { mDetailedGrid = b; }
void SongData::AddRangeShift(int, float) {}
void SongData::AddKeyboardRangeShift(int, int, float, int, int) {}

// --- HxSongData (only GetTempoMap is exercised; CalcSongPos is BeatMatcher's
//     SetNow path, which the native driver bypasses) ---
SongPos SongData::CalcSongPos(float f) {
    if (!mTempoMap)
        return SongPos();
    float tick = mTempoMap->TimeToTick(f);
    return SongPos(tick, 0.0f, 0, 0, (int)tick);
}
SongPos SongData::CalcSongPos(class HxMaster *, float f) { return CalcSongPos(f); }

// --- GemListInterface (mirror of the real SongData; queried post-hoc) ---
void SongData::SetTrack(Symbol name) {
    mGems = 0;
    int trk = TrackNamed(name);
    if (trk != -1)
        mGems = GetGemListByDiff(trk, GetNumDifficulties() - 1);
}
bool SongData::GetGem(int gemId, int &startTick, int &endTick, int &slots) {
    if (mGems && gemId < mGems->NumGems()) {
        GameGem &g = mGems->GetGem(gemId);
        startTick = g.GetTick();
        endTick = startTick + g.GetDurationTicks();
        slots = g.GetSlots();
        return true;
    }
    return false;
}

// --- SongData query surface the TrackWatcher path calls (non-virtual) --------
GameGemList *SongData::GetGemListByDiff(int track, int diff) {
    return mGemDBs[track]->GetDiffGemList(diff);
}
GameGemList *SongData::GetGemList(int track) {
    return GetGemListByDiff(track, mTrackDifficulties[track]);
}
int SongData::TrackNamed(Symbol name) const {
    for (size_t i = 0; i < mTrackInfos.size(); i++)
        if (mTrackInfos[i]->mName == name)
            return (int)i;
    return -1;
}
Symbol SongData::TrackName(int track) const { return mTrackInfos[track]->mName; }

// No roll / trill / fill lanes in the minimal chart: report "nothing here".
unsigned int SongData::GetRollingSlotsAtTick(int, int) const { return 0; }
bool SongData::GetNextRoll(int, int, unsigned int &, int &) const { return false; }
bool SongData::GetTrillSlotsAtTick(int, int, std::pair<int, int> &) const { return false; }
FillInfo *SongData::GetFillInfo(int) { return 0; }

// FillInfo has no compiled .cpp in-tree. TrackWatcherImpl::IsFillCompletion
// references FillExtentAtOrBefore, but it is only reached when the parent
// reports FillsEnabled()==true — never in the native driver (no fill lanes,
// GetFillInfo()==0). Stub it so the reference links; it is never called.
bool FillInfo::FillExtentAtOrBefore(int, FillExtent &) const { return false; }
