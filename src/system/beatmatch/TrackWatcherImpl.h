#pragma once
#include "utl/HxGuid.h"
#include "beatmatch/BeatMatchControllerSink.h"
#include "beatmatch/TrackWatcherParent.h"
#include <vector>

// forward decs
class SongData;
class GameGemList;
class DataArray;
class BeatMatchSink;

struct GemInProgress {
    GemInProgress() : mInUse(0), mGemID(-1), unk8(0) {}
    void Reset() {
        mInUse = false;
        mGemID = -1;
        unk8 = 0;
    }
    // actually a byte, word, and float
    bool mInUse;
    int mGemID;
    float unk8; // the endMs of this gem? i've seen ms + durationms assigned to it
};

class TrackWatcherState {
public:
    int mLastGemHit;
    bool mAutoplayCoda;
    bool mIsCurrentTrack;
    std::vector<GemInProgress> mGemsInProgress;
    float mSyncOffset;
    int mTrack;
    bool mEnabled;
    int mLastGemPassed;
    int mLastGemSeen;
    float mLastMiss;
    bool mCheating;
    int mCheatError;
    int mNextCheatError;
    int mNextGemToAutoplay;
    bool mPitchBendReady;
    int mPitchBendRange;
    int mPitchBendMsToFull;
    float mPitchBendMsHit;
    float mBiggestWhammy;
    int mCymbalAutoplayMs;
    bool mSucceeding;
};

class TrackWatcherImpl {
public:
    TrackWatcherImpl(
        int,
        const UserGuid &,
        int,
        SongData *,
        GameGemList *,
        TrackWatcherParent *,
        DataArray *,
        int
    );
    virtual ~TrackWatcherImpl();
    virtual void Init();
    virtual void HandleDifficultyChange();
    virtual void SetIsCurrentTrack(bool);
    virtual void AddSink(BeatMatchSink *);
    virtual void Jump(float ms);
    virtual void SetAutoplayError(int);
    virtual void Poll(float ms);
    virtual bool Swing(int slot, bool guitar, bool provisional, GemHitFlags flags) = 0;
    virtual void NonStrumSwing(int slot, bool button_down, bool solo) = 0;
    virtual void FretButtonDown(int slot) = 0;
    // ★ Slot order proven on RETAIL BYTES (lane SETDIFF, 2026-08-31), NOT from
    // an oracle: retail `TrackWatcher::FretButtonUp` is the forwarder at
    // 0x8279d768, which dispatches through `lwz r11, 0x2c(r11)` == slot 11.
    // It is identified without any map name by `BeatMatcher::FretButtonUp`
    // (anchored by its own "(%2d%10.1f UP\t%d)\n" format string) doing
    // `bl 0x8279d768`.  Corroborated by Joypad/Keyboard, whose retail slot 12
    // is the universal empty stub 0x826c3888 -- i.e. the inherited
    // RGFretButtonDown{} -- while their FretButtonUp bodies are non-empty.
    // ⚠ Do NOT "fix" this back to RGFretButtonDown-first: that ordering came
    // from BeatMatchControllerSink, an UNRELATED class TrackWatcherImpl does
    // not derive from.
    virtual void FretButtonUp(int slot) = 0;
    virtual void RGFretButtonDown(int) {}
    virtual void OutOfRangeSwing() {}
    virtual void SetGemsPlayedUntil(int end_gem);
    virtual void Enable(bool);
    virtual bool IsCheating() const;
    virtual void SetCheating(bool);
    virtual void Restart();
    virtual void SetSyncOffset(float);
    virtual void
    OnHit(float ms, int slot, int gemID, unsigned int slots, GemHitFlags flags);
    virtual void
    OnMiss(float ms, int slot, int gemID, unsigned int slots, GemHitFlags flags);
    virtual void OnPass(float ms, int gemID);
    virtual void FakeHitGem(float ms, int gemID, GemHitFlags flags);
    virtual void RegisterFill(int) {}
    virtual void ResetFill() {}
    virtual void FillSwing(int tick, int slot, int gemID, bool at_end) {}
    virtual void CodaSwing(int tick, int slot);
    virtual void FillStop() {}
    virtual bool IsSwingInRoll(int gemID, unsigned int);
    virtual bool AreSlotsInRoll(unsigned int slots, int tick) const;
    virtual bool GetNextRoll(int, unsigned int &roll, int &endTick) const;
    virtual void CheckForTrills(float ms, int, unsigned int slots);
    virtual void PollHook(float ms);
    virtual void JumpHook(float ms);
    virtual float HitGemHook(float ms, int gemID, GemHitFlags flags) { return 0.0f; }
    virtual bool ShouldAutoplayGem(float ms, int gemID);
    virtual bool GemCanBePassed(int gemID) { return true; }
    virtual int NextGemAfter(int gemID, bool timeout);
    virtual float Slop(int gemID) { return mSlop - mSyncOffset; }
    virtual int ClosestUnplayedGem(float ms, int slot);
    virtual int SustainedGemToKill(int slot);
    virtual bool InTrill(int tick) const;

    void EndAllSustainedNotes();
    void LoadState(TrackWatcherState &);
    void SaveState(TrackWatcherState &);
    void RecalcGemList();
    void SetAutoplayCoda(bool);
    float CycleAutoplayAccuracy();
    void SetAutoplayAccuracy(float);
    void E3CheatIncSlop();
    void E3CheatDecSlop();
    void KillSustainForSlot(int);
    void KillSustain(int);
    void NoteSwing(unsigned int, int);
    bool InSlopWindow(float, float) const;
    bool Playable(int);
    bool IsTrillActive() const;
    void SetAllGemsUnplayed();
    void EndSustainedNote(GemInProgress &);
    bool HasAnyGemInProgress() const;
    GemInProgress *GetGemInProgressWithID(int);
    GemInProgress *GetGemInProgressWithSlot(int);
    GemInProgress *GetUnusedGemInProgress(float);

    void CheckForSustainedNoteTimeout(float);
    void CheckForRolls(float, int);
    void CheckForTrillTimeout(float);
    void CheckForAutoplay(float);
    void CheckForPasses(float);
    void CheckForGemsSeen(float);
    void CheckForPitchBend(float);
    void CheckForCodaLanes(int);
    int GetOtherTrillSlot(int, const std::pair<int, int> &) const;
    void SendHit(float, int, unsigned int, GemHitFlags);
    void SendIgnore(float, int);
    void SendSeen(float, int);
    void SendWhammy(float);
    void HitGem(float, int, unsigned int, GemHitFlags);
    void SendSwingAtHopo(float, int);
    void SendHopo(float, int);
    void SendReleaseGem(float, int, float);
    void MaybeAutoplayFutureCymbal(int);
    bool IsFillCompletion(float, int, int &);
    void SendMiss(float, int, int, int, GemHitFlags);
    void SendPass(float, int);
    void SendImplicit(float, int);
    void SendSpuriousMiss(float, int, int);

    int Track() const { return mTrack; }
    int GetFillLogic() const { return mParent->GetFillLogic(); }

    UserGuid mUserGuid; // 0x4
    bool mIsLocalUser; // 0x14
    int mPlayerSlot; // 0x18
    GameGemList *mGemList; // 0x1c
    TrackWatcherParent *mParent; // 0x20
    float mSlop; // 0x24
    int mLastGemHit; // 0x28
    bool mIsCurrentTrack; // 0x2c
    std::vector<GemInProgress> mGemsInProgress; // 0x30
    float mSyncOffset; // 0x3c
    bool mSucceeding; // 0x40
    bool mEnabled; // 0x41
    std::vector<BeatMatchSink *> mSinks; // 0x44
    SongData *mSongData; // 0x50
    bool mTrillSucceeding; // 0x54
    int mTrillNextSlot; // 0x58
    float mTrillLastFretMs; // 0x5c
    float mTrillIntervalMs; // 0x60
    DataArray *mRollIntervalsConfig; // 0x64
    int mTrack; // 0x68
    bool mButtonMashingMode; // 0x6c
    int mLastGemPassed; // 0x70
    int mLastGemSeen; // 0x74
    float mLastMiss; // 0x78
    bool mCheating; // 0x7c
    bool mAutoplayCoda; // 0x7d
    int mCheatError; // 0x80
    int mNextCheatError; // 0x84
    float mLastCheatCodaSwing; // 0x88
    int mNextGemToAutoplay; // 0x8c
    float mAutoplayAccuracy; // 0x90
    int mCymbalAutoplayMs; // 0x94
    GemHitFlags mLastPlayedHitFlags; // 0x98
    bool mPitchBendReady; // 0x9c
    int mPitchBendRange; // 0xa0
    int mPitchBendMsToFull; // 0xa4
    float mPitchBendMsHit; // 0xa8
    float mBiggestWhammy; // 0xac
    std::vector<float> mRollSlotsLastSwingMs; // 0xb0
    int mRollActiveSlots; // 0xbc
    float mRollIntervalMs; // 0xc0
    int mRollEndTick; // 0xc4
    DataArray *mTrillIntervalsConfig; // 0xc8
};
