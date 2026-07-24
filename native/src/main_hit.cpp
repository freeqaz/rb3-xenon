// rb3-xenon native — M3b hit-detection driver (second half of the gameplay core).
//
// Wires the REAL beatmatch scoring engine so synthetic input is judged against
// a real gem timeline parsed from a .mid. Pipeline:
//
//   Stage 1 (M3a): SongParser reads the .mid and streams gems into a minimal
//                  SongData (native/src/beatmatch_native_support.cpp) that is the
//                  SongParser sink and owns the per-track GameGemDBs.
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

#include <cstdio>
#include <vector>

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

class NativeBeatMatchSink : public BeatMatchSink {
public:
    int mHits, mMisses, mPasses, mSeen, mIgnores;
    NativeBeatMatchSink() : mHits(0), mMisses(0), mPasses(0), mSeen(0), mIgnores(0) {}

    void SeeGem(int trk, float ms, int gemID) {
        mSeen++;
        printf("      -> SEE   gem %d approaches (t=%.1f)\n", gemID, ms);
    }
    void Hit(int trk, float ms, int gemID, unsigned int slots, GemHitFlags) {
        mHits++;
        printf("      => HIT   gem %d  slots=0x%02x (%s)  t=%.1f  [correct on-time strum]\n",
               gemID, slots, SlotName(GameGem::GetHighestSlot(slots)), ms);
    }
    void Miss(int trk, int slot, float ms, int gemID, int, GemHitFlags) {
        mMisses++;
        printf("      => MISS  gem %d  fret=%s  t=%.1f  [wrong fret / off-time strum]\n",
               gemID, SlotName(slot), ms);
    }
    void SpuriousMiss(int trk, int slot, float ms, int gemID) {
        mMisses++;
        printf("      => MISS  (spurious) fret=%s t=%.1f\n", SlotName(slot), ms);
    }
    void Pass(int trk, float ms, int gemID, bool) {
        mPasses++;
        printf("      => PASS  gem %d  t=%.1f  [gem went by unplayed -> dropped]\n",
               gemID, ms);
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

    gSystemConfig = DataReadString(kBeatmatcherDta);

    printf("=== rb3-xenon native M3b: hit detection (BeatMatch scoring) ===\n");
    printf("mid: %s\n\n", midPath);

    NativeSongInfo songInfo;
    SongData songData;
    songData.mNumDifficulties = kNumDifficulties;
    songData.mHopoThreshold = songInfo.GetHopoThreshold();

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
    GameGemList *gems = songData.GetGemList(track);
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
    NativeBeatMatchSink sink;

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

    printf("\n--- tally ---\n");
    printf("  HIT=%d  MISS=%d  PASS=%d  SEE=%d  IGNORE=%d\n", sink.mHits, sink.mMisses,
           sink.mPasses, sink.mSeen, sink.mIgnores);
    printf("\nDone.\n");
    return 0;
}
