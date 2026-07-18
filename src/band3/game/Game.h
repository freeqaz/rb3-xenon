#pragma once
#include "Shuttle.h"
#include "beatmatch/BeatMaster.h"
#include "beatmatch/BeatMasterSink.h"
#include "beatmatch/FillInfo.h"
#include "game/Band.h"
#include "game/BandUser.h"
#include "game/Player.h"
#include "game/SongDB.h"
#include "game/TrackerManager.h"
#include "math/Interp.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/DiscErrorMgr_Wii.h"
#include "os/Timer.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongPos.h"

enum GameState {
    kGameNeedIntro = 0,
    kGameNeedStart = 1,
    kGamePlaying = 2,
    kGameOver = 3
};

enum EndGameResult {
    kRestart = 0,
    kLost = 1,
    kWon = 2,
    kWonFinale = 3,
    kSkip = 4,
    kQuit = 5
};

// Retail Xbox does NOT inherit the Wii-only DiscErrorMgrWii::Callback base
// (that third polymorphic base's vptr pushed mProperties to 0x30). Retail
// keeps only BeatMasterSink + Hmx::Object, placing mProperties at 0x2c —
// verified against retail disasm (GemPlayer::CanFlail / GemManager::IsSpotlightGem
// read mProperties bools 4 bytes lower than our old 3-base layout).
class Game : public BeatMasterSink, public Hmx::Object {
public:
    enum LoadState {
        kLoadingSong = 0,
        kWaitingForAudio = 1,
        kReady = 2
    };

    struct Properties {
        Properties();

        bool mInTrainer; // 0x0
        bool mInDrumTrainer; // 0x1
        bool mInPracticeMode; // 0x2
        // Prop+0x3 (this+0x2f): NOT AllowOverdrivePhrases (that field is at
        // Prop+0x5 / this+0x31 -- see GetCommonPhraseID/IsSpotlightGem/etc,
        // both proven by objdiff diff_arg mismatches: base wanted 0x31, our
        // header previously gave 0x2f here). This slot is some other
        // TU5-added bool; Game::Poll's movie-sync block reads exactly this
        // address for an unidentified condition (kept as a raw placeholder
        // there -- see mUnkTU5_movieSync usage). TODO: identify real meaning.
        bool mUnkTU5_movieSync; // 0x3 (was misnamed mAllowOverdrivePhrases)
        // TU5: two bools inserted here (Properties 25->29 bytes). Proven by
        // CanUserPause reading mEndWithSong at Prop+0x6 (0x32) vs base 0x30,
        // and retail Poll reading a bool at Prop+0x4 (0x30) where our source
        // still calls MetaPerformer::IsPlayingDemo. Growing Properties by 4
        // also re-aligns mSongPos 0x48->0x4c, giving every post-Properties
        // member the observed +4 shift. TODO: identify (mIsPlayingDemo?).
        bool mUnkTU5_prop4; // 0x4 (new in TU5)
        // This is the REAL AllowOverdrivePhrases (proven by GetCommonPhraseID
        // and GemManager::IsSpotlightGem objdiff: both want a byte load at
        // this+0x31 = Prop+0x5, not the old Prop+0x3). Previously named
        // mUnkTU5_prop5 as an unidentified TU5 tail bool; repurposed here.
        bool mAllowOverdrivePhrases; // 0x5 (was mUnkTU5_prop5)
        bool mEndWithSong; // 0x6 (was 0x4)
        bool mForceUseCymbals; // 0x5
        bool mForceDontUseCymbals; // 0x6
        bool mAllowAutoVocals; // 0x7
        bool mHasSongSections; // 0x8
        bool mLoadChars; // 0x9
        bool mLetterbox; // 0xa
        bool mCrowdReacts; // 0xb
        bool mIsPractice; // 0xc
        bool mEnableWhammy; // 0xd
        bool mEnableCapstrip; // 0xe
        bool mDisableGuitarFx; // 0xf
        bool mDisableKeysFx; // 0x10
        bool mEnableOverdrive; // 0x11
        bool mEnableCoda; // 0x12
        bool mCanSolo; // 0x13
        bool mHasBeatMask; // 0x14
        bool mCanLose; // 0x15
        bool mEnableStreak; // 0x16
        bool mShowStars; // 0x17
        bool mPlayStarSfx; // 0x18
        // TU5: two more bools bring Properties to 29 bytes total (the +4 that
        // re-aligns mSongPos 0x48->0x4c). Placed at the tail because no loss
        // function reads a late Properties bool; only their count matters for
        // layout. TODO: identify + relocate to the real insertion points.
        bool mUnkTU5_prop19; // 0x19 (new in TU5)
        bool mUnkTU5_prop20; // 0x1a (new in TU5)
    };
    Game();
    virtual ~Game();
    virtual void Beat(int, int) {}
    virtual void UpdateSongPos(const SongPos &) {}
    virtual void HandleSubmix(int, const char *) {}
    virtual DataNode Handle(DataArray *, bool);

    void SetPaused(bool, bool, bool);
    void SetGameOver(bool);
    bool ResumedNoScore() const;
    bool IsActiveUser(BandUser *) const;
    bool IsWaiting();
    int NumActivePlayers() const;
    void AddBonusPoints(BandUser *, int, int);
    void OnPlayerAddEnergy(Player *, float);
    void OnRemoteTrackerFocus(Player *, int, int, int);
    void OnRemoteTrackerPlayerProgress(Player *, float);
    void OnRemoteTrackerSectionComplete(Player *, int, int, int);
    void OnRemoteTrackerPlayerDisplay(Player *, int, int, int);
    void OnRemoteTrackerDeploy(Player *);
    void OnRemoteTrackerEndDeployStreak(Player *, int);
    void OnRemoteTrackerEndStreak(Player *, int, int);
    void OnPlayerQuarantined(Player *);
    Band *GetBand();
    void ForceTrackerStars(int);
    void RemovePlayer(Player *);
    void OnPlayerRemoved(Player *);
    void SetBackgroundVolume(float);
    void SetForegroundVolume(float);
    void SetStereo(bool);
    void PopulatePlayerLists();
    void LoadSong();
    bool IsLoaded();
    void PostLoad();
    void PrintBasePoints();
    void ResetVoiceChatState();
    bool IsReady();
    void Start();
    void Go();
    void AddPlayer(BandUser *);
    void SetRealtime(bool);
    void StartIntro();
    void Reset();
    bool HasIntro();
    float GetSongToTaskMgrMs();
    float GetSongMs() const;
    void UpdatePausedState(bool, bool);
    bool CanUserPause() const;
    void Restart(bool);
    void Poll();
    ExcitementLevel GetCrowdExcitement();
    void SetVocalPercussionBank(ObjectDir *);
    void SetDrumKitBank(ObjectDir *);
    Player *GetPlayerFromTrack(int, bool) const;
    float GetMusicSpeed() const;
    void SetMusicSpeed(float);
    void SetPitchMucker(bool);
    void SetMusicVolume(float);
    void SetIntroRealTime(float);
    int GetScoringTracks() const;
    EndGameResult GetResult(bool);
    EndGameResult GetResultForUser(BandUser *);
    Player *GetActivePlayer(int) const;
    void Jump(float, bool);
    void CheckRollbackEnd(float);
    void Replay();
    void Rollback(float, float);
    void EnableWorldPolling(bool);
    bool HandleRollbackAnimation();
    void ResetAudio();
    void SetVocalCueVolume(float);
    void AddMusicFader(Fader *);
    Performer *GetMainPerformer();
    bool AllowInput() const;
    void SetKickAutoplay(bool);
    void SetVocalPercussionBank(Player *, ObjectDir *);
    void SetDrumKitBank(Player *, ObjectDir *);
    void DropUser(BandUser *);
    void AddUser(BandUser *);
    void ReconcilePlayers();
    void SetInvalidScore(bool);
    void SetSkippedSong(bool);
    void SetResumeFraction(float);
    bool IsInvalidScore() const;
    bool SkippedSong() const;
    void OnPlayerSaved(Player *);
    void OvershellSetPaused(bool);
    Symbol GetSectionAtMs(float) const;
    void NeverAllowInput(bool b) { mNeverAllowInput = b; }
    float GetFractionCompleted() const;
    void OnStatsSynced();
    void AdjustForVocalPhrases(float &, float &) const;
    void ClearState();
    void E3CheatAutoplayAccuracy();
    const char *DebugCycleAutoplay();
    const char *DebugCycleAutoplayAccuracy();
    void SetNoFail(bool);

    bool InTrainer() const { return mProperties.mInTrainer; }
    bool InDrumTrainer() const { return mProperties.mInDrumTrainer; }
    bool CodaEnabled() const { return mProperties.mEnableCoda; }
    bool InPracticeMode() const { return mProperties.mInPracticeMode; }
    bool AllowOverdrivePhrases() const { return mProperties.mAllowOverdrivePhrases; }
    std::vector<Player *> &GetActivePlayers();
    BeatMaster *GetBeatMaster() const { return mMaster; }
    FillLogic GetFillLogic() const {
        return mDrumFillsMod ? kFillsRegular : kFillsDeployGemAndInvisible;
    }
    bool DrumFillsMod() const { return mDrumFillsMod; }
    bool IsPaused() const { return mIsPaused; }
    bool InRollback() const { return unkdc != -1.0f ? true : false; }

    DataNode OnJump(const DataArray *);
    DataNode OnLocalUserReadyToPlay(const DataArray *);
    DataNode OnSetShuttle(DataArray *);
    DataNode ForEachActivePlayer(const DataArray *);
    DataNode OnAdjustForVocalPhrases(DataArray *);
    DataNode OnMsg(const LocalUserLeftMsg &);
    DataNode OnMsg(const RemoteUserLeftMsg &);
    DataNode OnMsg(const RemoteLeaderLeftMsg &);
    DataNode OnMsg(const UIScreenChangeMsg &);
    DataNode OnMsg(const class NewOvershellLocalUserMsg &);
    DataNode OnMsg(const class GameEndedMsg &);

    void SetTimeOffset();
    bool HandleAudioLoad();
    void CheckSectionEnd(float);
    float PollShuttle();
    void RebuildData();

    Properties mProperties; // 0x24
    SongPos mSongPos; // 0x40
    SongDB *mSongDB; // 0x54
    SongInfo *mSongInfo; // 0x58
    BeatMaster *mMaster; // 0x5c
    std::vector<Player *> mAllActivePlayers; // 0x60
    // Retail places mRealtime at this+0x78 and unk6f at this+0x79 (proven by
    // Game::HandleAudioLoad reading lbz 0x78 / stb 0x79). With mAllActivePlayers
    // ending at 0x74, mRealtime must be the 5th bool of this run (index 4), so
    // retail has only ONE unknown bool (mPauseTime) between mOvershellWantsPause
    // and mRealtime — unk6b/unk6c (DC3/Wii-era extras) sit AFTER unk6f, absorbed
    // by the alignment pad before mTimeOffset@0x7c. Reordering (not deleting)
    // keeps their ctor init + accessors valid while fixing the +2 bool shift.
    bool mIsPaused; // 0x74
    bool mGameWantsPause; // 0x75
    bool mOvershellWantsPause; // 0x76
    bool mPauseTime; // 0x77
    bool mRealtime; // 0x78
    bool unk6f; // 0x79
    bool unk6b; // 0x7a (pad region)
    bool unk6c; // 0x7b (pad region) - screen saver?
    float mTimeOffset; // 0x7c
    // TU5: a new 4-byte member sits between mTimeOffset and mTime. Proven by
    // Game::Poll reading mTimeOffset at 0x80 (+4) but mTime (Timer) at 0x88
    // (+8) — the extra +4 lands exactly here. TODO: identify this member.
    int mUnkTU5_0x84; // (new in TU5, base ~0x80)
    // NOTE (360 offsets): Timer is 0x30 on X360 (8-byte-aligned unsigned long
    // long mCycles) vs 0x28 on Wii, so everything from mHasIntro on sits +0x8
    // vs the old Wii-era annotations here. Proven by the retail getter
    // fn_82659CD8 (= Game::HasIntro): `lbz r3, 0xb0(r3)`, called on
    // GamePanel::mGame in GamePanel::StartGame (target 0x826773F4).
    Timer mTime; // 0x80 (0x30 bytes on 360)
    bool mHasIntro; // 0xb0 (Wii: 0xa8)
    float mLastPollMs; // 0xb4 (Wii: 0xac)
    // Retail packs mMuckWithPitch with the mNeverAllowInput/unkb9 bool group
    // (Wii interleaved it with mMusicSpeed, wasting 4 bytes of alignment pad),
    // so mLoadState lands at 0xcc rather than 0xd0.
    float mMusicSpeed; // 0xb8
    bool mNeverAllowInput; // 0xbc
    bool mMuckWithPitch; // 0xbd
    bool unkb9; // 0xbe
    int mDemoMaxPctComplete; // 0xc0
    float mDemoMaxMs; // 0xc4
    bool unkc4; // 0xc8
    LoadState mLoadState; // 0xcc
    EndGameResult mResult; // 0xcc
    Band *mBand; // 0xd0
    Shuttle *mShuttle; // 0xd4
    float unkd8;
    float unkdc; // 0xdc - mRollbackEndMs?
    ATanInterpolator mInterpolator; // 0xe0
    float unk11c;
    bool unk120;
    bool mSkippedSong; // 0x121
    float unk124;
    float mResumeTime; // 0x128
    bool mInvalidScore; // 0x12c
    float unk130;
    float unk134;
    bool unk138;
    bool mDrumFillsMod; // 0x139
    int unk13c;
    float unk140;
    TrackerManager *mTrackerManager; // 0x144
    bool unk148;
    float mDisablePauseMs; // 0x14c
    bool unk150;
    std::vector<BandUser *> unk154;
};

DECLARE_MESSAGE(GameEndedMsg, "game_ended");
GameEndedMsg(int i, float f) : Message(Type(), i, f) {}
EndGameResult GetResult() const { return (EndGameResult)mData->Int(2); }
END_MESSAGE

void GameInit();
void GameTerminate();

extern Game * /*you just lost*/ TheGame;
