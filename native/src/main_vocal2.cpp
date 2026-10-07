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
// The ONLY synthetic input is the microphone: GameMic::mLastPitch / mLastEnergy
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
#include "game/GameConfig.h"
#include "game/SongDB.h"
#include <string>
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
void DataInit();
void SetTheBeatMap(BeatMap *);

// m8_support.cpp (W16-PJ: real SongDB bring-up)
Game *NativeMakeGame();
GameConfig *NativeMakeGameConfig(PlayerTrackConfigList *, float);
void NativeSongDBPostLoad(SongDB *, float);
// m10_support.cpp
GameMicManager *NativeMakeGameMicManager(int nSingers);
void NativeSetMicFrame(int i, float pitch, float energy);
NetSession *NativeMakeNetSession();
extern GameMicManager *TheGameMicManager;

#include "utl/SongInfoCopy.h"
#include "utl/SongInfoAudioType.h"
#include "retail_boot_macros.h"
#include "retail_system_config.h"
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

// ---------------------------------------------------------------- config ----
// SystemConfig() -- (beatmatcher ...) for the SongParser, (scoring ...) for
// Scoring / PlayerParams / Band / CrowdRating / the MultiplayerAnalyzer, and
// every other section -- and the macro table (TRACK_SYMBOLS for SymToTrackType,
// kDifficulty*) are retail's post-SystemInit config read off the disc
// (retail_system_config.h, W16-UD). Before W16-UD this driver typed in its own
// (beatmatcher ...)/(scoring ...) blocks, spliced in crowd/solo/coda blocks cut
// from a host TEXT extraction, and defined its own TRACK_SYMBOLS; W16-UD's lane
// doc lists where those disagreed with retail.

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
    RetailBootMacros::Define(); // retail's boot DTA macros, before any read (W16-UA)
    ObjectDir::PreInit(256, 4096);
    if (int rc = RetailSystemConfig::Boot()) // retail's config + macros, off the disc (W16-UD)
        return rc;

    printf("=== rb3-xenon native M10: full vocal-gameplay orchestration ===\n");
    printf("mid : %s\n\n", midPath);


    new Scoring(); // sets TheScoring (real star-threshold + solo-award machinery)

    // --- Stage 1: REAL SongParser -> SongData (vocals via the sink) -----------
    // TheGame first: the REAL SongDB sink (AddPhrase) consults it during
    // SongData::PostLoad below.
    Game *game = NativeMakeGame();
    // W16-PJ: parse INTO the real SongDB's own SongData (its ctor registers the
    // SongDB as that SongData's parser sink and builds its MultiplayerAnalyzer).
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
    for (size_t i = 0; i < songData.mGemDBs.size(); i++)
        songData.mGemDBs[i]->MergeChordGems();
    PlayerTrackConfigList expertList(1);
    expertList.mDefaultDifficulty = kExpertDiff;
    // W16-PJ: one REAL player config (a generated UserGuid on the vocals track at
    // Expert) + the REAL SongData::PostLoad, which runs PostLoadVocals itself
    // (see main_score4.cpp for the full rationale).
    UserGuid playerGuid;
    playerGuid.Generate();
    expertList.AddConfig(playerGuid, kTrackVocals, kExpertDiff, 0, false);
    songData.mPlayerTrackConfigList = &expertList;
    NativeMakeGameConfig(&expertList, 0.0f);
    songData.PostLoad(&expertList);

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
    // W16-PJ: the event-independent half of the REAL SongDB::PostLoad (see
    // m8_support.cpp). Runs before the VocalPlayer exists, as in the game (the
    // SongDB loads before the players are built).
    NativeSongDBPostLoad(songDB, durationMs);

    TheGameMicManager = NativeMakeGameMicManager(1); // one synthetic mic (solo)
    TheNetSession = NativeMakeNetSession();

    game->mIsPaused = false; // (TheGame itself is made before the parse)

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
    // meter needs it (the REAL CrowdRating since W16-PD; its config is (scoring (crowd ...)) off the disc).
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
