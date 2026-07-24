// rb3-xenon native M9 — the vocal pitch-track run-through.
//
// The one untouched instrument domain. This driver parses a REAL chart's
// PART VOCALS through the REAL SongParser -> SongData -> VocalNoteList path
// (the same Stage-1 parse the M8 score driver uses), prints the genuine vocal
// note / phrase / lyric table, then drives a synthetic "singer": a pitch stream
// fed frame-by-frame into the REAL VocalPart pitch-matching engine
// (ScoreNote / GetBestHit / GetSloppyPitch / GetNoteSliceWeight / CalcNoteWeights
// / CalcPhraseScoreMax). Perfect-pitch segments score ~1.0 per note and credit
// their phrases; off-pitch (tritone) segments score 0 and miss. A final vocal
// score + star rating falls out of the REAL rating thresholds.
//
// REAL vs SHIM census (see the report):
//   REAL : SongParser -> SongData -> VocalNoteList vocal parse (pitches, ticks,
//          ms, durations, lyrics, phrases, freestyle sections); SongData::
//          PostLoadVocals finalize (NotesDone / DeterminePhraseTimes / phrase
//          copy / freestyle intersect); the entire VocalPart pitch-matching
//          engine — VocalPart::SetDifficultyVariables (real retail scoring.dta
//          numbers), CalcNoteWeights, GetNoteSliceWeight, GetNoteRange,
//          GetSloppyPitch, ScoreNote (the Gaussian pitch-distance matcher with
//          octave folding), GetBestHit, CalcPhraseScoreMax; the rating-threshold
//          mapping (VocalPlayer::CalculatePhraseRating logic over the real
//          rating_thresholds array).
//   SHIM : the microphone. There is no DSP/PitchDetector/Fonix here — the mic
//          pitch+energy is a synthetic stream (that is the "only the mic input
//          is synthetic" contract). The VocalPart object is brought up with a
//          documented calloc + real-method-init shim (VocalPart has no vtable;
//          its ctor's sole dependency is mPlayer->GetUser()->GetDifficulty(),
//          which the pitched ScoreNote/GetBestHit path never touches — mPlayer
//          stays null). The full VocalPlayer::Poll orchestration (greedy
//          singer<->part assignment, Singer/TalkyMatcher/VibratoDetector,
//          overlay, tambourine, net) is the M10 frontier, not exercised here.

#include "game/VocalPart.h"
#include "beatmatch/VocalNote.h"
#include "beatmatch/SongParser.h"
#include "beatmatch/SongData.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/TuningOffsetList.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/TrackType.h"
#include "utl/BeatMap.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"
#include "utl/TempoMap.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
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

// NativeSongInfo — the same concrete SongInfo the M8 driver uses (SongInfo is
// abstract; the parse reads track/pan/vol/vocal-part hints).
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

// ---- the real retail (scoring.dta) vocal scoring block. VocalPart::SetDifficulty-
// Variables reads (scoring (vocals ...)); values transcribed verbatim from the
// extracted retail config (../rb3/orig-assets/extracted/config/scoring.dta). Diff
// arrays are indexed ->Float(diff+1), so index 0 is the key and 1..4 are the four
// difficulties (easy..expert).
static const char *kConfigDta =
    // (beatmatcher ...) — SongParser reads player_slot / vocal pitch range /
    // track_mapping from here (same block M8 uses).
    "(beatmatcher"
    "   (parser"
    "      (player_slot 9)"
    // kTrackVocals=3 -> routes the VOCALS track to the vocal-note parser
    // (mState=kVocalNotes) instead of the gem parser. Without this the vocal
    // track is (wrongly) read as gems and no VocalNotes are emitted.
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
    "(scoring"
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
    "      (freestyle_deployment_time (500 400))"
    "      (freestyle_min_duration (600 500))"
    "      (freestyle_pad (100 50))"
    "      (synapse_proximity_solo 0.78 0.78 0.78 0.78)"
    "      (synapse_focus_solo 1.0e-4 1.0e-4 1.0e-4 1.0e-4)"
    "      (synapse_proximity_harm 0.956 0.956 0.956 0.956)"
    "      (synapse_focus_harm 1.0e-4 1.0e-4 1.0e-4 1.0e-4)))";

// The one shim: bring up a REAL VocalPart without the VocalPlayer-coupled ctor.
// VocalPart has no virtuals (no vtable), so a zeroed buffer is a valid object;
// the zeroed std::vector<float> mNoteWeights is STLport's empty-vector state,
// which CalcNoteWeights (clear/reserve/push_back) handles. We then run the REAL
// SetDifficultyVariables + CalcNoteWeights. mPlayer stays null (never touched on
// the pitched ScoreNote/GetBestHit path). This mirrors M8's calloc'd-Game shim.
static VocalPart *MakeRealVocalPart(int partIndex, VocalNoteList *list, int diff) {
    VocalPart *part = static_cast<VocalPart *>(std::calloc(1, sizeof(VocalPart)));
    part->mPlayer = 0;
    part->mPartIndex = partIndex;
    part->mScoringEnabled = true;
    part->mPhraseScorePartMultiplier = 1.0f;
    part->unk9c = FLT_MAX;
    part->unka0 = -FLT_MAX;
    part->mBestSingerPitchDistance = FLT_MAX;
    part->unkc8 = 6;
    part->unkbc = -1.0f;
    part->mPhraseRank = 0;
    part->SetDifficultyVariables(diff);            // REAL
    part->mVocalNoteList = list;                   // REAL parsed list
    part->mFreestyleSection = list->mFreestyleSections.begin();
    part->CalcNoteWeights();                       // REAL
    part->mThisPhrase = list->mPhrases.begin();
    return part;
}

static const char *NoteName(int midi) {
    static const char *n[12] = {"C", "C#", "D", "D#", "E", "F",
                                "F#", "G", "G#", "A", "A#", "B"};
    static char buf[8];
    std::snprintf(buf, sizeof(buf), "%s%d", n[midi % 12], midi / 12 - 1);
    return buf;
}

// REAL rating map (VocalPlayer::CalculatePhraseRating over rating_thresholds).
static int RateFraction(DataArray *thresh, float f) {
    for (int i = thresh->Size() - 1; i >= 1; i--)
        if (f > thresh->Float(i)) return i;
    return 0;
}

int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-grinnz/tool/vicarious/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;
    const char *partName = (argc >= 3) ? argv[2] : "PART VOCALS";

    setvbuf(stdout, nullptr, _IONBF, 0);
    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);
    gSystemConfig = DataReadString(kConfigDta);

    printf("=== rb3-xenon native M9: vocal pitch-track run-through ===\n");
    printf("mid : %s\npart: %s\n\n", midPath, partName);

    // --- Stage 1: REAL SongParser -> SongData (vocals populate via the sink) ---
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

    // Bind the parser's tempo/measure-map out-references directly to the SongData
    // members (not to locals as M8 does). Vocal parsing warns via
    // VocalNoteList::AddNote -> mSongData->GetMeasureMap() *during* the parse, so
    // the measure map must be live on the SongData before the loop completes.
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

    // Track config (Expert) — ComputeVocalRangeData reads
    // mPlayerTrackConfigList->UseVocalHarmony(); mirror the M8 setup.
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    songData.mPlayerTrackConfigList = &expertList;
    songData.FixUpTrackConfig(&expertList);
    songData.SetUpTrackDifficulties(&expertList);

    // Finalize the vocal note lists (phrase times, lyric phrases, freestyle
    // sections) — the REAL SongData::PostLoadVocals.
    songData.PostLoadVocals();

    int nLists = songData.GetVocalNoteListCount();
    printf("--- vocal note lists parsed: %d ---\n", nLists);
    if (nLists == 0) {
        printf("  no vocal note lists on this chart; abort.\n");
        return 1;
    }

    // --- Stage 2: print the REAL vocal note / phrase / lyric table ------------
    VocalNoteList *list = songData.GetVocalNoteList(0); // solo vocals (part 0)
    const std::vector<VocalNote> &notes = list->GetNotes();
    std::vector<VocalPhrase> &phrases = list->GetPhrases();
    printf("\n--- Stage 2: REAL vocal note table (part 0 '%s': %d notes, %d phrases) ---\n",
           list->GetTrackName().Str(), (int)notes.size(), (int)phrases.size());
    printf("  %-4s %9s %9s %6s %-9s %-5s %-4s  %s\n",
           "#", "ms", "durMs", "pitch", "note", "type", "bend", "lyric");
    int shown = 0;
    for (int i = 0; i < (int)notes.size(); i++) {
        const VocalNote &nt = notes[i];
        if (shown < 40) {
            const char *type = nt.mUnpitchedNote ? "talky" : "pitch";
            char pitchbuf[16];
            if (nt.mUnpitchedNote)
                std::snprintf(pitchbuf, sizeof(pitchbuf), "-");
            else if (nt.mBeginPitch == nt.mEndPitch)
                std::snprintf(pitchbuf, sizeof(pitchbuf), "%d", nt.mBeginPitch);
            else
                std::snprintf(pitchbuf, sizeof(pitchbuf), "%d>%d", nt.mBeginPitch, nt.mEndPitch);
            printf("  %-4d %9.1f %9.1f %6s %-9s %-5s %-4s  \"%s\"\n",
                   i, nt.mMs, nt.mDurationMs, pitchbuf,
                   nt.mUnpitchedNote ? "-" : NoteName(nt.mBeginPitch),
                   type, nt.mBends ? "yes" : "no", nt.mText.c_str());
        }
        shown++;
    }
    if ((int)notes.size() > 40)
        printf("  ... (%d more notes)\n", (int)notes.size() - 40);

    // phrase summary
    int pitchedNotes = 0, talkyNotes = 0;
    for (int i = 0; i < (int)notes.size(); i++)
        (notes[i].mUnpitchedNote ? talkyNotes : pitchedNotes)++;
    printf("\n  notes: %d pitched, %d talky/unpitched;  phrases: %d\n",
           pitchedNotes, talkyNotes, (int)phrases.size());

    // --- Stage 3: bring up the REAL VocalPart pitch-matching engine -----------
    DataArray *voxCfg = SystemConfig("scoring", "vocals");
    DataArray *ratingThresh = voxCfg->FindArray("rating_thresholds");
    VocalPart *part = MakeRealVocalPart(0, list, kExpertDiff);
    printf("\n--- Stage 3: REAL VocalPart (Expert) difficulty vars ---\n");
    printf("  slop=%.0fms  pitchMaxDist=%.2f  pitchSigma=%.4f  phraseValue=%d\n",
           part->mSlop, part->mPitchMaximumDistance, part->mPitchSigma, part->mPhraseValue);
    printf("  ratingThresholds = { %.2f %.2f %.2f %.2f }  (real config)\n",
           ratingThresh->Float(1), ratingThresh->Float(2),
           ratingThresh->Float(3), ratingThresh->Float(4));

    // --- Stage 4: drive the synthetic singer over the real chart --------------
    // Policy: sing the FIRST HALF of the phrases perfectly (on-pitch), the SECOND
    // HALF a tritone (6 semitones) off. Perfect -> ScoreNote ~1.0 (HIT); off ->
    // ScoreNote 0 (MISS). Each note is sampled at its center time; per-phrase we
    // sum ScoreNote*noteWeight (REAL) and divide by the REAL CalcPhraseScoreMax to
    // get the phrase fraction, then RateFraction (REAL thresholds) -> stars.
    printf("\n--- Stage 4: synthetic singer over the REAL matcher ---\n");
    printf("  policy: phrases in first half sung on-pitch, second half +6 semis (off)\n\n");
    printf("  %-6s %9s %9s %7s %8s %6s  %s\n",
           "phrase", "startMs", "endMs", "notes", "frac", "rating", "verdict");

    int nPitchedPhrases = 0;
    for (int p = 0; p < (int)phrases.size(); p++)
        if (phrases[p].unk10 != phrases[p].unk14) nPitchedPhrases++;
    int halfway = nPitchedPhrases / 2;

    double totalAchieved = 0.0, totalMax = 0.0;
    int phrasesHit = 0, phrasesMissed = 0, scoredPhrases = 0;
    int totalNoteHits = 0, totalNoteScored = 0;

    int pitchedPhraseIdx = 0;
    for (int p = 0; p < (int)phrases.size(); p++) {
        VocalPhrase &ph = phrases[p];
        if (ph.unk10 == ph.unk14) continue; // empty phrase (no notes)
        bool singOff = (pitchedPhraseIdx >= halfway);
        pitchedPhraseIdx++;

        float phraseMax = part->CalcPhraseScoreMax(&ph); // REAL
        if (!(phraseMax > 0.0f)) continue; // skip empty / degenerate (nan/inf) phrases

        float achieved = 0.0f;
        int noteCount = 0, noteHits = 0;
        for (int i = ph.unk10; i < ph.unk14 && i < (int)notes.size(); i++) {
            const VocalNote &nt = notes[i];
            if (nt.mUnpitchedNote) continue; // talky: needs Singer+TalkyMatcher (M10)
            noteCount++;

            // synthetic sung pitch: the REAL target at the note center (GetSloppyPitch
            // interpolates bends), then optionally detuned a tritone for the "off" half.
            float centerMs = nt.mMs + nt.mDurationMs * 0.5f;
            float outPitch;
            float target = part->GetSloppyPitch(centerMs, i, (float)nt.mBeginPitch, outPitch); // REAL
            float sung = singOff ? (target + 6.0f) : target;

            // REAL Gaussian pitch matcher.
            float pitch = sung;
            int octaves = 0;
            float sloppyOut, arg5;
            float score = part->ScoreNote(centerMs, i, pitch, octaves, sloppyOut, arg5); // REAL

            achieved += score * part->mNoteWeights[i];
            if (score > 0.5f) noteHits++;
        }
        if (noteCount == 0) continue;

        float frac = achieved / phraseMax;
        if (frac > 1.0f) frac = 1.0f;
        int rating = RateFraction(ratingThresh, frac); // REAL threshold map
        bool hit = rating > 0;

        totalAchieved += achieved;
        totalMax += phraseMax;
        totalNoteHits += noteHits;
        totalNoteScored += noteCount;
        scoredPhrases++;
        if (hit) phrasesHit++; else phrasesMissed++;

        // show the on-pitch stretch (first ~16) and the off-pitch stretch (last
        // ~14) so both HIT and MISS verdicts are visible.
        if (scoredPhrases <= 16 || scoredPhrases > nPitchedPhrases - 14) {
            float startMs = ph.unk0;
            float endMs = ph.unk0 + ph.unk4;
            printf("  %-6d %9.1f %9.1f %5d/%d %8.3f %6d  %s\n",
                   p, startMs, endMs, noteHits, noteCount, frac, rating,
                   hit ? "HIT" : "MISS");
        } else if (scoredPhrases == 17) {
            printf("  ...\n");
        }
    }

    // --- final vocal score + stars (real ideal-relative fraction) ------------
    printf("\n--- final ---\n");
    float overallFrac = totalMax > 0.0 ? (float)(totalAchieved / totalMax) : 0.0f;
    printf("  scored phrases : %d  (%d hit, %d missed)\n",
           scoredPhrases, phrasesHit, phrasesMissed);
    printf("  note hits      : %d / %d  (%.1f%%)\n",
           totalNoteHits, totalNoteScored,
           totalNoteScored ? 100.0 * totalNoteHits / totalNoteScored : 0.0);
    printf("  achieved/max   : %.1f / %.1f  = %.3f overall pitch fraction\n",
           totalAchieved, totalMax, overallFrac);

    // Star mapping via the same rating thresholds (0..4 -> up to gold at top).
    int overallRating = RateFraction(ratingThresh, overallFrac);
    static const char *stars[5] = {"0 (failed)", "1 star", "2 stars", "3 stars",
                                    "4 stars"};
    printf("  overall rating : %d -> %s\n", overallRating,
           stars[overallRating < 0 ? 0 : (overallRating > 4 ? 4 : overallRating)]);
    printf("\n  (perfect-pitch phrases credited; tritone-off phrases missed — all\n"
           "   scoring via REAL VocalPart::ScoreNote / CalcPhraseScoreMax / thresholds)\n");

    // --- Stage 5: full-range sanity (all-perfect vs all-off) -----------------
    // Same REAL matcher, two extreme synthetic singers, aggregate only — shows
    // the score spans the whole 0-star..4-star range off real chart + real math.
    printf("\n--- Stage 5: full-range check (same REAL matcher) ---\n");
    printf("  %-14s %10s %10s %8s  %s\n", "singer", "noteHit%", "achieved", "frac", "rating");
    for (int mode = 0; mode < 2; mode++) {
        float detune = (mode == 0) ? 0.0f : 6.0f; // perfect vs tritone-off
        double ach = 0.0, mx = 0.0;
        int nh = 0, ns = 0;
        for (int p = 0; p < (int)phrases.size(); p++) {
            VocalPhrase &ph = phrases[p];
            if (ph.unk10 == ph.unk14) continue;
            float pmax = part->CalcPhraseScoreMax(&ph);
            if (!(pmax > 0.0f)) continue;
            float a = 0.0f;
            int cnt = 0;
            for (int i = ph.unk10; i < ph.unk14 && i < (int)notes.size(); i++) {
                const VocalNote &nt = notes[i];
                if (nt.mUnpitchedNote) continue;
                cnt++;
                float centerMs = nt.mMs + nt.mDurationMs * 0.5f;
                float outP;
                float tgt = part->GetSloppyPitch(centerMs, i, (float)nt.mBeginPitch, outP);
                float pitch = tgt + detune;
                int oct = 0;
                float sp, a5;
                float sc = part->ScoreNote(centerMs, i, pitch, oct, sp, a5);
                a += sc * part->mNoteWeights[i];
                if (sc > 0.5f) nh++;
                ns++;
            }
            if (cnt == 0) continue;
            ach += a;
            mx += pmax;
        }
        float f = mx > 0.0 ? (float)(ach / mx) : 0.0f;
        if (f > 1.0f) f = 1.0f;
        int r = RateFraction(ratingThresh, f);
        printf("  %-14s %9.1f%% %10.1f %8.3f  %d -> %s\n",
               mode == 0 ? "all-perfect" : "all-off (+6)",
               ns ? 100.0 * nh / ns : 0.0, ach, f, r,
               stars[r < 0 ? 0 : (r > 4 ? 4 : r)]);
    }

    return 0;
}
