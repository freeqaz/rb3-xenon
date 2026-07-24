// rb3-xenon native — M5 game-layer scorer (score / multiplier / overdrive / stars).
//
// Extends the M4 hit path (rb3-hit) with the RB3 GAME-LAYER scoring engine: the
// real TrackWatcher Hit/Miss/Pass judgments now drive a faithful native scorer
// that produces real score, streak multiplier, overdrive meter, and a solo star
// rating from the real config/scoring.dta values and formulas (see
// score_engine.h for the exact oracle provenance). This replaces rb3-hit's
// HIT/MISS tally with genuine RB3 numbers.
//
// Wires the REAL beatmatch scoring engine so synthetic input is judged against
// a real gem timeline parsed from a .mid. Pipeline:
//
//   Stage 1 (M3a/M4): SongParser reads the .mid and streams gems into the REAL
//                  src/system/beatmatch/SongData.cpp (M4 replaced the minimal
//                  stand-in). SongData is the InternalSongParserSink and owns the
//                  per-track GameGemDBs, plus phrases, drum fills, roll/trill
//                  lanes, drum-mix, and PlayerTrackConfigList-driven difficulty.
//
//   Stage 2 (M3b): a TrackWatcher (-> GuitarTrackWatcherImpl, the guitar/bass
//                  5-fret hit-detection impl) is constructed against that
//                  SongData for one track at Expert. A native TrackWatcherParent
//                  supplies the beat clock + fill/coda/whammy stubs, and a native
//                  BeatMatchSink prints the engine's Hit / Miss / Pass / SeeGem
//                  verdicts. We drive the clock forward and feed synthetic strums
//                  (exact-time = hit, wrong fret = miss, absent = pass, off-time =
//                  spurious miss). Every judgment below is produced by the real
//                  TrackWatcherImpl / GuitarTrackWatcherImpl code, unmodified.
//
// Why guitar (not joypad): both are 5-fret; GuitarTrackWatcherImpl keys its slop
// window off the gem's own time (InSlopWindow(TimeAt(gem), now)) so exact-time
// strums land cleanly, giving deterministic evidence. Only the guitar impl is
// wired (NewTrackWatcherImpl is guarded to it under HX_NATIVE).

#include "beatmatch/SongParser.h"
#include "beatmatch/SongData.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/TrackType.h"
#include "beatmatch/TrackWatcher.h"
#include "beatmatch/TrackWatcherParent.h"
#include "beatmatch/BeatMatchSink.h"
#include "beatmatch/HitSink.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/FillInfo.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/TuningOffsetList.h"
#include "utl/RangedDataCollection.h"
#include "utl/BeatMap.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongInfoAudioType.h"
#include "utl/TempoMap.h"
#include "utl/MeasureMap.h"
#include "utl/HxGuid.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "beatmatch/Phrase.h"
#include "score_engine.h"

#include <cstdio>
#include <vector>
#include <string>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp

static const int kNumDifficulties = 4;   // Easy / Medium / Hard / Expert
static const int kExpert = 3;

// ---------------------------------------------------------------- config ----
// beatmatcher config read by (a) the SongParser ctor [parser/track_mapping/...]
// and (b) ControllerTypeToTrackWatcherType [controllers/beatmatch_controller_
// mapping]. The controller symbol "guitar" maps to watcher type "guitar" ->
// GuitarTrackWatcherImpl, regardless of whether the track is GUITAR or BASS
// (bass is played on a guitar controller in RB).
static const char *kBeatmatcherDta =
    "(beatmatcher"
    "   (parser"
    "      (player_slot 9)"
    "      (low_vocal_pitch 36)"
    "      (high_vocal_pitch 84)"
    "      (keyboard_range_shift_duration_ms 100.0)"
    "      (track_mapping"
    "         (DRUMS  0 0 'PART DRUMS')"
    "         (BASS   3 2 'PART BASS')"
    "         (GUITAR 4 1 'PART GUITAR')"
    "         (VOCALS 6 3 'PART VOCALS')"
    "         (KEYS   9 4 'PART KEYS')"
    "      )"
    "   )"
    "   (controllers"
    "      (beatmatch_controller_mapping"
    "         (guitar guitar)"
    "         (joypad_guitar guitar)"
    "         (real_guitar real_guitar)"
    "      )"
    "   )"
    "   (audio (submixes))"
    ")";

// The (watcher ...) block TrackWatcherImpl reads: slop + the required
// roll_interval_ms / trill_interval_ms arrays. roll_interval_ms is empty (no
// roll lanes -> GetRollIntervalMs returns 0); trill_interval_ms carries a
// per-difficulty list (Array(1) then Float(diff)).
static const char *kWatcherDta =
    "(watcher"
    "   (slop 100.0)"
    "   (roll_interval_ms)"
    "   (trill_interval_ms (100.0 100.0 100.0 100.0))"
    "   (pitch_bend_range 2)"
    "   (ms_to_full_pitch_bend 1000)"
    ")";

// The (scoring ...) config, values transcribed verbatim from the retail
// config/scoring.dta (points / streaks / overdrive) and star_thresholds.dta
// (instrument_thresholds). This is the same DataArray layout the real
// Scoring::Scoring() ctor parses via SystemConfig("scoring"); we read it through
// the identical DataArray API (FindArray/FindInt/FindFloat) in ScoreConfig::Load.
// The `#include star_thresholds.dta` of the retail file is inlined below (guitar
// + bass rows) so the driver stays self-contained and deterministic.
static const char *kScoringDta =
    "(scoring"
    "   (points"
    "      (drum   (head 25)(tail 12)(chord -1)(pro_bonus 5))"
    "      (bass   (head 25)(tail 12)(chord -1))"
    "      (guitar (head 25)(tail 12)(chord -1))"
    "      (vocals (head 0)(tail 0)(chord -1))"
    "      (keys   (head 25)(tail 12)(chord -1))"
    "      (real_guitar (head 60)(tail 30)(chord 120))"
    "      (real_bass   (head 60)(tail 30)(chord 120)))"
    "   (streaks"
    "      (multipliers"
    "         (singleplayer (0 1)(10 2)(20 3)(30 4))"
    "         (bass         (0 1)(10 2)(20 3)(30 4)(40 5)(50 6))"
    "         (multi        (0 1)(20 2)(40 3)(60 4))"
    "         (default      (0 1)(10 2)(20 3)(30 4)))"
    "      (energy"
    "         (default (0 1))))"
    "   (overdrive"
    "      (recharge_rate 0.0)"
    "      (star_phrase 0.25)"
    "      (common_phrase 0.15)"
    "      (fill_boost 0.35)"
    "      (whammy_rate 3.4e-2)"
    "      (ready_level 0.5)"
    "      (multiplier 2)"
    "      (crowd_boost 6))"
    ")";

// -------------------------------------------------------------- SongInfo ----
// Same minimal SongInfo as M3a: SongParser only needs NumChannelsOfTrack +
// TrackIndex; report every standard instrument present.
class NativeSongInfo : public SongInfo {
public:
    Symbol GetName() const { return Symbol("native_test"); }
    const char *GetBaseFileName() const { return "native_test"; }
    const char *GetPackageName() const { return ""; }
    const std::vector<TrackChannels> &GetTracks() const { return mTracks; }
    bool IsPlayTrackChannel(int) const { return true; }
    const TrackChannels *FindTrackChannel(SongInfoAudioType) const { return nullptr; }
    int NumChannelsOfTrack(SongInfoAudioType t) const { return t <= kAudioTypeKeys3 ? 2 : 0; }
    int TrackIndex(SongInfoAudioType t) const { return (int)t; }
    int GetNumVocalParts() const { return 1; }
    int GetHopoThreshold() const { return 170; }
    const std::vector<float> &GetPans() const { return mPans; }
    const std::vector<float> &GetVols() const { return mVols; }
    const std::vector<int> &GetCores() const { return mCores; }
    const std::vector<int> &GetCrowdChannels() const { return mCores; }
    const std::vector<Symbol> &GetDrumSoloSamples() const { return mSyms; }
    const std::vector<Symbol> &GetDrumFreestyleSamples() const { return mSyms; }
    float GetMuteVolume() const { return -96.0f; }
    float GetVocalMuteVolume() const { return -96.0f; }
    bool UnkTU5Virtual_0x4c() const { return false; }
    int NumExtraMidiFiles() const { return 0; }
    const char *GetExtraMidiFile(int) const { return nullptr; }

    std::vector<TrackChannels> mTracks;
    std::vector<float> mPans, mVols;
    std::vector<int> mCores;
    std::vector<Symbol> mSyms;
};

// ----------------------------------------------------- TrackWatcherParent ----
// Minimal parent: drives the beat clock (SetNow) and stubs everything the
// scoring engine asks about the surrounding chart (fills / coda / solos /
// whammy / pitch-bend). No fills/coda/solo -> all false; no whammy -> -1.
class NativeWatcherParent : public TrackWatcherParent {
public:
    NativeWatcherParent(TempoMap *tm) : mNow(0.0f), mTempoMap(tm) {}
    void SetNow(float f) { mNow = f; }

    float GetNow() const { return mNow; }
    int GetTick() const { return mTempoMap ? (int)mTempoMap->TimeToTick(mNow) : 0; }
    float GetWhammyBar() const { return -1.0f; }
    int GetMaxSlots() const { return 5; }
    void SetPitchBend(int, float, bool) {}
    void ResetPitchBend(int) {}
    bool InFillNow() { return false; }
    bool InFill(int, bool) { return false; }
    bool FillsEnabled(int) { return false; }
    FillLogic GetFillLogic() const { return (FillLogic)0; }
    bool InSolo(int) { return false; }
    bool InCoda(int) { return false; }
    bool InCodaFreestyle(int, bool) { return false; }
    void SetButtonMashingMode(bool) {}
    int GetVelocityBucket(int) { return 0; }
    int GetVirtualSlot(int) { return 0; }
    void PlayDrum(int, bool, float) {}

    float mNow;
    TempoMap *mTempoMap;
};

// --------------------------------------------------------- BeatMatchSink -----
// The scoring sink. TrackWatcherImpl::Send{Hit,Miss,Pass,Seen,Ignore,...} fire
// these callbacks — this is where the engine's judgments surface. (HitSink is a
// separate, audio-feedback interface consumed by MasterAudio for pad/fret SFX;
// it is not on the TrackWatcher scoring path, so the scoring sink is the one the
// hit-detection wiring needs. A trivial NativeHitSink is included below to show
// the interface, unused by the timeline.)
static const char *kSlotName[6] = { "green", "red", "yellow", "blue", "orange", "?" };
static const char *SlotName(int s) { return (s >= 0 && s < 5) ? kSlotName[s] : "?"; }

// The scoring sink: the real TrackWatcher Hit/Miss/Pass verdicts drive the
// faithful native ScoreState (score_engine.*). Every scoring decision below —
// points, streak build/reset, multiplier — is the real RB3 math applied to the
// real engine judgments.
class ScoringSink : public BeatMatchSink {
public:
    ScoreState &mState;
    int mHits, mMisses, mPasses, mSeen, mIgnores;
    ScoringSink(ScoreState &st)
        : mState(st), mHits(0), mMisses(0), mPasses(0), mSeen(0), mIgnores(0) {}

    void SeeGem(int trk, float ms, int gemID) {
        mSeen++;
        printf("      -> SEE   gem %d approaches (t=%.1f)\n", gemID, ms);
    }
    void Hit(int trk, float ms, int gemID, unsigned int slots, GemHitFlags) {
        mHits++;
        int before = mState.Score();
        int numSlots = GameGem::CountBitsInSlotType(slots);
        mState.OnHit(numSlots); // BuildHitStreak + AddHeadPoints -> AddPoints
        int delta = mState.Score() - before;
        printf("      => HIT   gem %d (%s)  +%d pts  |  score=%d  streak=%d  "
               "mult=x%d%s\n",
               gemID, SlotName(GameGem::GetHighestSlot(slots)), delta,
               mState.Score(), mState.Streak(), mState.TotalMultiplier(),
               mState.OverdriveDeployed() ? "  [OVERDRIVE 2x]" : "");
    }
    void Miss(int trk, int slot, float ms, int gemID, int, GemHitFlags) {
        mMisses++;
        mState.OnMiss(); // BuildMissStreak
        printf("      => MISS  gem %d  fret=%s  |  score=%d  streak=%d\n", gemID,
               SlotName(slot), mState.Score(), mState.Streak());
    }
    void SpuriousMiss(int trk, int slot, float ms, int gemID) {
        mMisses++;
        printf("      => MISS  (spurious) fret=%s t=%.1f\n", SlotName(slot), ms);
    }
    void Pass(int trk, float ms, int gemID, bool) {
        mPasses++;
        int hadStreak = mState.Streak();
        mState.OnPass(); // EndHitStreak (streak -> 0) + BuildMissStreak
        printf("      => PASS  gem %d dropped  |  score=%d  streak %d -> 0 (broken)\n",
               gemID, mState.Score(), hadStreak);
    }
    void Ignore(int trk, float ms, int gemID, const UserGuid &) {
        mIgnores++;
        printf("      .. IGNORE gem %d (t=%.1f)\n", gemID, ms);
    }
    void ImplicitGem(int, float, int, const UserGuid &) {}

    // Remaining BeatMatchSink surface — silent (not exercised by the timeline).
    void Swing(int, int, float, bool, bool) {}
    void SetTrack(const UserGuid &, int) {}
    void FretButtonDown(int, float) {}
    void FretButtonUp(int, float) {}
    void MercurySwitch(bool, float) {}
    void FilteredWhammyBar(float) {}
    void SwingAtHopo(int, float, int) {}
    void Hopo(int, float, int) {}
    void ReleaseGem(int, float, int, float) {}
    void SetCurrentPhrase(int, const PhraseInfo &) {}
    void NoCurrentPhrase(int) {}
    void FillSwing(int, int, int, int, bool) {}
    void FillReset() {}
    void FillComplete(int, int) {}
};

// Audio-feedback interface (kHitGreenFret etc.), shown for completeness. Not on
// the scoring path (TrackWatcher sinks are BeatMatchSink*), so it is unused here.
class NativeHitSink : public HitSink {
public:
    void Hit(HitType) {}
    void Key(int) {}
    void RGFret(int, int) {}
    void RGStrum(int) {}
};

// ---------------------------------------------------------------- driver -----
struct SwingEv {
    float t;
    int slot;
    const char *label;
    bool fired;
};

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-cort/hurt/pills/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;

    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    // Root system config holds both (beatmatcher ...) and (scoring ...) so that
    // SystemConfig("beatmatcher") and SystemConfig("scoring") both resolve.
    std::string cfg = std::string(kBeatmatcherDta) + "\n" + kScoringDta;
    gSystemConfig = DataReadString(cfg.c_str());
    DataArray *scoringCfg = gSystemConfig->FindArray(Symbol("scoring"));

    printf("=== rb3-xenon native M5: game-layer scorer (score / multiplier / OD / stars) ===\n");
    printf("mid: %s\n\n", midPath);

    NativeSongInfo songInfo;
    SongData songData;
    songData.mNumDifficulties = kNumDifficulties;
    songData.mHopoThreshold = songInfo.GetHopoThreshold();
    // The driver drives its own SongParser (below) instead of SongData::Load,
    // so replicate the per-load state Load() would allocate before the real
    // InternalSongParserSink callbacks fire: the BeatMap (SongData::AddBeat
    // writes it), the tuning-offset list (AddPitchOffset), the phrase analyzer,
    // per-difficulty keyboard range sections, and mSongInfo (SongFullPath).
    songData.mSongInfo = &songInfo;
    songData.mDetailedGrid = false;
    songData.mBeatMap = new BeatMap();
    SetTheBeatMap(songData.mBeatMap);
    songData.mPhraseAnalyzer = new PhraseAnalyzer(&songData);
    songData.mTuningOffsetList = new TuningOffsetList();
    songData.mKeyboardRangeSections.resize(kNumDifficulties);

    TempoMap *tempoMap = nullptr;
    MeasureMap *measureMap = nullptr;

    // --- Stage 1: parse the .mid into the (minimal) SongData ----------------
    printf("--- Stage 1: SongParser -> SongData gem parse ---\n");
    {
        FileStream fs(midPath, FileStream::kRead, false);
        SongParser parser(songData, kNumDifficulties, tempoMap, measureMap, 2);
        parser.ReadMidiFile(fs, midPath, &songInfo);
        int pumps = 0;
        while (!parser.NoMidiReader() && pumps < 2000000) {
            parser.Poll();
            pumps++;
        }
        printf("  drove SongParser in %d Poll() pump(s).\n", pumps);
    }
    songData.mTempoMap = tempoMap;
    songData.mMeasureMap = measureMap;
    for (size_t i = 0; i < songData.mGemDBs.size(); i++) {
        songData.mGemDBs[i]->MergeChordGems();
        songData.mGemDBs[i]->Finalize();
    }
    printf("  tempoMap=%p  parsed %d track(s).\n", (void *)tempoMap,
           (int)songData.mTrackInfos.size());

    // --- Stage 1.5: real difficulty selection via PlayerTrackConfigList ------
    // The REAL SongData (unlike the old stand-in, which hard-pinned Expert in
    // AddTrack) leaves mTrackDifficulties empty until a PlayerTrackConfigList is
    // resolved. Difficulty selection is exactly the retail PostLoad path:
    // FixUpTrackConfig() builds the per-track TrackType list and hands it to
    // PlayerTrackConfigList::Process(); SetUpTrackDifficulties() then copies the
    // resolved per-track diffs into SongData. Default every track to Expert.
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpert;
    songData.mPlayerTrackConfigList = &expertList;
    songData.FixUpTrackConfig(&expertList);
    songData.SetUpTrackDifficulties(&expertList);
    printf("  difficulty: PlayerTrackConfigList(default=Expert) -> FixUpTrackConfig "
           "-> SetUpTrackDifficulties\n");

    // Process a second config at Medium so we can flip difficulty on demand.
    PlayerTrackConfigList mediumList(1);
    mediumList.mDefaultDifficulty = 1; // Easy=0 Medium=1 Hard=2 Expert=3
    songData.FixUpTrackConfig(&mediumList);

    // --- SongData chart surface (real SongData.cpp) -------------------------
    // Everything below is answered by the real SongData query API populated
    // during parse: gems per selected difficulty, drum fills (DrumFillInfo),
    // and roll/trill lanes (RangedDataCollection). The stand-in reported none.
    printf("\n--- SongData chart surface (real SongData.cpp query API) ---\n");
    printf("  %-4s %-14s %-5s %-5s %6s %6s %6s %6s\n", "trk", "name", "type",
           "diff", "gems", "fills", "rolls", "trills");
    for (int t = 0; t < songData.GetNumTracks(); t++) {
        int diff = songData.TrackDiffAt(t);
        int nG = songData.GetGemList(t)->NumGems();
        FillInfo *fi = songData.GetFillInfo(t);
        int nFills = fi ? (int)fi->mFills.size() : 0;
        RangedDataCollection<unsigned int> *ri = songData.GetRollInfo(t);
        RangedDataCollection<std::pair<int, int> > *ti = songData.GetTrillInfo(t);
        int nRolls = (diff < (int)ri->mRangeDataArray.size())
                         ? (int)ri->mRangeDataArray[diff].size()
                         : 0;
        int nTrills = (diff < (int)ti->mRangeDataArray.size())
                          ? (int)ti->mRangeDataArray[diff].size()
                          : 0;
        printf("  %-4d %-14s %-5d %-5d %6d %6d %6d %6d\n", t,
               songData.TrackName(t).Str(), (int)songData.TrackTypeAt(t), diff, nG,
               nFills, nRolls, nTrills);
    }
    printf("\n");

    // --- Pick a playable track: prefer PART BASS, else first non-empty ------
    int track = songData.TrackNamed(Symbol("PART BASS"));
    if (track == -1 || songData.GetGemList(track)->NumGems() == 0) {
        track = -1;
        for (size_t i = 0; i < songData.mTrackInfos.size(); i++) {
            if (songData.GetGemList((int)i)->NumGems() > 0) {
                track = (int)i;
                break;
            }
        }
    }
    if (track == -1) {
        printf("  no playable track parsed; aborting.\n");
        return 1;
    }
    // --- difficulty selection is real: flip Expert<->Medium, gem count moves --
    songData.SetUpTrackDifficulties(&mediumList);
    int nMedium = songData.GetGemList(track)->NumGems();
    songData.SetUpTrackDifficulties(&expertList);
    int nExpert = songData.GetGemList(track)->NumGems();
    printf("  difficulty flip on track %d '%s': Expert=%d gems  Medium=%d gems  (%s)\n",
           track, songData.TrackName(track).Str(), nExpert, nMedium,
           nExpert != nMedium ? "SetUpTrackDifficulties re-selects the chart"
                              : "same count at both diffs for this track");

    GameGemList *gems = songData.GetGemList(track); // back on Expert
    int nGems = gems->NumGems();
    printf("  track %d '%s' (type %d): %d Expert gem(s)\n\n", track,
           songData.TrackName(track).Str(), (int)songData.TrackTypeAt(track), nGems);

    printf("--- Expert gem timeline ---\n");
    printf("  %-4s %8s %10s %8s\n", "#", "tick", "ms", "slot");
    for (int i = 0; i < nGems; i++) {
        GameGem &g = gems->GetGem(i);
        printf("  %-4d %8d %10.1f  %d (%s)\n", i, g.GetTick(), g.GetMs(),
               g.GetSlot(), SlotName(g.GetSlot()));
    }
    printf("\n");

    // --- Stage 2: build the TrackWatcher + drive synthetic input ------------
    DataArray *watcherTop = DataReadString(kWatcherDta);
    DataArray *watcherCfg = watcherTop->FindArray("watcher");

    UserGuid guid;
    NativeWatcherParent parent(tempoMap);

    // --- Build the game-layer scorer from the real scoring.dta config -------
    // Pick the points/streak config for this track's instrument, exactly as
    // Player would from mUser->GetTrackType() + mBehavior->mStreakType/mMaxMult.
    int trackType = (int)songData.TrackTypeAt(track);
    const char *instrument = "guitar";
    const char *streakList = "singleplayer";
    int maxMult = 4;
    std::vector<float> starThresh; // instrument_thresholds row (multiples of base)
    if (trackType == kTrackBass) {
        instrument = "bass";
        streakList = "bass"; // bass ramps to 6x
        maxMult = 6;
        // (bass 0.21 0.5 0.9 2.77 4.62 6.78) from star_thresholds.dta
        float b[] = { 0.21f, 0.5f, 0.9f, 2.77f, 4.62f, 6.78f };
        starThresh.assign(b, b + 6);
    } else if (trackType == kTrackDrum) {
        instrument = "drum";
        streakList = "singleplayer";
        float d[] = { 0.21f, 0.46f, 0.77f, 1.85f, 3.08f, 4.29f };
        starThresh.assign(d, d + 6);
    } else if (trackType == kTrackKeys) {
        instrument = "keys";
        streakList = "singleplayer";
        float k[] = { 0.21f, 0.46f, 0.77f, 1.85f, 3.08f, 4.52f };
        starThresh.assign(k, k + 6);
    } else {
        // guitar (0.21 0.46 0.77 1.85 3.08 4.52)
        float g[] = { 0.21f, 0.46f, 0.77f, 1.85f, 3.08f, 4.52f };
        starThresh.assign(g, g + 6);
    }
    ScoreConfig scoreCfg;
    scoreCfg.Load(scoringCfg, instrument, streakList, maxMult, starThresh);
    printf("--- scoring config (config/scoring.dta) ---\n");
    printf("  instrument=%s  head=%d  chord=%d  streak_list=%s  max_mult=x%d  "
           "OD(star_phrase=%.2f ready=%.2f mult=x%d)\n\n",
           instrument, scoreCfg.HeadPoints(), scoreCfg.ChordPoints(), streakList,
           scoreCfg.MaxMultiplier(), scoreCfg.OverdriveStarPhrase(),
           scoreCfg.OverdriveReadyLevel(), scoreCfg.OverdriveMultiplier());

    // Base score = a perfect run (all gems, streak-ramped, no OD) -> the
    // denominator for solo star cutoffs.
    std::vector<int> allSlots;
    for (int i = 0; i < nGems; i++)
        allSlots.push_back(GameGem::CountBitsInSlotType(gems->GetGem(i).GetSlots()));
    int baseScore = ComputeBaseScore(scoreCfg, allSlots);
    printf("  base score (perfect run, %d gems, no overdrive) = %d\n", nGems, baseScore);
    {
        // Show the star-cutoff ladder for this chart.
        printf("  star cutoffs:");
        const std::vector<float> &m = scoreCfg.StarThresholds();
        for (size_t i = 0; i < m.size(); i++)
            printf("  %d*=%d", (int)i + 1, (int)(m[i] * (float)baseScore));
        printf("\n\n");
    }

    ScoreState score(scoreCfg);
    ScoringSink sink(score);

    TrackWatcher watcher(track, guid, /*playerSlot*/ 0, Symbol("guitar"), &songData,
                         &parent, watcherCfg);
    watcher.AddSink(&sink);
    watcher.SetIsCurrentTrack(true);
    watcher.Jump(0.0f); // seed gem cursors from the start of the song

    printf("--- Stage 2: synthetic input vs the scoring engine ---\n");
    printf("(GuitarTrackWatcherImpl judgments; slop = 100 ms)\n\n");

    // Build a data-driven schedule from the real gem times. Scenarios:
    //   gem0            : correct fret, exact time      -> HIT
    //   gem1            : correct fret, exact time      -> HIT
    //   gem2            : WRONG fret, exact time        -> MISS (fret timeout)
    //   gem3            : no input (absent)             -> PASS
    //   gem4 (if any)   : correct fret, exact time      -> HIT
    //   pre-song strum  : off-time, before any gem      -> MISS (spurious)
    std::vector<SwingEv> sched;
    float ms0 = gems->GetGem(0).GetMs();
    float preT = ms0 > 800.0f ? ms0 - 400.0f : ms0 * 0.4f;
    SwingEv pre = { preT, 2, "off-time strum before song", false };
    sched.push_back(pre);
    for (int i = 0; i < nGems; i++) {
        GameGem &g = gems->GetGem(i);
        int correct = g.GetSlot();
        if (i == 2) {
            int wrong = (correct == 0) ? 1 : 0;
            SwingEv e = { g.GetMs(), wrong, "wrong fret", false };
            sched.push_back(e);
        } else if (i == 3) {
            continue; // absent: no strum -> expect PASS
        } else {
            SwingEv e = { g.GetMs(), correct, "correct fret on-time", false };
            sched.push_back(e);
        }
    }

    float endT = gems->GetGem(nGems - 1).GetMs() + 800.0f;
    for (float now = 0.0f; now <= endT; now += 4.0f) {
        for (size_t k = 0; k < sched.size(); k++) {
            if (!sched[k].fired && sched[k].t <= now) {
                sched[k].fired = true;
                parent.SetNow(now);
                printf("  [t=%7.1f] STRUM fret=%s   (%s)\n", now,
                       SlotName(sched[k].slot), sched[k].label);
                watcher.Swing(sched[k].slot, /*guitar*/ true, /*provisional*/ false,
                              kGemHitFlagNone);
            }
        }
        parent.SetNow(now);
        watcher.Poll(now);
    }

    printf("\n--- scenario result (real judgments -> real scoring) ---\n");
    printf("  HIT=%d  MISS=%d  PASS=%d  SEE=%d\n", sink.mHits, sink.mMisses,
           sink.mPasses, sink.mSeen);
    printf("  SCORE=%d   final streak=%d   longest streak=%d   final mult=x%d\n",
           score.Score(), score.Streak(), score.LongestStreak(),
           score.TotalMultiplier());
    printf("  stars (vs base %d) = %d  (%.2f fractional)\n\n", baseScore,
           score.Stars(baseScore), score.StarsFloat(baseScore));

    // --- Overdrive (star power) meter --------------------------------------
    // Real star-power phrases are the kCommonPhrase list on the track. Award
    // OverdriveConfig::star_phrase energy for each fully-hit phrase.
    printf("--- overdrive (star power) meter ---\n");
    const PhraseList &odPhrases = songData.GetPhraseList(track, kCommonPhrase);
    int nOD = (int)odPhrases.mPhrases.size();
    printf("  chart star-power (kCommonPhrase) phrases on this track: %d\n", nOD);
    if (nOD > 0) {
        for (int i = 0; i < nOD; i++) {
            score.CompleteOverdrivePhrase();
            printf("  phrase %d complete -> OD meter = %.0f%%%s\n", i,
                   score.OverdriveEnergy() * 100.0f,
                   score.OverdriveReady() ? "  [READY to deploy]" : "");
        }
    } else {
        // Charts often have no parsed OD phrases in this native slice — the
        // MIDI star-power markers are not streamed by the minimal parser path.
        // Demonstrate the meter path synthetically with the real config values.
        printf("  (none parsed on this slice; demonstrating the meter path "
               "synthetically with real config values)\n");
        for (int i = 0; i < 2; i++) {
            score.CompleteOverdrivePhrase();
            printf("  synthetic phrase %d complete -> OD meter = %.0f%%%s\n", i,
                   score.OverdriveEnergy() * 100.0f,
                   score.OverdriveReady() ? "  [READY to deploy]" : "");
        }
    }

    if (score.OverdriveReady()) {
        int multBefore = score.TotalMultiplier();
        bool deployed = score.DeployOverdrive();
        printf("  DEPLOY overdrive: %s -> band-energy multiplier now x%d "
               "(was x%d)\n",
               deployed ? "activated" : "failed", score.OverdriveMultiplierNow(),
               multBefore ? multBefore : 1);
        // Show a scored note under overdrive: doubles the per-note points.
        int b = score.Score();
        score.OnHit(1);
        printf("  scored 1 note while deployed: +%d pts (individual x%d * OD x%d) "
               "-> score=%d\n",
               score.Score() - b, score.IndividualMultiplier(),
               score.OverdriveMultiplierNow(), score.Score());
        score.DrainOverdrive(1.0f);
        printf("  overdrive drained -> meter=%.0f%%  deployed=%s\n",
               score.OverdriveEnergy() * 100.0f,
               score.OverdriveDeployed() ? "yes" : "no");
    } else {
        printf("  OD meter below ready_level (%.0f%%) — not deployable.\n",
               scoreCfg.OverdriveReadyLevel() * 100.0f);
    }

    // --- Stage 3: vicarious stress run -------------------------------------
    // The real chart above is only 5 gems, so the streak never reaches the 10-
    // note threshold where the individual multiplier climbs. To exercise the
    // full multiplier ramp (1x -> 2x -> 3x -> 4x ...), a mid-run streak break,
    // an overdrive deploy, and a plausible final star rating, drive the SAME
    // ScoreState/ScoreConfig math over a longer synthetic single-note run.
    printf("\n--- Stage 3: vicarious stress run (%s scoring math, synthetic gems) ---\n",
           instrument);
    const int kStress = 64;
    std::vector<int> stressSlots(kStress, 1); // 64 single-note gems
    int stressBase = ComputeBaseScore(scoreCfg, stressSlots);
    ScoreState run(scoreCfg);
    int lastMult = -1;
    int missAt = 34;    // one dropped gem mid-run
    int deployAt = 20;  // deploy overdrive once ready
    for (int i = 0; i < kStress; i++) {
        if (i == missAt) {
            int hadStreak = run.Streak();
            run.OnPass();
            printf("  gem %2d: DROPPED -> streak %d -> 0 (multiplier resets to x1)\n",
                   i, hadStreak);
            lastMult = -1;
            continue;
        }
        // Feed overdrive so the meter is deployable partway through.
        if (i == 8 || i == 16)
            run.CompleteOverdrivePhrase();
        if (i == deployAt && run.OverdriveReady() && !run.OverdriveDeployed()) {
            run.DeployOverdrive();
            printf("  gem %2d: DEPLOY overdrive (meter %.0f%%) -> band x%d engaged\n",
                   i, run.OverdriveEnergy() * 100.0f, run.OverdriveMultiplierNow());
        }
        int before = run.Score();
        run.OnHit(1);
        if (run.OverdriveDeployed())
            run.DrainOverdrive(0.12f); // deplete over a handful of notes
        int mult = run.TotalMultiplier();
        if (mult != lastMult) {
            printf("  gem %2d: streak=%d -> multiplier x%d  (+%d pts, score=%d)%s\n", i,
                   run.Streak(), mult, run.Score() - before, run.Score(),
                   run.OverdriveDeployed() ? "  [OD 2x]" : "");
            lastMult = mult;
        }
    }
    printf("  final: score=%d  longest streak=%d  hits=%d\n", run.Score(),
           run.LongestStreak(), run.Hits());
    printf("  stars (vs base %d) = %d  (%.2f fractional)\n", stressBase,
           run.Stars(stressBase), run.StarsFloat(stressBase));

    printf("\nDone.\n");
    return 0;
}
