// rb3-xenon native M10 — the full vocal-gameplay orchestration run-through.
//
// Where M9 (rb3-vocal) proved the VocalPart pitch engine in isolation (driver-owned
// scoring loop, VocalPart brought up by calloc+init), M10 runs the REAL orchestration
// layer M9 bypassed: a REAL VocalPlayer, built through a native scoring-core ctor,
// with REAL Singers and REAL VocalParts, driven by the REAL VocalPlayer::Poll each
// frame off an advancing clock. Every frame:
//   VocalPlayer::Poll -> VocalPart::Poll (phrase/freestyle state)
//                     -> Singer::Poll -> (synthetic GameMic) -> Singer::Poll_
//                     -> VocalPart::ScoreSinger (real Gaussian pitch matcher)
//                     -> greedy singer<->part assignment (FindBestPart)
//                     -> Singer::SetAssignedPart / AllScoresAreIn / ResolveAmbiguity
//                     -> VocalPart::AddScore / AfterPoll (phrase-score accumulation)
//                     -> at phrase boundaries: VocalPart::HandlePhraseEnd
//                        (real CalculatePhraseRating + AddPoints)
// The ONLY synthetic input is the microphone: GameMic::unk2c (pitch) / unk28 (energy)
// are written per frame (perfect-pitch first half, tritone-off second half). All
// pitch matching, singer assignment, phrase accumulation, rating, and player scoring
// is REAL engine code. See native/src/m10_support.cpp for the mic/singleton census.
#include "game/VocalPlayer.h"
#include "game/VocalPart.h"
#include "game/Singer.h"
#include "game/Player.h"
#include "game/Band.h"
#include "game/PlayerBehavior.h"
#include "game/Scoring.h"
#include "game/CrowdRating.h"
#include "game/SongDB.h"
#include "game/Game.h"
#include "game/GameMicManager.h"
#include "net/NetSession.h"
#include "beatmatch/VocalNote.h"
#include "beatmatch/SongParser.h"
#include "beatmatch/SongData.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/TuningOffsetList.h"
#include "beatmatch/GameGemDB.h"
#include "utl/BeatMap.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "utl/TempoMap.h"
#include "utl/SongPos.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "os/System.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig;
void DataInit();
void SetTheBeatMap(BeatMap *);

// m10_support.cpp
GameMicManager *NativeMakeGameMicManager(int nSingers);
void NativeSetMicFrame(int i, float pitch, float energy);
NetSession *NativeMakeNetSession();
extern GameMicManager *TheGameMicManager;

#include "utl/SongInfoCopy.h"
#include "utl/SongInfoAudioType.h"
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

static const int kNDiff = 4;
static const int kExpertDiff = 3;

// Full retail scoring/vocals config (transcribed from the extracted scoring.dta),
// now including the scream + tambourine keys the Singer / TambourineDetector ctors
// read (absent from M9's block because M9 never constructed a Singer).
static const char *kConfigDta =
    "(beatmatcher"
    "   (parser"
    "      (player_slot 9)"
    "      (vocal_style_instruments (3))"
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
    "(player (handlers))"
    "(scoring"
    // Band/Player scoring init (Band::NativeLoadBonuses reads (bonuses ...);
    // Player streak/energy config reads the rest) — same retail values as M8.
    "   (points"
    "      (vocals (head 0)(tail 0)(chord -1)))"
    "   (streaks"
    "      (multipliers (vocals (0 1)(1 2)(2 3)(3 4))(default (0 1)(10 2)(20 3)(30 4)))"
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
    // solo/tambourine award table (ComputeTambourinePoints -> GetSoloAward)
    "   (solo"
    "      (default"
    "         (awards (0 0 failed_solo)(60 5 bad_solo)(70 10 okay_solo)"
    "                 (80 20 solid_solo)(90 30 great_solo)(95 50 awesome_solo)"
    "                 (100 100 perfect_solo))"
    "         (reward 1.0)(penalty 1.0))"
    "      (tambourine"
    "         (awards (0 0 tamb_rating_1)(1 5 tamb_rating_2)(40 10 tamb_rating_3)"
    "                 (60 20 tamb_rating_4)(80 50 tamb_rating_5)(100 100 tamb_rating_6))))"
    "   (crowd"
    "      (save_level 0.3)(time_to_return_from_brink 2.0)(crowd_loss_per_sec 0.1))"
    "   (star_ratings"
    "      (new_instrument_thresholds"
    "         (vocals 5.0e-2 0.11 0.19 0.46 0.77 1.06))"
    "      (new_num_instruments_multiplier 1.0 1.26 1.52 1.8 1.8)"
    "      (new_bonus_thresholds 5.0e-2 0.1 0.2 0.3 0.4 0.95 1.0))"
    "   (vocals"
    "      (rating_thresholds 0.6 0.75 0.9 0.99)"
    "      (slop 180 140 120 120)"
    "      (pitch_margin 3.8 2.6 1.9 1.2)"
    "      (nonpitch_easy_multiplier 3.0)"
    "      (nonpitch_energy_threshold 4.5e-4)"
    "      (nonpitch_stickiness 0.25)"
    "      (vocal_cap_growth 1.2 1.15 1.15 1.1)"
    "      (pitch_hit_multiplier 1.7 1.5 1.35 1.25)"
    "      (nonpitch_hit_multiplier 1.7 1.5 1.35 1.25)"
    "      (short_note_threshold_ms 166)"
    "      (short_note_multiplier 1.5 1.4 1.35 1.3)"
    "      (note_length_factor 1.0 1.0 1.0 1.0)"
    "      (phrase_value 200 400 800 1000)"
    "      (part_score_multiplier 1.0 0.1 0.1)"
    "      (track_wrapping_margin 0.0)"
    "      (max_detune 1.0)"
    "      (packet_period 250)"
    "      (scream_energy_threshold 0.3)"
    "      (tambourine_points 0)"
    "      (tambourine_crowd_success 0.0 0.0 0.0 0.0)"
    "      (tambourine_crowd_failure 0.0 0.0 0.0 0.0)"
    "      (tambourine_energy_rise_threshold 4.0e-2)"
    "      (tambourine_energy_drop_threshold 2.0e-2)"
    "      (tambourine_window_ticks 120)"
    "      (tambourine_ms_offset 24)"
    "      (tambourine_deployment_suppress_ms 20.0)"
    "      (freestyle_deployment_time (500 400))"
    "      (freestyle_min_duration (600 500))"
    "      (freestyle_pad (100 50))"
    "      (synapse_proximity_solo 0.78 0.78 0.78 0.78)"
    "      (synapse_focus_solo 1.0e-4 1.0e-4 1.0e-4 1.0e-4)"
    "      (synapse_proximity_harm 0.956 0.956 0.956 0.956)"
    "      (synapse_focus_harm 1.0e-4 1.0e-4 1.0e-4 1.0e-4)))";

static const char *RatingName(int r) {
    static const char *n[6] = {"awful", "ok", "good", "great", "awesome", "perfect"};
    if (r < 0) r = 0;
    if (r > 5) r = 5;
    return n[r];
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-grinnz/tool/vicarious/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;

    setvbuf(stdout, nullptr, _IONBF, 0);
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);
    gSystemConfig = DataReadString(kConfigDta);

    printf("=== rb3-xenon native M10: full vocal-gameplay orchestration ===\n");
    printf("mid : %s\n\n", midPath);

    // TRACK_SYMBOLS data-macro (Scoring/TrackType symbol<->enum mapping), as M8.
    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    new Scoring(); // sets TheScoring (real star-threshold + solo-award machinery)

    // --- Stage 1: REAL SongParser -> SongData (vocals via the sink) -----------
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
    {
        FileStream fs(midPath, FileStream::kRead, false);
        SongParser parser(songData, kNDiff, songData.mTempoMap, songData.mMeasureMap, 2);
        parser.ReadMidiFile(fs, midPath, &songInfo);
        int pumps = 0;
        while (!parser.NoMidiReader() && pumps < 2000000) {
            parser.Poll();
            pumps++;
        }
        printf("--- Stage 1: REAL SongParser -> SongData (%d Poll pumps) ---\n", pumps);
    }
    if (songData.mTempoMap) songData.mTempoMap->Finalize();
    for (size_t i = 0; i < songData.mGemDBs.size(); i++) {
        songData.mGemDBs[i]->MergeChordGems();
        songData.mGemDBs[i]->Finalize();
    }
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    songData.mPlayerTrackConfigList = &expertList;
    songData.FixUpTrackConfig(&expertList);
    songData.SetUpTrackDifficulties(&expertList);
    songData.PostLoadVocals();

    int nLists = songData.GetVocalNoteListCount();
    printf("--- vocal note lists parsed: %d ---\n", nLists);
    if (nLists == 0) { printf("  no vocal note lists; abort.\n"); return 1; }

    VocalNoteList *list = songData.GetVocalNoteList(0);
    const std::vector<VocalNote> &notes = list->GetNotes();
    std::vector<VocalPhrase> &phrases = list->GetPhrases();
    int pitched = 0, talky = 0;
    for (size_t i = 0; i < notes.size(); i++) (notes[i].mUnpitchedNote ? talky : pitched)++;
    float lastMs = notes.empty() ? 0.0f : notes.back().mMs + notes.back().mDurationMs;
    float durationMs = lastMs + 3000.0f;
    printf("  part 0 '%s': %d notes (%d pitched, %d talky), %d phrases, ~%.1fs\n\n",
           list->GetTrackName().Str(), (int)notes.size(), pitched, talky,
           (int)phrases.size(), durationMs / 1000.0f);

    // --- singletons the REAL VocalPlayer::Poll path resolves through ----------
    SongDB *songDB = new SongDB();
    songDB->mSongData = &songData;
    songDB->mSongDurationMs = durationMs;
    TheSongDB = songDB;

    TheGameMicManager = NativeMakeGameMicManager(1); // one synthetic mic (solo)
    TheNetSession = NativeMakeNetSession();

    Game *game = (Game *)std::calloc(1, sizeof(Game));
    game->mProperties.mEnableStreak = true;
    game->mProperties.mEnableOverdrive = true;
    game->mProperties.mAllowOverdrivePhrases = true;
    game->mProperties.mEndWithSong = false;
    game->unkdc = -1.0f;
    game->mIsPaused = false;
    TheGame = game;

    Band *band = new Band(true, 1, true);
    band->NativeLoadBonuses();

    // --- REAL VocalPlayer via the native scoring-core ctor --------------------
    int trackNum = 0;
    VocalPlayer *vp = new VocalPlayer(
        /*user*/ 0, /*bmaster*/ 0, band, trackNum, /*perf*/ 0, /*nsingers*/ 1,
        kExpertDiff, /*native_tag*/ true);
    band->mActivePlayers.push_back(vp);

    // mTrack sentinel: non-null so VocalPlayer::Poll enters the scoring path; all
    // VocalTrack render derefs on the Poll/phrase-end path are HX_NATIVE-gated.
    vp->mTrack = (VocalTrack *)std::calloc(1, 4096);
    // mCrowd: the Player scoring-core ctor leaves it null; the phrase-end crowd
    // meter needs it (headless CrowdRating shim, same as M8's NativeScorePlayer).
    vp->mCrowd = new CrowdRating(0, (Difficulty)kExpertDiff);
    // Streak-multiplier config: retail's ConfigureBehavior sets these from
    // mUser->GetTrackSym(); headless has no BandUser, so set them directly (the
    // vocals streak table lives in the config). Without this GetIndividualMultiplier
    // returns 0 and phrase points never credit the score.
    vp->mBehavior->SetStreakType(Symbol("vocals"));
    vp->mBehavior->SetMaxMultiplier(4);

    vp->PostLoad(true); // builds VocalParts + Singer::PostLoad(TalkyMatcher LoadEvents)

    // init phrase iterators / score state on the REAL parts + singers (retail does
    // this through VocalPlayer::Restart, which also pokes mBeatMaster/mUser we omit)
    for (size_t i = 0; i < vp->mVocalParts.size(); i++) vp->mVocalParts[i]->Restart(false);
    for (size_t i = 0; i < vp->mSingers.size(); i++) vp->mSingers[i]->Restart(false);
    for (size_t i = 0; i < vp->mVocalParts.size(); i++) vp->mVocalParts[i]->Start();
    for (size_t i = 0; i < vp->mSingers.size(); i++) vp->mSingers[i]->Start();

    printf("--- REAL VocalPlayer (Expert): %d singer(s), %d vocal part(s) ---\n",
           vp->NumSingers(), vp->NumVocalParts());
    VocalPart *part0 = vp->mVocalParts[0];
    printf("  part0 slop=%.0fms pitchMaxDist=%.2f pitchSigma=%.4f phraseValue=%d\n",
           part0->mSlop, part0->mPitchMaximumDistance, part0->mPitchSigma,
           part0->mPhraseValue);
    printf("  ratingThresholds via CalculatePhraseRating (real config)\n\n");

    // ===================== the clock-driven Poll loop =========================
    // Synthetic singer: sing the target pitch exactly for the first half of the
    // song, then a tritone (+6 semitones) off for the second half. Energy is held
    // above the non-pitch threshold whenever there's a note to sing.
    const float dt = 1000.0f / 60.0f;
    const float switchMs = durationMs * 0.5f;
    bool announcedSwitch = false;

    printf("--- clock-driven run (dt=%.2fms; perfect first half, +6 semis after "
           "%.1fs) ---\n", dt, switchMs / 1000.0f);
    printf("  %-8s %-7s %6s %9s %9s %7s  %-8s\n",
           "songMs", "phrase", "mode", "phrScore", "phrMax", "frac", "rating");

    int scoredPhrases = 0, phrasesHit = 0;
    double sumFrac = 0.0;
    int lastReportedPhrase = -1;

    for (float ms = 0.0f; ms <= durationMs; ms += dt) {
        bool singOff = (ms >= switchMs);
        if (singOff && !announcedSwitch) {
            printf("  >>> %.1fs: singer switches perfect -> +6 semitones (off)\n",
                   ms / 1000.0f);
            announcedSwitch = true;
        }

        // synthetic mic frame: the REAL target pitch at this ms (0 => no note =>
        // silence), detuned a tritone in the off half. Energy above threshold when
        // there is pitch to sing.
        float target = list->PitchAt(vp->GetCompensatedTime(ms));
        float sungPitch = 0.0f, energy = 0.0f;
        if (target != 0.0f) {
            sungPitch = singOff ? (target + 6.0f) : target;
            energy = 0.05f; // comfortably over nonpitch_energy_threshold
        }
        NativeSetMicFrame(0, sungPitch, energy);

        // capture the ending phrase's real accumulated score just before Poll (which
        // resets it inside VocalPart::HandlePhraseEnd at the boundary)
        float cms = vp->GetCompensatedTime(ms);
        bool atEnd = part0->AtPhraseEnd(cms);
        float endScore = 0.0f, endMax = 0.0f;
        int endPhrase = -1;
        if (atEnd) {
            endScore = part0->mPhraseScore;
            endMax = part0->mPhraseScoreMax;
            endPhrase = part0->CurrentPhraseIndex();
        }

        // ===== the REAL orchestration =====
        SongPos pos = songData.CalcSongPos(ms);
        vp->Poll(ms, pos);

        if (atEnd && endMax > 0.0f && endPhrase != lastReportedPhrase) {
            lastReportedPhrase = endPhrase;
            float frac = endScore / endMax;
            if (frac > 1.0f) frac = 1.0f;
            int rating = vp->CalculatePhraseRating(frac);
            scoredPhrases++;
            sumFrac += frac;
            if (rating > 0) phrasesHit++;
            if (scoredPhrases <= 20 || scoredPhrases % 4 == 0) {
                printf("  %-8.0f %-7d %6s %9.2f %9.2f %7.3f  %d (%s)\n",
                       ms, endPhrase, singOff ? "+6off" : "exact",
                       endScore, endMax, frac, rating, RatingName(rating));
            }
        }
    }

    // ============================ final results =============================
    printf("\n--- SONG COMPLETE ---\n");
    printf("  scored phrases : %d  (%d hit, %d missed)\n",
           scoredPhrases, phrasesHit, scoredPhrases - phrasesHit);
    printf("  avg phrase frac: %.3f\n",
           scoredPhrases ? sumFrac / scoredPhrases : 0.0);
    printf("  player score   : %d  (REAL Player::GetScore via AddPoints)\n",
           vp->GetScore());
    printf("  stars          : %d  (%.2f)\n", vp->GetNumStars(), vp->GetNumStarsFloat());
    printf("\n  (all pitch matching, singer<->part assignment, phrase accumulation,\n"
           "   and rating via REAL VocalPlayer::Poll / Singer / VocalPart engine code;\n"
           "   only the microphone pitch/energy stream is synthetic.)\n");
    printf("\nDone.\n");
    return 0;
}
