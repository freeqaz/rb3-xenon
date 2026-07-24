// rb3-xenon native M11 — the multi-part HARMONY vocal-scoring topology.
//
// Where M10 (rb3-vocal2) ran ONE VocalPart + ONE Singer through the real
// VocalPlayer::Poll pipeline, M11 exercises the last untested scoring topology:
// MULTIPLE vocal parts (HARM1/HARM2/HARM3) scored by MULTIPLE synthetic singers
// through the REAL greedy singer<->part assignment (FindBestPart /
// AddSingerCandidate / GetBestSingerCandidate) and the REAL ambiguity machinery
// (Singer::AddAmbiguousPart / AllScoresAreIn / ResolveAmbiguity).
//
// The chart is a REAL Rock Band 3 harmony .mid (centerfold: PART VOCALS + HARM1 +
// HARM2 + HARM3). The parse is the REAL SongParser path: track_mapping routes the
// HARM tracks by name -> SongData::AddTrack computes harmPartNum from the "HARM%d"
// suffix -> a distinct VocalNoteList per harmony part; PlayerTrackConfigList::
// UseVocalHarmony() gates GetVocalNoteListCount()/GetVocalNoteList() so the parts
// surface as parts 0/1/2. SongData::PostLoadVocals then runs the real harmony
// phrase-copy (HARM2/HARM3 CopyPhrasesFrom HARM1) + freestyle intersection.
//
// Every frame:
//   VocalPlayer::Poll -> per singer: Singer::Poll -> VocalPart::ScoreSinger on
//     EACH part (Gaussian pitch matcher) -> ambiguity detection between part pairs
//   -> greedy loop: FindBestPart per singer, AddSingerCandidate, assign best
//     singer per part, remove assigned, repeat (real Duff's-device remove_if)
//   -> Singer::AllScoresAreIn / ResolveAmbiguity -> VocalPart::AddScore
//   -> at part[0] phrase boundary: VocalPlayer::HandlePhraseEnd (real per-part
//      rank/rating + double/triple-harmony stat accounting + combined scoring)
//
// The ONLY synthetic input is the two microphones. Singers are driven through
// three phases to force each distinct assignment/ambiguity regime (see below).
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
#include "game/MultiplayerAnalyzer.h" // PlayerScoreInfo (star-threshold base scores)
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
void NativeSetMicSamples(int i, const short *buf, int count);
NetSession *NativeMakeNetSession();
extern GameMicManager *TheGameMicManager;

#include "game/Singer.h"
#include "synth/VoiceBeat.h" // TalkyMatcher / VoiceBeat (talky-hit observation)

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
    int GetNumVocalParts() const { return 3; } // HARM1/HARM2/HARM3
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

// Full retail scoring/vocals config (transcribed from the extracted scoring.dta).
// The ONLY delta from M10's block is the track_mapping: HARM1/HARM2/HARM3 entries
// so the parser routes the harmony tracks. Each HARM entry uses the VOCALS audio
// type (6) and TrackType kTrackVocals (3); the 4th field (original_name) is the
// literal MIDI track name so PartInfo::ContainsTrackName pairs it.
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
    "         (HARM1 6 3 HARM1)(HARM2 6 3 HARM2)(HARM3 6 3 HARM3)"
    "         (KEYS 9 4 'PART KEYS'))"
    "   )"
    "   (controllers (beatmatch_controller_mapping (guitar guitar)))"
    "   (audio (submixes))"
    ")"
    "(player (handlers))"
    "(scoring"
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

// A synthetic VOICED burst (fundamental + two harmonics, ~180Hz in the vocal
// band) fed to the REAL TalkyMatcher/VoiceBeat DSP for the chart's unpitched
// (talky) notes. Voiced tone -> high voice/full-band energy ratio -> VoiceBeat
// unk1 (syllable) true, while a short burst keeps unk0 (spam) low: the exact
// window VocalPart::CouldScoreAgainstPart wants (unk1 && !unk0 && overEnergy).
static const int kBurstSamples = 267; // ~one 60fps frame at 16 kHz
static short gVoicedBurst[kBurstSamples];
static void InitVoicedBurst() {
    const double kPi = 3.14159265358979323846, sr = 16000.0, f0 = 180.0;
    for (int i = 0; i < kBurstSamples; i++) {
        double t = (double)i / sr;
        double v = 0.6 * sin(2 * kPi * f0 * t)
                 + 0.3 * sin(2 * kPi * 2 * f0 * t)
                 + 0.15 * sin(2 * kPi * 3 * f0 * t);
        gVoicedBurst[i] = (short)(v * 3200.0);
    }
}

static const char *RatingName(int r) {
    static const char *n[6] = {"awful", "ok", "good", "great", "awesome", "perfect"};
    if (r < 0) r = 0;
    if (r > 5) r = 5;
    return n[r];
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/rock-band-3-deluxe/_ark/songs/centerfold/centerfold.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;

    setvbuf(stdout, nullptr, _IONBF, 0);
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);
    gSystemConfig = DataReadString(kConfigDta);

    printf("=== rb3-xenon native M11: multi-part HARMONY scoring topology ===\n");
    printf("mid : %s\n\n", midPath);

    DataArray *trackSyms = DataReadString(
        "(drum guitar bass vocals keys real_keys real_guitar "
        "real_guitar_22fret real_bass real_bass_22fret)");
    DataSetMacro(Symbol("TRACK_SYMBOLS"), trackSyms->Array(0));

    new Scoring();

    // --- Stage 1: REAL SongParser -> SongData (harmony via the sink) ----------
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

    // Enable vocal harmony BEFORE parse-finalize: SongData::GetVocalNoteListCount /
    // GetVocalNoteList and ComputeVocalRangeData all gate on UseVocalHarmony().
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    expertList.SetUseVocalHarmony(true);
    songData.mPlayerTrackConfigList = &expertList;

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
    songData.FixUpTrackConfig(&expertList);
    songData.SetUpTrackDifficulties(&expertList);
    songData.PostLoadVocals(); // real harmony phrase-copy + freestyle intersection

    int nLists = songData.GetVocalNoteListCount();
    printf("--- vocal harmony parts parsed: %d (UseVocalHarmony=%d) ---\n",
           nLists, expertList.UseVocalHarmony());
    if (nLists < 2) {
        printf("  fewer than 2 harmony parts; this chart is not a harmony chart. abort.\n");
        return 1;
    }

    float lastMs = 0.0f;
    std::vector<VocalNoteList *> lists(nLists);
    for (int p = 0; p < nLists; p++) {
        VocalNoteList *l = songData.GetVocalNoteList(p);
        lists[p] = l;
        const std::vector<VocalNote> &notes = l->GetNotes();
        std::vector<VocalPhrase> &phrases = l->GetPhrases();
        int pitched = 0, talky = 0;
        for (size_t i = 0; i < notes.size(); i++)
            (notes[i].mUnpitchedNote ? talky : pitched)++;
        if (!notes.empty()) {
            float e = notes.back().mMs + notes.back().mDurationMs;
            if (e > lastMs) lastMs = e;
        }
        printf("  part %d '%s': %d notes (%d pitched, %d talky), %d phrases\n",
               p, l->GetTrackName().Str(), (int)notes.size(), pitched, talky,
               (int)phrases.size());
    }
    float durationMs = lastMs + 3000.0f;
    printf("  song ~%.1fs\n\n", durationMs / 1000.0f);

    // --- singletons the REAL VocalPlayer::Poll path resolves through ----------
    SongDB *songDB = new SongDB();
    songDB->mSongData = &songData;
    songDB->mSongDurationMs = durationMs;
    TheSongDB = songDB;

    const int kNSingers = 2;
    TheGameMicManager = NativeMakeGameMicManager(kNSingers); // two synthetic mics
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

    // --- REAL VocalPlayer with TWO singers ------------------------------------
    int trackNum = 0;
    VocalPlayer *vp = new VocalPlayer(
        /*user*/ 0, /*bmaster*/ 0, band, trackNum, /*perf*/ 0, /*nsingers*/ kNSingers,
        kExpertDiff, /*native_tag*/ true);
    band->mActivePlayers.push_back(vp);

    vp->mTrack = (VocalTrack *)std::calloc(1, 4096);
    // Headless: no BandUser, so Player::ctor left mTrackType at kTrackNone. The
    // star-rating path (GetSoloNumStars) keys base scores by track type, so set it
    // to the real vocals type here (retail derives it from mUser->GetTrackType()).
    vp->mTrackType = kTrackVocals;
    vp->mCrowd = new CrowdRating(0, (Difficulty)kExpertDiff);
    vp->mBehavior->SetStreakType(Symbol("vocals"));
    vp->mBehavior->SetMaxMultiplier(4);

    vp->PostLoad(true); // builds N VocalParts (one per harmony list) + Singers

    for (size_t i = 0; i < vp->mVocalParts.size(); i++) vp->mVocalParts[i]->Restart(false);
    for (size_t i = 0; i < vp->mSingers.size(); i++) vp->mSingers[i]->Restart(false);
    for (size_t i = 0; i < vp->mVocalParts.size(); i++) vp->mVocalParts[i]->Start();
    for (size_t i = 0; i < vp->mSingers.size(); i++) vp->mSingers[i]->Start();

    int nParts = vp->NumVocalParts();
    printf("--- REAL VocalPlayer (Expert): %d singers, %d vocal parts ---\n",
           vp->NumSingers(), nParts);
    for (int p = 0; p < nParts; p++) {
        VocalPart *vpp = vp->mVocalParts[p];
        printf("  part%d '%s' slop=%.0fms pitchMaxDist=%.2f pitchSigma=%.4f phraseValue=%d\n",
               p, vpp->mVocalNoteList->GetTrackName().Str(), vpp->mSlop,
               vpp->mPitchMaximumDistance, vpp->mPitchSigma, vpp->mPhraseValue);
    }
    printf("\n");

    // ===================== the clock-driven Poll loop =========================
    // Two synthetic singers driven through THREE phases to exercise every
    // assignment/ambiguity regime the greedy matcher can hit:
    //   Phase A [0, T/3)   : singer0 -> HARM1 pitch, singer1 -> HARM2 pitch
    //                        (distinct targets -> clean 1:1 greedy assignment)
    //   Phase B [T/3, 2T/3): BOTH singers -> HARM1 pitch
    //                        (contested part -> greedy must split them; equal
    //                         scores on HARM1==HARM2 unison -> AddAmbiguousPart)
    //   Phase C [2T/3, T]  : singer0 -> HARM2 pitch, singer1 -> HARM3 pitch
    //                        (re-assignment: singers migrate to different parts)
    const float dt = 1000.0f / 60.0f;
    const float t1 = durationMs / 3.0f;
    const float t2 = 2.0f * durationMs / 3.0f;
    const float kEnergy = 0.05f; // comfortably over nonpitch_energy_threshold

    printf("--- clock-driven run (dt=%.2fms, %d singers, %d parts) ---\n",
           dt, kNSingers, nParts);
    printf("  phases: A[0-%.0fs] s0->HARM1 s1->HARM2 | B[%.0f-%.0fs] both->HARM1 | "
           "C[%.0f-%.0fs] s0->HARM2 s1->HARM3\n",
           t1 / 1000.0f, t1 / 1000.0f, t2 / 1000.0f, t2 / 1000.0f,
           durationMs / 1000.0f);
    printf("  %-8s %-5s %-24s %-24s %-10s\n",
           "songMs", "phase", "singer0(pitch->part)", "singer1(pitch->part)", "ambig");

    // per-part phrase-rating accounting
    std::vector<int> partScoredPhrases(nParts, 0), partHitPhrases(nParts, 0);
    std::vector<double> partSumFrac(nParts, 0.0);
    int lastReportedPhrase = -1;

    // per-singer per-part assignment-frame census + ambiguity census
    std::vector<std::vector<long> > assignCensus(kNSingers, std::vector<long>(nParts + 1, 0));
    long ambigFrames = 0, resolvedAmbig = 0;
    long sampledRows = 0;
    // total raw phrase-point pool offered across every part & phrase = the base
    // score the REAL Scoring::ComputeStarThresholds rates against (stars stretch).
    double idealPhrasePool = 0.0;
    // talky waveform stretch: per-singer VoiceBeat DSP response census
    long talkyFrames[2] = {0, 0}, talkyVoicedFrames[2] = {0, 0}, talkyHitFrames[2] = {0, 0};
    float voiceBeatMaxUnk4[2] = {0.0f, 0.0f};
    bool prevTalky[2] = {false, false};
    InitVoicedBurst();

    for (float ms = 0.0f; ms <= durationMs; ms += dt) {
        int phase = (ms < t1) ? 0 : (ms < t2 ? 1 : 2);
        float cms = vp->GetCompensatedTime(ms);

        // per-part target pitch at this instant (0 => no note => that part silent)
        std::vector<float> partPitch(nParts, 0.0f);
        for (int p = 0; p < nParts; p++)
            partPitch[p] = lists[p]->PitchAt(cms);

        // which part each singer is driven to sing this phase
        int drivenPart[2];
        if (phase == 0) {           // s0->HARM1, s1->HARM2 (distinct)
            drivenPart[0] = 0;
            drivenPart[1] = (nParts > 1) ? 1 : 0;
        } else if (phase == 1) {    // both -> HARM1 (contested)
            drivenPart[0] = 0;
            drivenPart[1] = 0;
        } else {                    // s0->HARM2, s1->HARM3 (re-assignment)
            drivenPart[0] = (nParts > 1) ? 1 : 0;
            drivenPart[1] = (nParts > 2) ? 2 : (nParts > 1 ? 1 : 0);
        }

        // drive each mic. If the driven part's current note is UNPITCHED (talky),
        // inject a synthetic voiced burst into the continuous-sample buffer so the
        // REAL TalkyMatcher/VoiceBeat DSP scores it; otherwise drive the pitch.
        float sung[2] = {0.0f, 0.0f};
        bool talky[2] = {false, false};
        for (int s = 0; s < kNSingers; s++) {
            int dp = drivenPart[s];
            const VocalNote *note = lists[dp]->NoteAt(cms);
            if (note && note->mUnpitchedNote) {
                talky[s] = true;
                sung[s] = 0.0f;                     // talky notes carry no pitch
                NativeSetMicFrame(s, 0.0f, kEnergy); // energy over nonpitch thresh
                // Model each talky note as ONE syllable: a clean onset transient
                // (reset filter + inject the voiced burst) on the first frame, then
                // silence. A sustained periodic tone drives the syllable IIR to
                // resonance (inf); real vocal syllables are transient + decaying.
                if (!prevTalky[s]) {
                    vp->mSingers[s]->mTalkyMatcher->Reset();
                    NativeSetMicSamples(s, gVoicedBurst, kBurstSamples);
                } else {
                    NativeSetMicSamples(s, 0, 0);
                }
            } else {
                sung[s] = partPitch[dp];
                NativeSetMicFrame(s, sung[s], sung[s] != 0.0f ? kEnergy : 0.0f);
                NativeSetMicSamples(s, 0, 0);        // silence -> no talky energy
            }
            prevTalky[s] = talky[s];
        }

        // capture each part's real accumulated phrase score just before Poll
        // (Poll resets it inside HandlePhraseEnd at the part[0] boundary)
        bool atEnd = vp->mVocalParts[0]->AtPhraseEnd(cms);
        std::vector<float> endScore(nParts, 0.0f), endMax(nParts, 0.0f);
        int endPhrase = -1;
        if (atEnd) {
            endPhrase = vp->mVocalParts[0]->CurrentPhraseIndex();
            for (int p = 0; p < nParts; p++) {
                endScore[p] = vp->mVocalParts[p]->mPhraseScore;
                endMax[p] = vp->mVocalParts[p]->mPhraseScoreMax;
            }
        }

        // ===== the REAL orchestration =====
        SongPos pos = songData.CalcSongPos(ms);
        vp->Poll(ms, pos);

        // talky / VoiceBeat DSP observation (REAL syllable-onset detector fed the
        // synthetic voiced burst above)
        for (int s = 0; s < kNSingers; s++) {
            if (!talky[s]) continue;
            talkyFrames[s]++;
            VoiceBeat &vb = vp->mSingers[s]->mTalkyMatcher->mVoiceBeat;
            if (vb.unk4 > voiceBeatMaxUnk4[s]) voiceBeatMaxUnk4[s] = vb.unk4;
            if (vb.unk1) talkyVoicedFrames[s]++;                 // ratio>0.3 voiced
            if (vb.unk1 && !vb.unk0 && vb.unk4 > 4.5e-4f)
                talkyHitFrames[s]++;                             // scoring-hit window
        }

        // observe assignment + ambiguity post-Poll
        bool anySung = (sung[0] != 0.0f) || (sung[1] != 0.0f) || talky[0] || talky[1];
        for (int s = 0; s < kNSingers; s++) {
            int ap = vp->mSingers[s]->GetFrameAssignedPart();
            assignCensus[s][ap < 0 ? nParts : ap]++;
        }
        long frameAmbig = 0, frameResolved = 0;
        for (int s = 0; s < kNSingers; s++) {
            std::vector<Singer::AmbiguousData> &ad = vp->mSingers[s]->mAmbiguousData;
            frameAmbig += (long)ad.size();
            for (size_t k = 0; k < ad.size(); k++)
                if (ad[k].isResolved) frameResolved++;
        }
        if (frameAmbig) { ambigFrames++; resolvedAmbig += frameResolved; }

        // sample a handful of live rows around phase boundaries + when contested
        bool nearBoundary = (fabsf(ms - t1) < 3 * dt) || (fabsf(ms - t2) < 3 * dt);
        if (anySung && ((sampledRows < 6) || nearBoundary || (frameAmbig > 0 && sampledRows < 40))
            && (long)(ms / dt) % 7 == 0) {
            char b0[48], b1[48];
            std::snprintf(b0, sizeof b0, "%.1f->p%d(tgt%.1f)",
                          vp->mSingers[0]->GetFrameMicPitch(),
                          vp->mSingers[0]->GetFrameAssignedPart(),
                          vp->mSingers[0]->mBestTargetPitch);
            std::snprintf(b1, sizeof b1, "%.1f->p%d(tgt%.1f)",
                          vp->mSingers[1]->GetFrameMicPitch(),
                          vp->mSingers[1]->GetFrameAssignedPart(),
                          vp->mSingers[1]->mBestTargetPitch);
            printf("  %-8.0f %-5c %-24s %-24s %ld\n",
                   ms, 'A' + phase, b0, b1, frameAmbig);
            sampledRows++;
        }

        // per-part phrase-end ratings (all parts close together at part[0] boundary)
        if (atEnd && endPhrase != lastReportedPhrase) {
            lastReportedPhrase = endPhrase;
            for (int p = 0; p < nParts; p++) idealPhrasePool += endMax[p];
            bool printed = false;
            for (int p = 0; p < nParts; p++) {
                if (endMax[p] <= 0.0f) continue;
                float frac = endScore[p] / endMax[p];
                if (frac > 1.0f) frac = 1.0f;
                int rating = vp->CalculatePhraseRating(frac);
                partScoredPhrases[p]++;
                partSumFrac[p] += frac;
                if (rating > 0) partHitPhrases[p]++;
                if (partScoredPhrases[0] <= 8 || partScoredPhrases[0] % 6 == 0) {
                    if (!printed) {
                        printf("  [phrase %d @%.0fms] ", endPhrase, ms);
                        printed = true;
                    }
                    printf("p%d %.2f/%.2f=%.2f(%s) ", p, endScore[p], endMax[p],
                           frac, RatingName(rating));
                }
            }
            if (printed) printf("\n");
        }
    }

    // ============================ final results =============================
    printf("\n--- SONG COMPLETE ---\n");
    printf("  per-part phrase ratings (REAL CalculatePhraseRating):\n");
    for (int p = 0; p < nParts; p++) {
        printf("    part%d '%s': %d scored (%d hit, %d missed), avg frac %.3f, "
               "overall hit%% %.1f\n",
               p, lists[p]->GetTrackName().Str(), partScoredPhrases[p],
               partHitPhrases[p], partScoredPhrases[p] - partHitPhrases[p],
               partScoredPhrases[p] ? partSumFrac[p] / partScoredPhrases[p] : 0.0,
               vp->mVocalParts[p]->GetOverallPartHitPercentage() * 100.0f);
    }
    printf("\n  greedy singer<->part assignment census (frames assigned to each part):\n");
    for (int s = 0; s < kNSingers; s++) {
        printf("    singer%d: ", s);
        for (int p = 0; p < nParts; p++)
            printf("part%d=%ld ", p, assignCensus[s][p]);
        printf("unassigned=%ld\n", assignCensus[s][nParts]);
    }
    printf("\n  ambiguity: %ld frames with >=1 ambiguous part-pair, %ld resolved "
           "(REAL Singer::AddAmbiguousPart/AllScoresAreIn/ResolveAmbiguity)\n",
           ambigFrames, resolvedAmbig);
    printf("\n  talky waveform -> REAL TalkyMatcher/VoiceBeat DSP (synthetic voiced burst):\n");
    for (int s = 0; s < kNSingers; s++) {
        printf("    singer%d: %ld talky-note frames, %ld voiced(unk1), %ld in scoring-hit "
               "window (unk1&!unk0&E>thr), peak sylEnergy=%.4g\n",
               s, talkyFrames[s], talkyVoicedFrames[s], talkyHitFrames[s],
               voiceBeatMaxUnk4[s]);
    }

    printf("\n  combined player score: %d  (REAL Player::GetScore via per-part AddScore/AddPoints)\n",
           vp->GetScore());

    // --- stars baseline: REAL Scoring::ComputeStarThresholds over a defined base --
    // Populate SongDB::GetBaseScores() with the vocal base score (= the raw phrase-
    // point pool offered across all parts/phrases), then run the genuine engine
    // star pipeline (ComputeStarThresholds -> GetSoloNumStars). The base is the
    // pre-streak-multiplier point pool, so the multiplied achieved score can cross
    // the lower thresholds -> a conservative but REAL star rating.
    int idealBase = (int)idealPhrasePool;
    std::vector<PlayerScoreInfo> &baseScores = songDB->GetBaseScores();
    baseScores.clear();
    baseScores.push_back(
        PlayerScoreInfo(kTrackVocals, kDifficultyExpert, idealBase, idealBase, 0));
    TheScoring->ComputeStarThresholds(false);
    printf("  base score pool      : %d  (Σ every part's every phrase max)\n", idealBase);
    printf("  solo-star thresholds : ");
    for (int s = 1; s <= 5; s++) printf("%d* @%d  ", s, vp->GetScoreForStars(s));
    printf("\n  stars                : %d  (%.2f)  (REAL Scoring::GetSoloNumStars)\n",
           vp->GetNumStars(), vp->GetNumStarsFloat());
    printf("\n  (all pitch matching, greedy singer<->part assignment, ambiguity\n"
           "   resolution, per-part phrase accumulation, harmony phrase-copy, and\n"
           "   rating via REAL engine code; only the two microphone streams are synthetic.)\n");
    printf("\nDone.\n");
    return 0;
}
