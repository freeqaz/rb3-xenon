// rb3-xenon native M12 — the REAL crowd-rating / band-failure meter.
//
// WHY THIS MILESTONE EXISTS
// -------------------------
// A stub-execution probe (native/src/cc5_stub_probe.c, enabled by the
// RB3_STUB_PROBE CMake option) measured which no-op link stubs actually run.
// During ONE rb3-score4 run it recorded:
//
//     25,905x  CrowdRating::Poll(float)
//
// i.e. the crowd meter was a no-op called every single frame while M8 reported
// its results as though the subsystem were present. Worse, the implementation
// had been decompiled in-tree the whole time (src/band3/game/CrowdRating.cpp,
// 139 lines, all 17 methods real) — only the NATIVE build was stubbing it, and
// m8_link_stubs.cpp / main_score4.cpp documented it as "CrowdRating (no impl
// anywhere)", which was simply out of date.
//
// M12 links the real CrowdRating and drives it from a real chart, so the crowd
// meter, the excitement levels, the warning state and BAND FAILURE are genuine
// engine computations over genuine shipped Harmonix tuning values.
//
// REAL vs DRIVER-SUPPLIED census (be precise about this — see the report):
//   REAL : CrowdRating in full — Configure/Reset/Update/UpdatePhrase/
//          CalculateValue/SetValue/SetDisplayValue/GetDisplayValue/GetExcitement/
//          GetThreshold/IsInWarning/IsBelowLoseLevel/CantFailYet/Poll.
//   REAL : the config — RB3's shipped config/scoring.dta `(crowd ...)` block,
//          verbatim (see crowd_config_dta.h), resolved through the REAL
//          Scoring::GetCrowdConfig -> per-difficulty -> per-track lookup.
//   REAL : the chart — SongParser -> SongData -> GameGemList, and the real
//          Player/Performer/Band/Scoring/Stats graph, with Performer::Poll
//          feeding mCrowd->Poll(songFraction) off the advancing clock.
//   DRIVER: the ~15-line note ARBITER that GemPlayer::UpdateCrowdMeter would
//          normally supply (which reward/penalty multiplier a given note earns).
//          GemPlayer.cpp is not linked here — it needs mFill/mDrumSlotWeights/
//          mUser/mTrackNum GemPlayer state and drags the TrackPanel/BandTrack
//          render cascade. The multiplier is still computed from the REAL
//          TheScoring->mCommonPhraseReward / mCommonPhrasePenalty and the REAL
//          Band::EnergyCrowdBoost(); only the branch policy is transcribed.
//   DRIVER: the hit/miss policy (deterministic, so the two scenarios are
//          reproducible).
//
// THE PROOF THAT IT IS NOT FAKING: the same chart is run twice with two hit
// policies. A stub returns a constant, so it cannot produce two different
// trajectories. A strong performance must drive the meter UP to kExcitementPeak;
// a weak one must drive it DOWN through the warning level and below lose_level
// (band failure). Both are printed with the real numbers.
#include "game/Player.h"
#include "game/Performer.h"
#include "game/Scoring.h"
#include "game/Stats.h"
#include "game/Band.h"
#include "game/PlayerBehavior.h"
#include "game/CommonPhraseCapturer.h"
#include "game/Game.h"
#include "game/SongDB.h"
#include "game/CrowdRating.h"
#include "game/MultiplayerAnalyzer.h" // PlayerScoreInfo

#include "beatmatch/TrackType.h"
#include "beatmatch/SongParser.h"
#include "beatmatch/SongData.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/Phrase.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/InternalSongParserSink.h"
#include "beatmatch/TuningOffsetList.h"
#include "utl/BeatMap.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongInfoAudioType.h"
#include "utl/TempoMap.h"
#include "utl/MeasureMap.h"
#include "utl/SongPos.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"

#include "crowd_config_dta.h" // the REAL shipped (crowd ...) block

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig;
void DataInit();

// ---- M8 support globals (native/src/m8_support.cpp) ----
extern float gNativeSongMs;
extern SongData *gM8SongData;
extern Player *gM8Player;
extern int gM8TrackNum;
extern float gM8DurationMs;
extern std::vector<int> gM8GemPhrase;
extern std::vector<bool> gM8Dealt;
extern std::vector<Player *> gM8ActivePlayers;
extern std::vector<PlayerScoreInfo> gM8BaseScores;

static const int kNDiff = 4;
static const int kExpertDiff = 3;

// Everything except the (crowd ...) block is identical to M8's config; the crowd
// block is spliced in at runtime from crowd_config_dta.h so the real shipped
// values drive Scoring::GetCrowdConfig.
static const char *kConfigHead =
    "(beatmatcher"
    "   (parser"
    "      (player_slot 9)"
    "      (low_vocal_pitch 36)(high_vocal_pitch 84)"
    "      (keyboard_range_shift_duration_ms 100.0)"
    "      (track_mapping"
    "         (DRUMS  0 0 'PART DRUMS')(BASS 3 2 'PART BASS')"
    "         (GUITAR 4 1 'PART GUITAR')(VOCALS 6 3 'PART VOCALS')"
    "         (KEYS 9 4 'PART KEYS'))"
    "   )"
    "   (controllers (beatmatch_controller_mapping (guitar guitar)))"
    "   (audio (submixes))"
    ")"
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
    "         (guitar (0 1)(10 2)(20 3)(30 4))"
    "         (bass   (0 1)(10 2)(20 3)(30 4)(40 5)(50 6))"
    "         (drum   (0 1)(10 2)(20 3)(30 4))"
    "         (keys   (0 1)(10 2)(20 3)(30 4))"
    "         (vocals (0 1)(10 2)(20 3)(30 4))"
    "         (default (0 1)(10 2)(20 3)(30 4)))"
    "      (energy (default (0 1))))"
    "   (overdrive"
    "      (recharge_rate 0.0)(star_phrase 0.25)(common_phrase 0.15)"
    "      (fill_boost 0.35)(whammy_rate 3.4e-2)(ready_level 0.5)"
    "      (multiplier 2)(crowd_boost 6))"
    "   (band_energy"
    "      (deploy_beats 32)(deploy_bonus 50)(spotlight_phrase 0.251)"
    "      (unison_phrase 0.501)(deploy_threshold 0.5)(save_energy 0.5))"
    "   (bonuses"
    "      (max_bonus 4)(multiplier (1 2 4 6 8))(crowd_boost (1 6 6 6 6)))"
    "   (unison_phrase (reward 2.0)(penalty 2.0)(point_bonus 1000))"
    "   (track_graphics (popup_help_intro_duration_ms 5000.0))"
    "   (star_ratings"
    "      (new_instrument_thresholds"
    "         (guitar 6.0e-2 0.12 0.2 0.47 0.78 1.15)"
    "         (bass 5.0e-2 0.1 0.19 0.47 0.78 1.15)"
    "         (drum 6.0e-2 0.12 0.2 0.45 0.75 1.09)"
    "         (vocals 5.0e-2 0.11 0.19 0.46 0.77 1.06)"
    "         (keys 6.0e-2 0.12 0.2 0.47 0.78 1.15)"
    "         (real_guitar 6.0e-2 0.12 0.2 0.47 0.78 1.15)"
    "         (real_bass 5.0e-2 0.1 0.19 0.47 0.78 1.15)"
    "         (real_keys 6.0e-2 0.12 0.2 0.47 0.78 1.15))"
    "      (new_num_instruments_multiplier 1.0 1.26 1.52 1.8 1.8)"
    "      (new_bonus_thresholds 5.0e-2 0.1 0.2 0.3 0.4 0.95 1.0))";

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

class NativeCrowdPlayer : public Player {
public:
    NativeCrowdPlayer(TrackType ty, Symbol streakType, int maxMult, Band *band)
        : Player(nullptr, band, 0, nullptr, true), mGemCounter(0) {
        mTrackType = ty;
        mBehavior->SetStreakType(streakType);
        mBehavior->SetMaxMultiplier(maxMult);
        NativeInitParams();
        unk2b0 = true;
        // REAL ctor now: CrowdRating::CrowdRating -> Configure() reads the real
        // shipped crowd config through Scoring::GetCrowdConfig, then Reset().
        mCrowd = new CrowdRating(nullptr, (Difficulty)kExpertDiff);
    }

    void ApplyHit(int numSlots) {
        BuildHitStreak(mGemCounter, 0.0f);
        EndMissStreak();
        int hp = TheScoring->GetHeadPoints(mTrackType);
        int cp = TheScoring->GetChordPoints(mTrackType);
        int pts = (numSlots == 1) ? numSlots * hp : (cp < 0 ? numSlots * hp : cp);
        AddPoints((float)pts, true, true);
        mStats.AddAccuracy(pts);
        mGemCounter++;
    }
    void ApplyPass() {
        EndHitStreak();
        BuildMissStreak(mGemCounter);
        mGemCounter++;
    }

    // The note ARBITER, transcribed from GemPlayer::UpdateCrowdMeter
    // (src/band3/game/GemPlayer.cpp:1960-2016) with the GemPlayer-only state
    // (mFill / mDrumSlotWeights / solo-mod flags) dropped. Every VALUE it uses is
    // real: Band::EnergyCrowdBoost() and Scoring::mCommonPhraseReward /
    // mCommonPhrasePenalty. The CrowdRating::Update call itself is real engine code.
    void FeedCrowdNote(float noteScore, bool inCommonPhrase) {
        if (!mCrowd->mActive)
            return;
        float multiplier = 1.0f;
        if (noteScore > mCrowd->mRawValue) {
            float reward = GetCrowdBoost(); // REAL: Band::EnergyCrowdBoost()
            if (inCommonPhrase)
                reward *= TheScoring->mCommonPhraseReward; // REAL
            multiplier = reward;
        } else if (inCommonPhrase) {
            multiplier *= TheScoring->mCommonPhrasePenalty; // REAL
        }
        mCrowd->Update(noteScore, multiplier); // REAL engine computation
    }

    int ScoreI() const { return GetScore(); }
    int Streak() const { return mStats.GetCurrentStreak(); }
    int NotesHit() const { return mStats.GetHitCount(); }

    bool PastFinalNote() const { return false; }
    float GetNotesHitFraction(bool *) const { return 0.0f; }
    bool IsReady() const { return true; }
    void Start() {}
    void PollTrack() {}
    void PollAudio() {}
    void SetPaused(bool) {}
    void SetRealtime(bool) {}
    void SetMusicSpeed(float) {}
    void Jump(float, bool) {}
    void SetAutoplay(bool) {}
    bool IsAutoplay() const { return false; }
    void HookupTrack() {}
    void UnHookTrack() {}
    void EnableFills(float, bool) {}
    void DisableFills() {}
    bool FillsEnabled(int) { return false; }
    bool DoneWithSong() const { return false; }
    int GetBaseMaxPoints() const { return 0; }
    int GetBaseMaxStreakPoints() const { return 0; }
    int GetBaseBonusPoints() const { return 0; }
    void HandleNewSection(const PracticeSection &, int, int) {}

    int mGemCounter;
};

static const char *TrackName(TrackType t) {
    switch (t) {
    case kTrackDrum: return "drum";
    case kTrackGuitar: return "guitar";
    case kTrackBass: return "bass";
    default: return "?";
    }
}

static const char *ExcitementName(ExcitementLevel e) {
    switch (e) {
    case kExcitementBoot: return "BOOT(failing)";
    case kExcitementBad: return "BAD";
    case kExcitementOkay: return "OKAY";
    case kExcitementGreat: return "GREAT";
    case kExcitementPeak: return "PEAK";
    default: return "?";
    }
}

struct Chart {
    std::vector<float> gemMs;
    std::vector<int> gemSlots;
    std::vector<int> gemPhrase;
    int nGems = 0, nPhrases = 0, track = 0;
    TrackType trackTy = kTrackNone;
    SongData *data = nullptr;
};

// One full clock-driven pass over the chart with a given hit policy, watching the
// REAL crowd meter. hitEvery==1 => hit everything; hitEvery==3 => hit 1 in 3.
static void RunScenario(const char *label, const Chart &ch, int hitEvery,
                        int maxMult) {
    printf("\n================ scenario: %s (hit 1 in %d) ================\n",
           label, hitEvery);

    Band *band = new Band(true, 1, true);
    band->NativeLoadBonuses();
    NativeCrowdPlayer player(ch.trackTy, Symbol(TrackName(ch.trackTy)), maxMult, band);
    band->mActivePlayers.push_back(&player);
    gM8Player = &player;
    gM8ActivePlayers.clear();
    gM8ActivePlayers.push_back(&player);
    band->mCommonPhraseCapturer = new CommonPhraseCapturer();

    CrowdRating *crowd = player.Crowd();
    printf("  REAL config for difficulty=Expert(3), track '%s':\n",
           TrackName(ch.trackTy));
    printf("    range=[%.3f %.3f] note_weight=%.4f phrase_weight=%.3f\n",
           crowd->kMin, crowd->kMax, crowd->kNoteWeight, crowd->kPhraseWeight);
    printf("    lose_level=%.4f warning=%.3f bad=%.3f okay=%.3f great=%.3f "
           "free_ride=%.3f\n", crowd->GetLoseLevel(), crowd->kWarningLevel,
           crowd->kBadLevel, crowd->kOkayLevel, crowd->kGreatLevel, crowd->kFreeRide);
    printf("    initial_display_level=%.4f -> start value %.4f (display %.4f), "
           "excitement %s\n\n", crowd->kInitialDisplayLevel, crowd->GetValue(),
           crowd->GetDisplayValue(), ExcitementName(crowd->GetExcitement()));

    const float dt = 1000.0f / 60.0f;
    const float timelineEvery = 15000.0f;
    float nextTimeline = 0.0f;
    int nextGem = 0, hits = 0, total = 0;
    ExcitementLevel lastLevel = crowd->GetExcitement();
    bool everWarned = false, everFailed = false;
    float failedAtMs = -1.0f, minValue = 1.0f;

    printf("  %8s %10s %8s %9s %9s %-14s\n", "songMs", "score", "crowd",
           "display", "raw", "excitement");

    for (gNativeSongMs = 0.0f; gNativeSongMs <= gM8DurationMs; gNativeSongMs += dt) {
        while (nextGem < ch.nGems && ch.gemMs[nextGem] <= gNativeSongMs) {
            int g = nextGem++;
            bool hit = (g % hitEvery) == 0;
            gM8Dealt[g] = true;
            total++;
            if (hit) {
                player.ApplyHit(ch.gemSlots[g] > 0 ? ch.gemSlots[g] : 1);
                hits++;
            } else {
                player.ApplyPass();
            }
            // REAL crowd response to this note.
            player.FeedCrowdNote(hit ? 1.0f : 0.0f, ch.gemPhrase[g] != -1);
            if (ch.gemPhrase[g] != -1)
                band->mCommonPhraseCapturer->HandlePhraseNote(
                    (GemPlayer *)&player, ch.track, g, hit);
        }

        SongPos pos = ch.data->CalcSongPos(gNativeSongMs);
        player.Poll(gNativeSongMs, pos); // REAL -> Performer::Poll -> mCrowd->Poll(frac)

        if (crowd->GetValue() < minValue)
            minValue = crowd->GetValue();

        ExcitementLevel lvl = crowd->GetExcitement();
        if (lvl != lastLevel) {
            printf("  >>> %8.0f  excitement %s -> %s   (crowd %.4f)\n",
                   gNativeSongMs, ExcitementName(lastLevel), ExcitementName(lvl),
                   crowd->GetValue());
            lastLevel = lvl;
        }
        if (crowd->IsInWarning() && !everWarned) {
            everWarned = true;
            printf("  !!! %8.0f  CROWD WARNING (value %.4f < warning_level %.4f)\n",
                   gNativeSongMs, crowd->GetValue(), crowd->kWarningLevel);
        }
        if (crowd->IsBelowLoseLevel() && !everFailed) {
            everFailed = true;
            failedAtMs = gNativeSongMs;
            printf("  *** %8.0f  BAND FAILED — crowd %.5f < lose_level %.5f "
                   "(CantFailYet=%s)\n", gNativeSongMs, crowd->GetValue(),
                   crowd->GetLoseLevel(), crowd->CantFailYet() ? "yes" : "no");
        }

        if (gNativeSongMs >= nextTimeline) {
            printf("  %8.0f %10d %8.4f %9.4f %9.4f %-14s\n", gNativeSongMs,
                   player.ScoreI(), crowd->GetValue(), crowd->GetDisplayValue(),
                   crowd->GetRawValue(), ExcitementName(crowd->GetExcitement()));
            nextTimeline += timelineEvery;
        }
    }

    printf("\n  --- %s result ---\n", label);
    printf("    notes hit      : %d / %d (%.1f%%)\n", hits, total,
           total ? 100.0f * hits / total : 0.0f);
    printf("    score          : %d\n", player.ScoreI());
    printf("    final crowd    : %.5f  (display %.4f, raw %.4f)\n",
           crowd->GetValue(), crowd->GetDisplayValue(), crowd->GetRawValue());
    printf("    running min    : %.5f  (real CrowdRating::GetMinValue)\n",
           crowd->GetMinValue());
    printf("    final excitement: %s\n", ExcitementName(crowd->GetExcitement()));
    printf("    ever in warning: %s\n", everWarned ? "YES" : "no");
    if (everFailed)
        printf("    BAND FAILED    : YES, at %.0f ms (%.1f s)\n", failedAtMs,
               failedAtMs / 1000.0f);
    else
        printf("    BAND FAILED    : no (stayed above lose_level all song)\n");
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-grinnz/tool/vicarious/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;
    const char *partName = (argc >= 3) ? argv[2] : "PART DRUMS";

    setvbuf(stdout, nullptr, _IONBF, 0);
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    // Splice the REAL shipped (crowd ...) block into the (scoring ...) section.
    std::string cfg(kConfigHead);
    cfg += kRealCrowdConfigDta;
    cfg += ")";
    gSystemConfig = DataReadString(cfg.c_str());
    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    printf("=== rb3-xenon native M12: the REAL crowd meter / band failure ===\n");
    printf("mid : %s\npart: %s\n", midPath, partName);
    printf("crowd config: REAL RB3 config/scoring.dta (crowd ...) block, "
           "difficulty macros expanded\n\n");

    Scoring *scoring = new Scoring();

    NativeSongInfo songInfo;
    SongData songData;
    songData.mNumDifficulties = kNDiff;
    songData.mHopoThreshold = songInfo.GetHopoThreshold();
    songData.mSongInfo = &songInfo;
    songData.mDetailedGrid = false;
    songData.mBeatMap = new BeatMap();
    SetTheBeatMap(songData.mBeatMap);
    songData.mPhraseAnalyzer = new PhraseAnalyzer(&songData);
    songData.mTuningOffsetList = new TuningOffsetList();
    songData.mKeyboardRangeSections.resize(kNDiff);

    TempoMap *tempoMap = nullptr;
    MeasureMap *measureMap = nullptr;
    {
        FileStream fs(midPath, FileStream::kRead, false);
        SongParser parser(songData, kNDiff, tempoMap, measureMap, 2);
        parser.ReadMidiFile(fs, midPath, &songInfo);
        int pumps = 0;
        while (!parser.NoMidiReader() && pumps < 2000000) {
            parser.Poll();
            pumps++;
        }
        printf("--- Stage 1: REAL SongParser -> SongData (%d Poll pumps) ---\n", pumps);
    }
    songData.mTempoMap = tempoMap;
    songData.mMeasureMap = measureMap;
    for (size_t i = 0; i < songData.mGemDBs.size(); i++) {
        songData.mGemDBs[i]->MergeChordGems();
        songData.mGemDBs[i]->Finalize();
    }
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    songData.mPlayerTrackConfigList = &expertList;
    songData.FixUpTrackConfig(&expertList);
    songData.SetUpTrackDifficulties(&expertList);

    int track = songData.TrackNamed(Symbol(partName));
    if (track == -1) { printf("  track '%s' not found; abort.\n", partName); return 1; }

    Chart ch;
    ch.track = track;
    ch.trackTy = songData.TrackTypeAt(track);
    ch.data = &songData;
    GameGemList *gems = songData.GetGemList(track);
    ch.nGems = gems->NumGems();
    const PhraseList &odPhrases = songData.GetPhraseList(track, kCommonPhrase);
    ch.nPhrases = (int)odPhrases.mPhrases.size();
    if (ch.nGems == 0) { printf("  no gems on this track; abort.\n"); return 1; }

    ch.gemMs.resize(ch.nGems);
    ch.gemSlots.resize(ch.nGems);
    std::vector<int> gemTick(ch.nGems);
    float lastMs = 0.0f;
    for (int g = 0; g < ch.nGems; g++) {
        const GameGem &gm = gems->GetGem(g);
        gemTick[g] = gm.GetTick();
        ch.gemSlots[g] = gm.NumSlots();
        ch.gemMs[g] = songData.GetTempoMap()->TickToTime(gm.GetTick());
        if (ch.gemMs[g] > lastMs) lastMs = ch.gemMs[g];
    }
    ch.gemPhrase.assign(ch.nGems, -1);
    for (int p = 0; p < ch.nPhrases; p++) {
        const Phrase &ph = odPhrases.mPhrases[p];
        int s = ph.GetTick(), e = ph.GetTick() + ph.GetDurationTicks();
        for (int g = 0; g < ch.nGems; g++)
            if (gemTick[g] >= s && gemTick[g] < e) ch.gemPhrase[g] = p;
    }
    gM8GemPhrase = ch.gemPhrase;
    gM8Dealt.assign(ch.nGems, false);
    gM8DurationMs = lastMs + 3000.0f;
    gM8SongData = &songData;
    gM8TrackNum = track;

    printf("  track %d '%s' (type %d): %d Expert gems, %d OD phrase(s), "
           "duration ~%.1f s\n", track, songData.TrackName(track).Str(),
           (int)ch.trackTy, ch.nGems, ch.nPhrases, gM8DurationMs / 1000.0f);

    Game *game = (Game *)std::calloc(1, sizeof(Game));
    game->mProperties.mEnableStreak = true;
    game->mProperties.mEnableOverdrive = true;
    game->mProperties.mAllowOverdrivePhrases = true;
    game->mProperties.mEndWithSong = false;
    game->unkdc = -1.0f;
    TheGame = game;

    SongDB *songDB = new SongDB();
    songDB->mSongData = &songData;
    songDB->mSongDurationMs = gM8DurationMs;
    TheSongDB = songDB;

    gM8BaseScores.clear();
    gM8BaseScores.push_back(
        PlayerScoreInfo(ch.trackTy, (Difficulty)kExpertDiff, 100000, 100000, 0));
    scoring->ComputeStarThresholds(false);

    int maxMult =
        (ch.trackTy == kTrackBass || ch.trackTy == kTrackRealBass) ? 6 : 4;

    // Two passes over the SAME chart. A stubbed CrowdRating cannot produce two
    // different trajectories — that is the point.
    gM8Dealt.assign(ch.nGems, false);
    RunScenario("STRONG performance", ch, 1, maxMult);
    gM8Dealt.assign(ch.nGems, false);
    RunScenario("WEAK performance", ch, 3, maxMult);

    printf("\n=== M12 complete: the crowd meter above is REAL engine code "
           "(src/band3/game/CrowdRating.cpp)\n");
    printf("    running on REAL shipped RB3 tuning values. Previously this whole "
           "subsystem\n    was a no-op stub called 25,905x per run.\n");
    return 0;
}
