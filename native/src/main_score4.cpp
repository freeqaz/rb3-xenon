// rb3-xenon native M8 — the full-song run-through.
//
// A single driver that plays an entire real song start-to-finish through the
// REAL engine loop. Unlike M7 (main_score3), which exercised the overdrive cycle
// by hand-calling each transition per scenario, M8 is driven by an ADVANCING
// CLOCK: every frame the headless-audio clock ticks, the REAL SongData::CalcSong-
// Pos turns it into a SongPos, and the REAL Player::Poll(ms, pos) re-evaluates
// everything — streak multiplier, band energy, the beat-accurate overdrive drain.
// Gems are judged by a deterministic scripted input policy as the clock passes
// their times; completing an overdrive phrase credits energy through the REAL,
// PORTED CommonPhraseCapturer; deploy fires the moment the REAL CanDeployOverdrive
// gate trips mid-song; energy then drains on the beat until expiry — all off the
// one clock, no per-scenario calls.
//
// REAL vs SHIM census (see the report / m8_support.cpp / m8_link_stubs.cpp):
//   REAL : SongParser->SongData chart, PhraseList (note 116 -> kCommonPhrase),
//          Scoring/Stats/Player/Performer/Band scoring+energy graph, PlayerParams,
//          Band bonuses table, Player::Poll / UpdateEnergy(beat-drain) / Perform-
//          DeployBandEnergy / Deploy / StopDeployingBandEnergy bookkeeping,
//          CommonPhraseCapturer (the real arbiter, driving real Complete-
//          CommonPhrase credit), SongData::CalcSongPos clock->SongPos,
//          CrowdRating (real since W16-PD; no driver arbiter feeds Update(), so
//          the meter only sees Reset/Poll/phrase traffic -- rb3-crowd is the
//          target that drives it with a note arbiter).
//   SHIM : the audio clock source (no synth/stream — headless),
//          TrackPanel/BandTrack/OverdriveMeter render leaves,
//          net message fan-out, BandPerformer aggregation (multi-player = M9).
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
#include "crowd_config_dta.h" // W16-PD: real shipped (crowd ...) block
#include "scoring_config_dta.h" // W16-PJ: real shipped (solo ...) + (coda ...)
#include "game/GameConfig.h"
#include <string>
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig;
void DataInit();

// ---- M8 support (native/src/m8_support.cpp) ----
extern float gNativeSongMs;
extern std::vector<bool> gM8Dealt;
Game *NativeMakeGame();
GameConfig *NativeMakeGameConfig(PlayerTrackConfigList *, float);
void NativeSongDBPostLoad(SongDB *, float);

static const int kNDiff = 4;
static const int kExpertDiff = 3;

// Combined SystemConfig — identical to M7 (real retail scoring.dta numbers).
static const char *kConfigDta =
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
    // W16-PD: the REAL shipped (crowd ...) block (crowd_config_dta.h) is spliced
    // in at @CROWD@ at startup -- the real CrowdRating::Configure reads it. The
    // three save/brink keys this used to hand-write (0.3 / 2.0 / 0.1) are in that
    // block with their shipped values (0.8333 / 3.5 / 0.04).
    "   @CROWD@"
    "   (track_graphics (popup_help_intro_duration_ms 5000.0))"
    // -- REAL star thresholds (config/star_thresholds.dta) so GetNumStars is real
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
    "      (new_bonus_thresholds 5.0e-2 0.1 0.2 0.3 0.4 0.95 1.0))"
    ")";

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

// Concrete Player over the real scoring/energy graph (M7's surface). Player::Poll
// / UpdateEnergy / PerformDeployBandEnergy etc. are the REAL (non-overridden)
// virtuals; only the pure I/O / render / audio virtuals are shimmed here.
class NativeScorePlayer : public Player {
public:
    NativeScorePlayer(TrackType ty, Symbol streakType, int maxMult, Band *band)
        : Player(nullptr, band, 0, nullptr, true), mGemCounter(0) {
        mTrackType = ty;
        mBehavior->SetStreakType(streakType);
        mBehavior->SetMaxMultiplier(maxMult);
        NativeInitParams();
        unk2b0 = true;                              // overdrive enabled
        mCrowd = new CrowdRating(nullptr, (Difficulty)kExpertDiff); // REAL CrowdRating (W16-PD)
    }

    // Scoring verdicts (real): mirror GemPlayer::Hit / Pass.
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

    int ScoreI() const { return GetScore(); }
    float Energy() const { return mBandEnergy; }
    bool CanDeploy() const { return CanDeployOverdrive(); }
    int TotalMult() const { int a = 1, b = 1, c = 1; return GetMultiplier(true, a, b, c); }
    int Streak() const { return mStats.GetCurrentStreak(); }
    int LongestStreak() const { return mStats.GetLongestStreak(); }
    int NotesHit() const { return mStats.GetHitCount(); }

    // Pure-virtual shim surface (I/O / render / audio).
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

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-grinnz/tool/vicarious/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;
    const char *partName = (argc >= 3) ? argv[2] : "PART DRUMS";

    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered so a crash never eats output
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    {
        std::string cfg(kConfigDta);
        cfg.replace(cfg.find("@CROWD@"), 7,
                    std::string(kRealCrowdConfigDta) + kRealSoloConfigDta
                        + kRealCodaConfigDta);
        gSystemConfig = DataReadString(cfg.c_str());
    }
    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    printf("=== rb3-xenon native M8: full-song run-through (clock-driven) ===\n");
    printf("mid : %s\npart: %s\n\n", midPath, partName);

    Scoring *scoring = new Scoring();

    // TheGame first: the REAL SongDB sink (AddPhrase) consults it during
    // SongData::PostLoad below.
    Game *game = NativeMakeGame();

    // --- Stage 1: parse the .mid into the REAL SongDB's SongData ------------
    // W16-PJ: the real SongDB ctor news its SongData, registers itself as that
    // SongData's parser sink and builds its MultiplayerAnalyzer over it; the
    // chart is parsed INTO it (as BeatMaster does for TheSongDB in the game).
    SongDB *songDB = new SongDB();
    TheSongDB = songDB;
    NativeSongInfo songInfo;
    SongData &songData = *songDB->GetData();
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
    for (size_t i = 0; i < songData.mGemDBs.size(); i++)
        songData.mGemDBs[i]->MergeChordGems();

    int track = songData.TrackNamed(Symbol(partName));
    if (track == -1) { printf("  track '%s' not found; abort.\n", partName); return 1; }
    TrackType trackTy = songData.TrackTypeAt(track);

    // W16-PJ: one REAL player config (a generated UserGuid on this track's type at
    // Expert), so PlayerTrackConfigList::TrackUsed / TrackPlayer answer as they
    // do for a one-player game -- the real SongDB::AddPhrase keeps a track's
    // overdrive phrases only if TrackUsed(track), and the real
    // MultiplayerAnalyzer keys every per-player total on that UserGuid.
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    UserGuid playerGuid;
    playerGuid.Generate();
    expertList.AddConfig(playerGuid, trackTy, kExpertDiff, 0, false);
    songData.mPlayerTrackConfigList = &expertList;
    NativeMakeGameConfig(&expertList, 0.0f);
    // The REAL SongData::PostLoad (FixUpTrackConfig + SetUpTrackDifficulties +
    // gem Finalize + backup tracks + per-track PostLoadTrack -> SongDB sink +
    // PhraseAnalyzer::Analyze + PostLoadVocals) -- what SongData::Poll runs when
    // the parse completes. Replaces the driver's hand-picked subset.
    songData.PostLoad(&expertList);
    if (expertList.GetTrackNumByUserGuid(playerGuid) != track) {
        printf("  config resolved track %d, expected %d; abort.\n",
               expertList.GetTrackNumByUserGuid(playerGuid), track);
        return 1;
    }
    GameGemList *gems = songData.GetGemList(track);
    int nGems = gems->NumGems();
    const PhraseList &odPhrases = songData.GetPhraseList(track, kCommonPhrase);
    int nPhrases = (int)odPhrases.mPhrases.size();

    // --- gem times + gem->OD-phrase map + song duration --------------------
    std::vector<float> gemMs(nGems);
    std::vector<int> gemTick(nGems), gemSlots(nGems);
    float lastMs = 0.0f;
    for (int g = 0; g < nGems; g++) {
        const GameGem &gm = gems->GetGem(g);
        gemTick[g] = gm.GetTick();
        gemSlots[g] = gm.NumSlots();
        gemMs[g] = songData.GetTempoMap()->TickToTime(gm.GetTick());
        if (gemMs[g] > lastMs) lastMs = gemMs[g];
    }
    gM8Dealt.assign(nGems, false);
    // Song end = last note + tail. (The real SongDB::ParseEvents takes it from
    // the chart's [end] text event, which needs BeatMaster's MIDI event list.)
    float gM8DurationMs = lastMs + 3000.0f;

    printf("  track %d '%s' (type %d): %d Expert gems, %d OD phrase(s), "
           "duration ~%.1f s\n\n", track, songData.TrackName(track).Str(),
           (int)trackTy, nGems, nPhrases, gM8DurationMs / 1000.0f);
    if (nGems == 0) { printf("  no gems on this track; abort.\n"); return 1; }

    // --- REAL Band + Player + wired singletons -----------------------------
    Band *band = new Band(true, 1, true);
    band->NativeLoadBonuses();

    int maxMult = (trackTy == kTrackBass || trackTy == kTrackRealBass) ? 6 : 4;
    NativeScorePlayer player(trackTy, Symbol(TrackName(trackTy)), maxMult, band);
    player.SetTrackNum(track);
    band->mActivePlayers.push_back(&player);
    // The real Game::GetPlayerFromTrack / GetScoringTracks read this.
    game->mAllActivePlayers.push_back(&player);

    // The REAL, ported overdrive-phrase arbiter (drives energy credit).
    band->mCommonPhraseCapturer = new CommonPhraseCapturer();

    // --- REAL base scores -> REAL star thresholds ---------------------------
    // W16-PJ: the event-independent half of the real SongDB::PostLoad
    // (SetupPhrases / DisableCodaGems / RunMultiplayerAnalyzer). The real
    // MultiplayerAnalyzer computes the per-player base scores from the gems and
    // its PostLoad runs Scoring::ComputeStarThresholds over them -- this replaces
    // the driver's own ideal-max that used to be pushed into a synthetic
    // GetBaseScores(). The driver ideal (100% hits, no deploy, through the real
    // scoring path) is still computed below, as a cross-check only.
    NativeSongDBPostLoad(songDB, gM8DurationMs);
    int idealMax = 0;
    {
        Band *ib = new Band(true, 1, true);
        ib->NativeLoadBonuses();
        NativeScorePlayer ideal(trackTy, Symbol(TrackName(trackTy)), maxMult, ib);
        for (int g = 0; g < nGems; g++)
            ideal.ApplyHit(gemSlots[g] > 0 ? gemSlots[g] : 1);
        idealMax = ideal.ScoreI();
    }
    {
        const std::vector<PlayerScoreInfo> &bs = songDB->GetBaseScores();
        int nUnisonIds = 0, nTrackUnisonIds = 0;
        for (int id = 0; id < songDB->NumCommonPhrases(); id++) {
            if (!songDB->IsUnisonPhrase(id)) continue;
            nUnisonIds++;
            if (songDB->GetData()->GetPhraseAnalyzer()->GetPhraseTracks(id) & (1 << track))
                nTrackUnisonIds++;
        }
        printf("--- REAL SongDB: %d common phrase(s) on this track (%d unison with "
               "another scoring track); analyzer: %d phrase id(s) song-wide, %d "
               "unison, %d of them on this track ---\n",
               songDB->GetNumOverdrivePhrases(track),
               songDB->GetNumUnisonPhrases(track), songDB->NumCommonPhrases(),
               nUnisonIds, nTrackUnisonIds);
        for (size_t i = 0; i < bs.size(); i++)
            printf("  MultiplayerAnalyzer base: type %d diff %d maxStreakPts %d "
                   "maxPts %d bonusPts %d  (driver ideal %d)\n",
                   (int)bs[i].mTrackType, (int)bs[i].mDifficulty,
                   bs[i].mMaxStreakPts, bs[i].mMaxPts, bs[i].mBonusPts, idealMax);
        printf("\n");
    }

    printf("--- graph: NativeScorePlayer(%s) over real Player/Band/Scoring + ported "
           "CommonPhraseCapturer ---\n", TrackName(trackTy));
    printf("  PlayerParams: spotlightPhrase=%.3f deployThreshold=%.3f "
           "deployBeats=%.5f  bonuses multiplier[1]=x%d\n\n",
           player.mParams->mSpotlightPhrase, player.mParams->mDeployThreshold,
           player.mParams->mDeployBeats, band->unk68.size() > 1 ? band->unk68[1] : 0);

    // ============================ the clock loop ============================
    // Scripted input policy: hit every gem except every 20th (a deterministic 5%
    // miss), and force-miss one gem outside any OD phrase to show a streak break.
    // The whole run advances off gNativeSongMs; Player::Poll re-evaluates each
    // frame; overdrive credit / deploy / drain all fall out of the clock.
    const float dt = 1000.0f / 60.0f; // 60 fps frame step
    const float timelineEvery = 5000.0f;
    // Deterministic policy: miss every 40th gem (so clean stretches let the streak
    // multiplier climb to its x4 cap), plus one specific mid-chart burst (a 40-gem
    // window) to show a hard streak break + recovery. ~95% hit overall.
    int burstStart = nGems / 2;
    int burstEnd = burstStart + 40;

    printf("--- clock-driven run (dt=%.2fms, deterministic ~95%% hit policy) ---\n", dt);
    printf("  %8s %10s %7s %6s %8s %9s\n",
           "songMs", "score", "streak", "mult", "energy", "deployed");

    int nextGem = 0;
    int deployedCount = 0;
    bool everDeployed = false;
    float nextTimeline = 0.0f;
    int notesHitTotal = 0, notesTotal = 0;
    float peakEnergy = 0.0f;

    for (gNativeSongMs = 0.0f; gNativeSongMs <= gM8DurationMs; gNativeSongMs += dt) {
        // 1) judge every gem the clock has reached this frame (scripted policy)
        while (nextGem < nGems && gemMs[nextGem] <= gNativeSongMs) {
            int g = nextGem++;
            bool miss = (g % 40 == 39) || (g >= burstStart && g < burstEnd);
            gM8Dealt[g] = true; // dealt with (hit OR pass) before the capturer asks
            notesTotal++;
            if (miss) {
                player.ApplyPass();
            } else {
                player.ApplyHit(gemSlots[g] > 0 ? gemSlots[g] : 1);
                notesHitTotal++;
            }
            // 2) drive the REAL capturer for OD-phrase gems (credits energy on the
            //    last gem of a completed phrase, exactly as GemPlayer does).
            if (songDB->GetPhraseID(track, g) != -1) {
                band->mCommonPhraseCapturer->HandlePhraseNote(
                    (GemPlayer *)&player, track, g, !miss);
            }
        }

        // 3) SongPos from the clock (real) + the REAL per-frame Poll
        SongPos pos = songData.CalcSongPos(gNativeSongMs);
        player.Poll(gNativeSongMs, pos); // REAL: PollMultiplier/UpdateEnergy/Performer::Poll

        if (player.Energy() > peakEnergy) peakEnergy = player.Energy();

        // 4) deploy the moment the real gate trips (and re-deploy each time energy
        //    refills past the threshold) — every cycle is clock-driven.
        if (!player.mDeployingBandEnergy && player.CanDeploy() && gNativeSongMs > 1000.0f) {
            printf("  >>> %.0fms: energy %.3f >= threshold %.3f -> REAL deploy "
                   "(mult x%d -> ", gNativeSongMs, player.Energy(),
                   player.mParams->mDeployThreshold, player.TotalMult());
            player.PerformDeployBandEnergy(0, true); // REAL deploy transition+bookkeeping
            everDeployed = true;
            deployedCount++;
            printf("x%d, Band.mMultiplier=%d bonusLevel=%d)\n",
                   player.TotalMult(), band->mMultiplier, band->mBonusLevel);
        }

        // 5) periodic timeline
        if (gNativeSongMs >= nextTimeline) {
            printf("  %8.0f %10d %7d %6d %8.4f %9s\n",
                   gNativeSongMs, player.ScoreI(), player.Streak(),
                   player.TotalMult(), player.Energy(),
                   player.mDeployingBandEnergy ? "YES" : "-");
            nextTimeline += timelineEvery;
        }
    }

    // ============================ final results =============================
    printf("\n--- SONG COMPLETE ---\n");
    int finalScore = player.ScoreI();
    int stars = player.GetNumStars();            // REAL Scoring solo-star rating
    float starsF = player.GetNumStarsFloat();
    printf("  total score   : %d  (ideal max %d, %.1f%% of ideal)\n", finalScore,
           idealMax, idealMax ? 100.0f * finalScore / idealMax : 0.0f);
    printf("  stars         : %d  (%.2f)  [6 = gold]\n", stars, starsF);
    printf("  longest streak: %d\n", player.LongestStreak());
    printf("  notes hit     : %d / %d  (%.1f%%)\n", notesHitTotal, notesTotal,
           notesTotal ? 100.0f * notesHitTotal / notesTotal : 0.0f);
    printf("  overdrive     : %d phrase(s) available, %d deploy(s), peak energy %.3f\n",
           nPhrases, deployedCount, peakEnergy);
    printf("\nDone.\n");
    return 0;
}
