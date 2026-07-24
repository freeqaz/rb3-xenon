// rb3-xenon native — M7: the REAL overdrive / band-energy cycle.
//
// M6 (main_score2) ran the real Performer/Player/Scoring/Stats scoring graph but
// *deployed* overdrive by hand — it set the Band shim's mMultiplier directly
// (NativeSetMultiplier). M7 closes the loop with the genuine energy pathway:
//
//   parse   : the REAL SongParser reads a real .mid; MIDI note 116 on the PART
//             track becomes a kCommonPhrase in the REAL SongData / PhraseList
//             (beatmatch layer, no rendering). We print the parsed phrase table
//             straight out of SongData::GetPhraseList.
//   credit  : completing every gem of an overdrive phrase calls the REAL
//             Player::CompleteCommonPhrase -> Player::AddEnergy -> SetEnergy ->
//             SetEnergyAutomatically (mBandEnergy += mParams->mSpotlightPhrase,
//             clamped to 1.0). mParams is the REAL PlayerParams, parsed from the
//             real scoring.dta band_energy block.
//   deploy  : the REAL Player::CanDeployOverdrive gate (mBandEnergy >=
//             mParams->mDeployThreshold) decides eligibility; the deploy state
//             transition then runs the REAL Band::UpdateBonusLevel, which counts
//             deploying players and sets mMultiplier = unk68[bonusLevel] from the
//             real bonuses.dta table. The gameplay multiplier doubles through the
//             REAL Player::GetMultiplier -> Band::EnergyMultiplier path — NOT the
//             shim.
//   drain   : the REAL Player::SubtractEnergy bleeds mBandEnergy by
//             deltaBeats * mParams->mDeployBeats each beat until empty.
//   expiry  : at 0 energy the deploy state clears and the REAL Band::
//             UpdateBonusLevel resets mMultiplier = unk68[0] = 1.
//
// SCOPE LINE (what stays shimmed, and why):
//   * CommonPhraseCapturer — the retail arbiter that *calls* CompleteCommonPhrase
//     is TheGame/SongDB/GemPlayer/TrackPanel-bound multi-track unison machinery
//     (its .cpp exists only in the rb3-Wii oracle). For a single player its
//     decision reduces exactly to HasPlayedWholePhrase -> OneTrackCompletedPhrase
//     -> Player::CompleteCommonPhrase(false,false); we run that real credit method
//     directly and drive the "whole phrase played" test from the real SongData
//     phrase/gem query API. Documented, not hidden.
//   * Deploy()/StopDeployingBandEnergy() also do overdrive-DURATION Stats
//     bookkeeping via Player::GetSongMs()==mBeatMaster->mAudio->GetTime() and play
//     rp_*.cue audio, and the teammate-save fan-out (Band::DeployBandEnergy ->
//     MainPerformer()->Crowd()) needs BandPerformer/CrowdRating. Those are the
//     real-BeatMaster-clock + band-render domain (M8). The multiplier machinery,
//     which is the headline, is fully real here.

#include "game/Player.h"
#include "game/Performer.h"
#include "game/Scoring.h"
#include "game/Stats.h"
#include "game/Band.h"
#include "game/PlayerBehavior.h"

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
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp

static const int kNDiff = 4;
static const int kExpertDiff = 3;

// ---------------------------------------------------------------- config ----
// One combined SystemConfig root: (beatmatcher ...) drives the SongParser, and
// (scoring ...) feeds Scoring / PlayerParams / Band exactly as retail's
// config/scoring.dta does. The band_energy / bonuses / crowd / unison_phrase
// values are the REAL retail numbers (orig-assets/extracted/config/scoring.dta).
static const char *kConfigDta =
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
    "         (guitar  (0 1)(10 2)(20 3)(30 4))"
    "         (bass    (0 1)(10 2)(20 3)(30 4)(40 5)(50 6))"
    "         (drum    (0 1)(10 2)(20 3)(30 4))"
    "         (keys    (0 1)(10 2)(20 3)(30 4))"
    "         (vocals  (0 1)(10 2)(20 3)(30 4))"
    "         (default (0 1)(10 2)(20 3)(30 4)))"
    "      (energy (default (0 1))))"
    "   (overdrive"
    "      (recharge_rate 0.0)(star_phrase 0.25)(common_phrase 0.15)"
    "      (fill_boost 0.35)(whammy_rate 3.4e-2)(ready_level 0.5)"
    "      (multiplier 2)(crowd_boost 6))"
    // -- the REAL band_energy block PlayerParams::PlayerParams() parses --
    "   (band_energy"
    "      (deploy_beats 32)"
    "      (deploy_bonus 50)"
    "      (spotlight_phrase 0.251)"
    "      (unison_phrase 0.501)"
    "      (deploy_threshold 0.5)"
    "      (save_energy 0.5))"
    // -- the REAL bonuses block Band::NativeLoadBonuses() (== retail ctor) parses
    "   (bonuses"
    "      (max_bonus 4)"
    "      (multiplier (1 2 4 6 8))"
    "      (crowd_boost (1 6 6 6 6)))"
    // -- unison_phrase.point_bonus + crowd.* are read by PlayerParams too --
    "   (unison_phrase (reward 2.0)(penalty 2.0)(point_bonus 1000))"
    "   (crowd"
    "      (save_level 0.3)"
    "      (time_to_return_from_brink 2.0)"
    "      (crowd_loss_per_sec 0.1))"
    ")";

// -------------------------------------------------------------- SongInfo ----
// Same minimal SongInfo main_gem/main_hit use: SongParser only needs
// NumChannelsOfTrack + TrackIndex.
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

// ======================================================= NativeScorePlayer ==
// Concrete subclass of the REAL abstract Player (M6's shim surface for the pure
// I/O / render / audio virtuals), extended for M7 with the energy cycle helpers
// that call the REAL Player energy methods.
class NativeScorePlayer : public Player {
public:
    NativeScorePlayer(TrackType ty, Symbol streakType, int maxMult, Band *band)
        : Player(/*user*/ nullptr, band, /*tracknum*/ 0, /*bmaster*/ nullptr,
                 /*native_score_tag*/ true),
          mGemCounter(0) {
        mTrackType = ty;
        mBehavior->SetStreakType(streakType);
        mBehavior->SetMaxMultiplier(maxMult);
        // M7: the REAL PlayerParams, parsed from the real band_energy/crowd/
        // unison_phrase config. Supplies mSpotlightPhrase / mDeployThreshold /
        // mDeployBeats that the real energy methods read. mPhraseBonus is on
        // (native ctor), so CompleteCommonPhrase awards mSpotlightPhrase.
        NativeInitParams();
        // unk2b0 = "overdrive enabled". Retail sets it from TheGame->mProperties
        // .mEnableOverdrive in the game ctor; the native scoring ctor leaves it 0,
        // so arm it here (== overdrive turned on for this player).
        unk2b0 = true;
    }

    // ---- M6 scoring core (unchanged): drive a beat-match verdict -----------
    void OnHit(int numSlots) {
        BuildHitStreak(mGemCounter, 0.0f);
        EndMissStreak();
        int hp = TheScoring->GetHeadPoints(mTrackType);
        int cp = TheScoring->GetChordPoints(mTrackType);
        int pts = (numSlots == 1) ? numSlots * hp : (cp < 0 ? numSlots * hp : cp);
        AddPoints((float)pts, true, true);
        mStats.AddAccuracy(pts);
        mGemCounter++;
    }
    void OnPass() { // GemPlayer::Pass -> EndHitStreak + BuildMissStreak
        EndHitStreak();
        BuildMissStreak(mGemCounter);
    }

    // ---- M7: REAL overdrive-phrase credit ---------------------------------
    // Exactly what CommonPhraseCapturer::OneTrackCompletedPhrase runs for one
    // player once a phrase is fully played: Player::CompleteCommonPhrase(false,
    // false) -> AddEnergy(mParams->mSpotlightPhrase). (UnisonHit(), which the
    // capturer also calls, is a TrackPanel audio cue = render leaf.)
    void CreditOverdrivePhrase() { CompleteCommonPhrase(false, false); }

    // ---- M7: REAL deploy state transition ---------------------------------
    // == Player::PerformDeployBandEnergy(0, true) minus Deploy()'s clock-gated
    // Stats bookkeeping + audio cue. Sets the real mDeployingBandEnergy and runs
    // the REAL Band::UpdateBonusLevel, so mMultiplier = unk68[bonusLevel].
    void DeployReal(float pollMs) {
        mDeployingBandEnergy = true;
        static_cast<Band *>(mBand)->UpdateBonusLevel(pollMs);
    }
    // ---- M7: REAL drain primitive (what UpdateEnergy calls each beat) ------
    void DrainReal(float deltaBeats) { SubtractEnergy(deltaBeats * mParams->mDeployBeats); }
    // ---- M7: REAL expiry state transition ---------------------------------
    // == StopDeployingBandEnergy's multiplier-relevant core (clear deploying +
    // Band::UpdateBonusLevel -> mMultiplier = unk68[0]).
    void ExpireReal(float pollMs) {
        mDeployingBandEnergy = false;
        static_cast<Band *>(mBand)->UpdateBonusLevel(pollMs);
    }

    // ---- Queries (all real) -----------------------------------------------
    int ScoreI() const { return GetScore(); }
    int IndividualMult() const { return GetIndividualMultiplier(); }
    float Energy() const { return mBandEnergy; }
    bool CanDeploy() const { return CanDeployOverdrive(); }
    int TotalMult() const {
        int a = 1, b = 1, c = 1;
        return GetMultiplier(true, a, b, c);
    }
    void MultTriple(int &i1, int &i2, int &i3) const { GetMultiplier(true, i1, i2, i3); }

    // ---- Pure-virtual shim surface (I/O / render / audio) -----------------
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
    default: return "?";
    }
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-grinnz/tool/vicarious/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;
    // Which PART track to score. vicarious carries note-116 OD markers on PART
    // DRUMS (15), HARM1 and PART VOCALS; PART GUITAR/BASS have none. Default to
    // the real instrumental OD track: drums.
    const char *partName = (argc >= 3) ? argv[2] : "PART DRUMS";

    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    gSystemConfig = DataReadString(kConfigDta);
    // Scoring::Scoring() resolves the points-block symbols through TRACK_SYMBOLS.
    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    printf("=== rb3-xenon native M7: the REAL overdrive / band-energy cycle ===\n");
    printf("mid : %s\n", midPath);
    printf("part: %s\n\n", partName);

    // --- REAL Scoring singleton --------------------------------------------
    Scoring *scoring = new Scoring(); // sets TheScoring

    // --- Parse the .mid into the REAL SongData (note 116 -> kCommonPhrase) --
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
    if (track == -1) {
        printf("  track '%s' not found in chart; aborting.\n", partName);
        return 1;
    }
    TrackType trackTy = songData.TrackTypeAt(track);
    GameGemList *gems = songData.GetGemList(track);
    int nGems = gems->NumGems();

    // --- REAL overdrive phrase table (SongData::GetPhraseList kCommonPhrase) -
    // GetPhraseList(track, type) -> mPhraseDBs[track]->GetPhraseList(
    // mTrackDifficulties[track], type): the selected track's common-phrase list
    // at its resolved (Expert) difficulty.
    const PhraseList &odPhrases = songData.GetPhraseList(track, kCommonPhrase);
    int nPhrases = (int)odPhrases.mPhrases.size();
    printf("  track %d '%s' (type %d): %d Expert gems, %d overdrive phrase(s) "
           "(MIDI note 116 -> kCommonPhrase)\n\n",
           track, songData.TrackName(track).Str(), (int)trackTy, nGems, nPhrases);

    if (nPhrases == 0) {
        printf("  NO overdrive phrases parsed on this track — pick a PART with "
               "note-116 markers (vicarious: PART DRUMS).\n");
        return 1;
    }

    printf("--- REAL parsed overdrive-phrase table ---\n");
    printf("  %-4s %10s %10s %10s %10s %6s\n", "#", "startTick", "endTick",
           "startMs", "endMs", "gems");
    // Map gems into each phrase [startTick, endTick).
    std::vector<std::vector<int> > phraseGems(nPhrases);
    for (int p = 0; p < nPhrases; p++) {
        const Phrase &ph = odPhrases.mPhrases[p];
        int start = ph.GetTick();
        int end = ph.GetTick() + ph.GetDurationTicks();
        for (int g = 0; g < nGems; g++) {
            int gt = gems->GetGem(g).GetTick();
            if (gt >= start && gt < end)
                phraseGems[p].push_back(g);
        }
        printf("  %-4d %10d %10d %10.1f %10.1f %6d\n", p, start, end, ph.GetMs(),
               ph.GetMs() + ph.GetDurationMs(), (int)phraseGems[p].size());
    }
    printf("\n");

    // --- Build the REAL scoring graph over the real energy machinery --------
    Band *band = new Band(/*native*/ true, /*mult*/ 1, /*disambig*/ true);
    band->NativeLoadBonuses(); // REAL bonuses.dta parse -> unk68/unk70/mMaxBonusLevel
    printf("--- REAL Band bonuses table (SystemConfig scoring/bonuses) ---\n");
    printf("  max_bonus=%d  multiplier[bonusLevel] = {", band->mMaxBonusLevel);
    for (size_t i = 0; i < band->unk68.size(); i++)
        printf("%s%d", i ? " " : "", band->unk68[i]);
    printf("}  (bonusLevel 0=>x%d, 1=>x%d)\n\n", band->unk68[0], band->unk68[1]);

    int maxMult = (trackTy == kTrackBass || trackTy == kTrackRealBass) ? 6 : 4;
    NativeScorePlayer player(trackTy, Symbol(TrackName(trackTy)), maxMult, band);
    band->mActivePlayers.push_back(&player); // so UpdateBonusLevel counts it
    printf("--- graph: NativeScorePlayer(%s) over real Player/Band/Scoring, "
           "PlayerParams parsed ---\n", TrackName(trackTy));
    printf("  PlayerParams: spotlightPhrase=%.3f deployThreshold=%.3f "
           "deployBeats(1/32/beat)=%.5f\n\n",
           player.mParams->mSpotlightPhrase, player.mParams->mDeployThreshold,
           player.mParams->mDeployBeats);

    // === Stage 2: credit energy by completing real overdrive phrases ========
    // Faithful single-player reduction of CommonPhraseCapturer: a phrase is
    // credited only when EVERY gem in it is hit (HasPlayedWholePhrase); then the
    // REAL Player::CompleteCommonPhrase awards mSpotlightPhrase via AddEnergy.
    // We fully hit every phrase except one (phrase index `missIdx`) where we drop
    // a gem, to show the gate: a missed phrase credits nothing.
    printf("--- Stage 2: complete overdrive phrases -> REAL AddEnergy ---\n");
    int missIdx = (nPhrases >= 3) ? 1 : -1; // drop one gem in phrase 1
    bool deployed = false;
    for (int p = 0; p < nPhrases && !deployed; p++) {
        const std::vector<int> &gs = phraseGems[p];
        if (gs.empty()) {
            printf("  phrase %d: no gems in extent (skip)\n", p);
            continue;
        }
        bool wholePhrase = true;
        for (size_t k = 0; k < gs.size(); k++) {
            if ((int)p == missIdx && k == gs.size() / 2) {
                wholePhrase = false; // simulate a dropped gem
                player.OnPass();     // real streak break
                continue;
            }
            player.OnHit(1); // real scoring
        }
        float before = player.Energy();
        if (wholePhrase) {
            player.CreditOverdrivePhrase(); // REAL CompleteCommonPhrase -> AddEnergy
            printf("  phrase %d: %d/%d gems hit -> CompleteCommonPhrase  energy "
                   "%.3f -> %.3f  (+%.3f)  canDeploy=%s\n",
                   p, (int)gs.size(), (int)gs.size(), before, player.Energy(),
                   player.Energy() - before, player.CanDeploy() ? "YES" : "no");
        } else {
            printf("  phrase %d: %d/%d gems hit (dropped 1) -> NO credit       "
                   "energy %.3f -> %.3f  (gate held)\n",
                   p, (int)gs.size() - 1, (int)gs.size(), before, player.Energy());
        }
        if (player.CanDeploy()) {
            printf("\n  >>> energy %.3f >= deployThreshold %.3f : REAL "
                   "CanDeployOverdrive() == true\n\n",
                   player.Energy(), player.mParams->mDeployThreshold);
            deployed = true;
        }
    }

    if (!player.CanDeploy()) {
        printf("\n  energy %.3f never reached deployThreshold %.3f on this chart.\n",
               player.Energy(), player.mParams->mDeployThreshold);
        return 0;
    }

    // === Stage 3: deploy through the REAL Band::EnergyMultiplier path ========
    printf("--- Stage 3: deploy -> REAL Band::UpdateBonusLevel / EnergyMultiplier "
           "---\n");
    {
        int i1 = 1, i2 = 1, i3 = 1;
        int pre = (player.MultTriple(i1, i2, i3), player.TotalMult());
        printf("  before deploy: individual=x%d bandEnergyMult=x%d deployFactor=x%d "
               "-> total=x%d  (Band.mMultiplier=%d bonusLevel=%d)\n",
               i1, i2, i3, pre, band->mMultiplier, band->mBonusLevel);

        player.DeployReal(0.0f); // REAL: mDeployingBandEnergy=1 + UpdateBonusLevel
        player.MultTriple(i1, i2, i3);
        int post = player.TotalMult();
        printf("  after  deploy: individual=x%d bandEnergyMult=x%d deployFactor=x%d "
               "-> total=x%d  (Band.mMultiplier=%d bonusLevel=%d)\n",
               i1, i2, i3, post, band->mMultiplier, band->mBonusLevel);
        printf("  Band::EnergyMultiplier()=%d (real accessor); gameplay multiplier "
               "%s through the REAL path\n\n",
               band->EnergyMultiplier(), post == 2 * pre ? "DOUBLED" : "changed");

        // Prove the unk68[bonusLevel] table is load-bearing: a 2nd deploying
        // player pushes bonusLevel to 2 -> mMultiplier = unk68[2] = 4.
        Band *band2 = new Band(true, 1, true);
        band2->NativeLoadBonuses();
        NativeScorePlayer a(kTrackGuitar, Symbol("guitar"), 4, band2);
        NativeScorePlayer b(kTrackBass, Symbol("bass"), 6, band2);
        band2->mActivePlayers.push_back(&a);
        band2->mActivePlayers.push_back(&b);
        a.DeployReal(0.0f);
        printf("  [table check] 1 deployer  -> bonusLevel=%d mMultiplier=unk68[1]=%d\n",
               band2->mBonusLevel, band2->mMultiplier);
        b.DeployReal(0.0f);
        printf("  [table check] 2 deployers -> bonusLevel=%d mMultiplier=unk68[2]=%d "
               "(the real bonuses table drives it)\n\n",
               band2->mBonusLevel, band2->mMultiplier);
    }

    // === Stage 4: drain + expiry through the REAL energy methods =============
    printf("--- Stage 4: drain (REAL SubtractEnergy, %.0f beats to empty) ---\n",
           player.Energy() / player.mParams->mDeployBeats);
    printf("  %6s %8s %8s\n", "beat", "energy", "mult");
    int beat = 0;
    float pollMs = 0.0f;
    while (player.Energy() > 0.0f && beat < 200) {
        if (beat % 4 == 0 || player.Energy() <= player.mParams->mDeployBeats) {
            printf("  %6d %8.4f     x%d\n", beat, player.Energy(),
                   player.TotalMult()); // REAL GetMultiplier product (stays doubled)
        }
        player.DrainReal(1.0f); // REAL SubtractEnergy(1 beat * mDeployBeats)
        pollMs += 20.0f;
        beat++;
    }
    printf("  energy hit %.4f at beat %d -> expiry\n", player.Energy(), beat);
    player.ExpireReal(pollMs); // REAL: mDeployingBandEnergy=0 + UpdateBonusLevel
    {
        int i1 = 1, i2 = 1, i3 = 1;
        player.MultTriple(i1, i2, i3);
        printf("  after expiry: deployFactor=x%d total=x%d  (Band.mMultiplier=%d "
               "bonusLevel=%d) -> reset to unk68[0]=%d\n",
               i3, player.TotalMult(), band->mMultiplier, band->mBonusLevel,
               band->unk68[0]);
    }

    printf("\nDone.\n");
    return 0;
}
