// rb3-xenon native — M6: REAL GemPlayer/Performer/Player/Scoring/Stats scoring
// object graph.
//
// Where M5 (rb3-score / score_engine.cpp) faithfully *transcribed* the RB3
// scoring formulas into a standalone module, M6 constructs the genuine band3
// scoring object graph and runs the ACTUAL engine code unmodified:
//
//   Performer::AddPoints         (src/band3/game/Performer.cpp)  — real
//   Player::GetMultiplier /
//     Player::GetIndividualMultiplier / Player::AddPoints        — real
//   Scoring::GetStreakMult / GetHeadPoints / GetChordPoints      — real
//     (Scoring singleton parsed from the real scoring.dta layout)
//   Stats::BuildHitStreak / EndHitStreak / BuildMissStreak /
//     EndMissStreak / AddScoreStreak / AddOverdrive /
//     AddBandContribution / AddAccuracy                          — real
//
// GRAPH CENSUS (real vs shim):
//   * Scoring (TheScoring)  — REAL, real config.        (self-contained)
//   * Stats (mStats)        — REAL, default ctor.       (owned by Performer)
//   * PlayerBehavior        — REAL, cfg via SetStreakType/SetMaxMultiplier,
//                             exactly as GemPlayer::SetupBehavior.
//   * Band (mBand)          — SHIM (native gated ctor): carries only the
//                             EnergyMultiplier state Player::GetMultiplier reads
//                             (mMultiplier). The retail Band ctor recursively
//                             builds players + BandPerformer + CommonPhrase-
//                             Capturer + parses scoring/bonuses — a whole
//                             foreign domain, so it is shimmed to a leaf.
//   * CrowdRating (mCrowd)  — SHIM: null. No in-tree impl; crowd meter is a
//                             visual/audio leaf.
//   * BandUser (mUser)      — SHIM: null. The scoring path never dereferences
//                             it; the per-instrument config it would supply is
//                             set directly on PlayerBehavior (as the engine's
//                             SetupBehavior does from mUser->GetTrackSym()).
//   * TheGame/net/UI msg    — SHIM: the send_streak / send_update_score / p0_hit
//                             broadcasts are gated off (unk1fd/unk1fe = 0); the
//                             headless scorer has no net session or GamePanel.
//   * GemPlayer::AddHeadPoints — its ~5-line point SELECTION is inlined here
//                             (the retail method looks the gem up through the
//                             game SongDB, an M7 subsystem). Everything it hands
//                             to — GetHeadPoints/GetChordPoints, AddPoints,
//                             AddAccuracy — is the real engine.
//
// The NativeScorePlayer below is an honest concrete subclass of the REAL,
// abstract Player: it supplies no-op bodies for the ~22 pure virtuals that are
// pure I/O / rendering / audio surface (Start, PollTrack, Jump, HookupTrack,
// GetBaseMaxPoints, ...) and otherwise inherits every real scoring method.

#include "game/Player.h"
#include "game/Performer.h"
#include "game/Scoring.h"
#include "game/Stats.h"
#include "game/Band.h"
#include "game/PlayerBehavior.h"
#include "beatmatch/TrackType.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "utl/Symbol.h"

#include "score_engine.h" // M5 transcription, for the cross-check

#include <cstdio>
#include <vector>
#include <string>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp

// ---------------------------------------------------------------------------
// The real scoring.dta layout the Scoring() ctor parses via SystemConfig(
// "scoring"). Values are the retail config (points / streaks / overdrive) plus
// the unison_phrase block the ctor requires. Identical to the block M5's
// ScoreConfig::Load reads through the same DataArray API.
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
    "         (guitar       (0 1)(10 2)(20 3)(30 4))"
    "         (bass         (0 1)(10 2)(20 3)(30 4)(40 5)(50 6))"
    "         (drum         (0 1)(10 2)(20 3)(30 4))"
    "         (keys         (0 1)(10 2)(20 3)(30 4))"
    "         (vocals       (0 1)(10 2)(20 3)(30 4))"
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
    "   (unison_phrase (reward 0.5)(penalty 0.5))"
    ")";

// ===========================================================================
// NativeScorePlayer — concrete subclass of the real Player. Only the pure
// virtuals (all pure I/O / render / audio surface) get shim bodies; every
// scoring method (AddPoints, GetMultiplier, GetIndividualMultiplier,
// BuildHitStreak, ...) is the inherited REAL engine code.
// ===========================================================================
class NativeScorePlayer : public Player {
public:
    NativeScorePlayer(TrackType ty, Symbol streakType, int maxMult, Band *band)
        : Player(/*user*/ nullptr, band, /*tracknum*/ 0, /*bmaster*/ nullptr,
                 /*native_score_tag*/ true),
          mGemCounter(0) {
        mTrackType = ty;
        // Exactly GemPlayer::SetupBehavior: streak type from the track symbol,
        // max multiplier from the instrument (bass/real_bass 6, else 4).
        mBehavior->SetStreakType(streakType);
        mBehavior->SetMaxMultiplier(maxMult);
    }

    // === The real scoring chain, driven per beat-match verdict. ============
    // GemPlayer::Hit scoring core: BuildHitStreak; EndMissStreak; AddHeadPoints
    // (-> AddPoints). numSlots = frets in the gem (1 = single note).
    void OnHit(int numSlots) {
        BuildHitStreak(mGemCounter, 0.0f); // Performer::BuildHitStreak -> mStats
        EndMissStreak();                   // Performer::EndMissStreak -> mStats
        int hp = TheScoring->GetHeadPoints(mTrackType); // real
        int cp = TheScoring->GetChordPoints(mTrackType); // real
        // GemPlayer::AddHeadPoints point selection (guitar/bass, no cymbal):
        int pts = (numSlots == 1) ? numSlots * hp : (cp < 0 ? numSlots * hp : cp);
        AddPoints((float)pts, true, true); // Player::AddPoints -> Performer::AddPoints
        mStats.AddAccuracy(pts);           // real Stats
        mGemCounter++;
    }
    // GemPlayer::Miss -> BuildMissStreak.
    void OnMiss() { BuildMissStreak(mGemCounter); }
    // GemPlayer::Pass -> EndHitStreak (streak -> 0) + BuildMissStreak.
    void OnPass() {
        EndHitStreak();
        BuildMissStreak(mGemCounter);
    }

    // Deploy band energy: exercises the REAL Player::GetMultiplier band/OD arm
    // (mDeployingBandEnergy + mBand->EnergyMultiplier()).
    void DeployBand() {
        mDeployingBandEnergy = true;
        static_cast<Band *>(mBand)->NativeSetMultiplier(2); // band energy x2
    }
    void EndDeployBand() {
        mDeployingBandEnergy = false;
        static_cast<Band *>(mBand)->NativeSetMultiplier(1);
    }

    // === Queries (all real) ===============================================
    int ScoreI() const { return GetScore(); }        // Performer::GetScore
    int StreakI() const { return mStats.GetCurrentStreak(); }
    int LongestStreakI() const { return mStats.GetLongestStreak(); }
    int IndividualMult() const { return GetIndividualMultiplier(); }
    int TotalMult() const {
        int a = 1, b = 1, c = 1;
        return GetMultiplier(true, a, b, c);
    }
    const Stats &GetStatsRef() const { return mStats; }

    // === Pure-virtual shim surface (I/O / render / audio — no scoring) =====
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

private:
    int mGemCounter;
};

// ---------------------------------------------------------------------------
static const char *TrackName(TrackType t) {
    switch (t) {
    case kTrackDrum: return "drum";
    case kTrackGuitar: return "guitar";
    case kTrackBass: return "bass";
    case kTrackVocals: return "vocals";
    case kTrackKeys: return "keys";
    default: return "?";
    }
}

int main(int argc, char **argv) {
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    gSystemConfig = DataReadString(kScoringDta);

    // TrackType <-> Symbol mapping: Scoring::Scoring() calls SymToTrackType on
    // each points-block symbol, which reads the DataArray #define TRACK_SYMBOLS
    // (obj/DataUtl DataGetMacro). Register it (same 10-entry order as the
    // TrackType enum) so the real Scoring ctor resolves the instrument rows.
    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    printf("=== rb3-xenon native M6: REAL Performer/Player/Scoring/Stats object graph ===\n\n");

    // --- Build the REAL Scoring singleton from the real scoring.dta ---------
    Scoring *scoring = new Scoring(); // sets TheScoring
    printf("--- real Scoring singleton (config/scoring.dta) ---\n");
    printf("  guitar head=%d chord=%d | bass head=%d chord=%d\n",
           scoring->GetHeadPoints(kTrackGuitar), scoring->GetChordPoints(kTrackGuitar),
           scoring->GetHeadPoints(kTrackBass), scoring->GetChordPoints(kTrackBass));
    printf("  GetStreakMult(guitar): streak 0->x%d 10->x%d 20->x%d 30->x%d\n",
           scoring->GetStreakMult(0, "guitar"), scoring->GetStreakMult(10, "guitar"),
           scoring->GetStreakMult(20, "guitar"), scoring->GetStreakMult(30, "guitar"));
    printf("  GetStreakMult(bass):   streak 40->x%d 50->x%d (bass ramps to 6)\n\n",
           scoring->GetStreakMult(40, "bass"), scoring->GetStreakMult(50, "bass"));

    // --- Choose the instrument (default bass, matching M5's picked track) ---
    TrackType ty = kTrackBass;
    if (argc >= 2 && std::string(argv[1]) == "guitar") ty = kTrackGuitar;
    Symbol streakSym = Symbol(TrackName(ty));
    int maxMult = (ty == kTrackBass || ty == kTrackRealBass) ? 6 : 4;

    // --- Construct the graph: native Band (EnergyMultiplier leaf) + real Player
    Band *band = new Band(/*native*/ true, /*mult*/ 1, /*disambig*/ true);
    NativeScorePlayer player(ty, streakSym, maxMult, band);
    printf("--- graph constructed: NativeScorePlayer(%s) over real Player/Performer, "
           "Band shim, TheScoring ---\n", TrackName(ty));
    printf("  PlayerBehavior: streakType=%s maxMultiplier=x%d (real SetupBehavior config)\n\n",
           player.mBehavior->mStreakType.Str(), player.mBehavior->mMaxMultiplier);

    // === Scenario A: the hurt/pills deterministic verdicts (M5 parity) ======
    // The exact HIT/HIT/MISS/PASS/PASS/HIT sequence M5's real TrackWatcher
    // produced on hurt/pills bass (see rb3-score). Slots: green,red then a
    // wrong-fret miss + pass + dropped pass + orange hit — all single notes.
    printf("--- Scenario A: hurt/pills verdicts through the REAL scoring chain ---\n");
    struct Ev { const char *kind; int slots; };
    Ev scenA[] = {
        {"HIT", 1}, {"HIT", 1}, {"MISS", 1}, {"PASS", 1}, {"PASS", 1}, {"HIT", 1},
    };
    // Mirror the same sequence through M5's transcription for the cross-check.
    ScoreConfig m5cfg;
    {
        DataArray *scoringCfg = gSystemConfig->FindArray(Symbol("scoring"));
        std::vector<float> starThresh; // unused for the score cross-check
        m5cfg.Load(scoringCfg, TrackName(ty), ty == kTrackBass ? "bass" : "guitar",
                   maxMult, starThresh);
    }
    ScoreState m5(m5cfg);
    for (size_t i = 0; i < sizeof(scenA) / sizeof(scenA[0]); i++) {
        const char *k = scenA[i].kind;
        if (!std::string(k).compare("HIT")) { player.OnHit(scenA[i].slots); m5.OnHit(scenA[i].slots); }
        else if (!std::string(k).compare("MISS")) { player.OnMiss(); m5.OnMiss(); }
        else { player.OnPass(); m5.OnPass(); }
        printf("  %-4s -> REAL score=%d streak=%d mult=x%d   |   M5 score=%d streak=%d mult=x%d\n",
               k, player.ScoreI(), player.StreakI(), player.IndividualMult(),
               m5.Score(), m5.Streak(), m5.IndividualMultiplier());
    }
    printf("  cross-check: REAL score=%d vs M5 score=%d  -> %s\n\n",
           player.ScoreI(), m5.Score(),
           player.ScoreI() == m5.Score() ? "MATCH" : "DIVERGENT");

    // === Scenario B: full multiplier ramp + mid-run drop, REAL chain vs M5 ===
    // No overdrive here so both engines stay in the same (individual-multiplier)
    // formula domain and the score must agree note-for-note.
    printf("--- Scenario B: 64-note run (multiplier ramp x1->max, mid-run drop) ---\n");
    printf("  gem  event      REAL(score/streak/mult)   M5(score/streak/mult)\n");
    Band *band2 = new Band(true, 1, true);
    NativeScorePlayer run(ty, streakSym, maxMult, band2);
    ScoreState m5run(m5cfg);
    const int kN = 64;
    int missAt = 34;
    int lastMult = -1;
    for (int i = 0; i < kN; i++) {
        const char *note = "hit";
        if (i == missAt) { run.OnPass(); m5run.OnPass(); note = "DROP"; }
        else { run.OnHit(1); m5run.OnHit(1); }
        int rm = run.IndividualMult();
        if (rm != lastMult || i == missAt) {
            printf("  %3d  %-9s      %6d/%2d/x%d            %6d/%2d/x%d\n", i, note,
                   run.ScoreI(), run.StreakI(), rm,
                   m5run.Score(), m5run.Streak(), m5run.IndividualMultiplier());
            lastMult = rm;
        }
    }
    printf("  final: REAL score=%d longestStreak=%d   |   M5 score=%d longestStreak=%d  -> %s\n",
           run.ScoreI(), run.LongestStreakI(), m5run.Score(), m5run.LongestStreak(),
           run.ScoreI() == m5run.Score() ? "MATCH" : "DIVERGENT");

    // === Scenario C: REAL band-energy deploy arithmetic =====================
    // Player::GetMultiplier's band/overdrive arm: with mDeployingBandEnergy set
    // and mBand->EnergyMultiplier() == 2, a scored note runs the real
    //   i1 (individual) * i2 (=EnergyMult/i3) * i3 (=2)  product.
    printf("\n--- Scenario C: REAL Player::GetMultiplier band-energy deploy ---\n");
    {
        int a = 1, b = 1, c = 1;
        int preMult = run.GetMultiplier(true, a, b, c);
        printf("  before deploy: individual=x%d band=x%d deployFactor=x%d -> total=x%d\n",
               a, b, c, preMult);
        run.DeployBand(); // mDeployingBandEnergy=1, band EnergyMultiplier=2
        int scoreBefore = run.ScoreI();
        run.OnHit(1);
        int mult = run.GetMultiplier(true, a, b, c);
        printf("  deployed:      individual=x%d band=x%d deployFactor=x%d -> total=x%d\n",
               a, b, c, mult);
        printf("  scored 1 note while deployed: +%d pts (real GetMultiplier product) -> score=%d\n",
               run.ScoreI() - scoreBefore, run.ScoreI());
        run.EndDeployBand();
    }

    // === Real Stats collection the transcription does NOT track =============
    // Performer::AddPoints splits every scored point into individual / overdrive
    // / band contributions inside the REAL Stats object. score_engine keeps only
    // a running score — these breakdowns are genuinely new information.
    const Stats &st = run.GetStatsRef();
    printf("\n--- real Stats breakdown (Performer::AddPoints contribution split) ---\n");
    printf("  hitCount=%d  missCount=%d  currentStreak=%d  longestStreak=%d\n",
           st.GetHitCount(), st.mMissCount, st.GetCurrentStreak(), st.GetLongestStreak());
    printf("  mScoreStreak(individual-mult contribution) = %.1f\n", st.mScoreStreak);
    printf("  mOverdrive (overdrive contribution)        = %.1f\n", st.mOverdrive);
    printf("  mBandContribution                          = %.1f\n", st.mBandContribution);
    printf("  mAccuracy (sum of raw head points)         = %d\n", st.mAccuracy);
    printf("  (M5's score_engine tracks none of these — real Stats is new state.)\n");

    printf("\nDone.\n");
    return 0;
}
