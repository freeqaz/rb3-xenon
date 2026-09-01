#pragma once
#include "bandtrack/GemManager.h"
#include "bandtrack/GemTrack.h"
#include "beatmatch/BeatMatchController.h"
#include "beatmatch/BeatMatchSink.h"
#include "beatmatch/BeatMatcher.h"
#include "game/GuitarFx.h"
#include "game/HeldNote.h"
#include "game/KeysFx.h"
#include "game/Player.h"
#include "game/StatCollector.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "rndobj/Overlay.h"
#include "synth/FxSendPitchShift.h"

class BandPerformer;

class DeltaTracker {
public:
    DeltaTracker() : mCur(0) {}

    void PrintDeltas() {
        if (mCur > 0) {
            std::vector<float> fvec;
            int num = 999;
            if (mCur <= 999)
                num = mCur;
            fvec.resize(num);
            memcpy(fvec.begin(), mDeltas, num * sizeof(float));
            std::sort(fvec.begin(), fvec.begin() + num);
            for (int i = 0; i < num; i++) {
                MILO_LOG("%d %.3f\n", i, fvec[i]);
            }
            MILO_LOG("median is %.3f\n", fvec[num / 2]);
        } else
            MILO_LOG("No samples to be had\n");
        mCur = 0;
    }

    float mDeltas[1000];
    int mCur;
};

class GemStatus {
public:
    GemStatus() : mHits(0), mMisses(0) {}

    // is hit: 0x1
    // is cymbal: 0x20
    // is solo: 0x80

    // 0x40? 0xe?
    // 0x4 - processed?

    // from GDRB:
    // missed: 0x2
    // ignored: 0x8

    bool GetHit(int idx) {
        // Retail materializes GetHit's bool result and tests it at the call
        // site (clrlwi/extract then clrlwi.,24) rather than fusing the bit-test
        // into the branch. Declaring mask as bool (not int) reproduces that
        // value-form, closing AllCodaGemsHit + OnGetPercentHitGemsPractice.
        // (Sibling accessors like GetIgnored keep `int` — their negated call
        // sites want the fused test-directly form.)
        bool mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 1;
        }
        return mask;
    }

    bool Get0x4(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 4;
        }
        return mask;
    }

    bool Get0x40(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x40;
        }
        return mask;
    }

    bool GetSolo(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x80;
        }
        return mask;
    }

    bool GetEncountered(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0xF;
        }
        return mask;
    }

    bool Get0x2(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x2;
        }
        return mask;
    }

    bool Get0xD(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0xD;
        }
        return mask;
    }

    bool GetHopoed(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x10;
        }
        return mask;
    }

    bool GetIgnored(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x8;
        }
        return mask;
    }

    bool HasDealtWithGem(int idx) {
        int mask;
        if (idx == -1)
            mask = 0;
        else {
            mask = mGems[idx] & 0x9;
        }
        return mask;
    }

    void SetHopoed(int idx) {
        if (idx != -1) {
            mGems[idx] |= 0x10;
        }
    }

    void SetIgnored(int idx) {
        if (idx != -1) {
            mGems[idx] |= 8;
        }
    }

    void SetHit(int idx) {
        if (idx != -1) {
            mGems[idx] |= 1;
        }
    }

    void Set0x2(int idx) {
        if (idx != -1) {
            mGems[idx] |= 2;
        }
    }

    void Set0x4(int idx) {
        if (idx != -1) {
            mGems[idx] |= 4;
        }
    }

    void Set0x40(int idx) {
        if (idx != -1) {
            mGems[idx] |= 0x40;
        }
    }

    // Retail calls these as inlined accessors rather than open-coding
    // `if (id != -1) mGemStatus->mGems[id] |= X` at the site: the `this` load
    // (`lwz r11, 0x8(r30)`) is evaluated BEFORE the `idx != -1` guard, and each
    // call re-loads it, which is what stops MSVC merging two adjacent guards.
    // See GemPlayer::Hit @0x826C7B?? (|=0x80) and the |=0x20 site after
    // UpdateSectionStats().
    void SetSolo(int idx) {
        if (idx != -1) {
            mGems[idx] |= 0x80;
        }
    }

    void Set0x20(int idx) {
        if (idx != -1) {
            mGems[idx] |= 0x20;
        }
    }

    void Clear0xBF(int idx) {
        if (idx != -1) {
            mGems[idx] &= 0xBF;
        }
    }

    void Clear() {
        for (int i = 0; i < mGems.size(); i++) {
            mGems[i] = 0;
        }
        mHits = 0;
        mMisses = 0;
    }

    float GetNotesHitFraction(bool *bptr) const {
        float total = mHits + mMisses;
        if (bptr)
            *bptr = total > 0;
        return total == 0 ? 0 : mHits / total;
    }

    void Resize(int num) { mGems.resize(num); }
    int NumHits() const { return mHits; }
    int NumMisses() const { return mMisses; }
    int GetSize() const { return mGems.size(); }

    std::vector<unsigned char> mGems; // 0x0
    int mHits; // 0x8
    int mMisses; // 0xc
};

class GemPlayer : public Player, public BeatMatchSink {
public:
    class UpcomingFretRelease {
    public:
        int unk0; // slot?
        float unk4; // ms?
    };
    GemPlayer(BandUser *, BeatMaster *, Band *, int, BandPerformer *);
    virtual ~GemPlayer();
    virtual DataNode Handle(DataArray *, bool);
    virtual int CodaScore() const { return mCodaPoints; }
    virtual Symbol GetStarRating() const;
    virtual int GetNumStars() const;
    virtual bool PastFinalNote() const;
    virtual void Poll(float, const SongPos &);
    virtual void Restart(bool);
    virtual float GetNotesHitFraction(bool *) const;
    virtual void FinalizeStats();

    virtual void DynamicAddBeatmatch();
    virtual void PostDynamicAdd();
    virtual void Leave();
    virtual void SetTrack(int);
    virtual void PostLoad(bool);
    virtual bool IsReady() const;
    virtual void Start();
    virtual void PollTrack();
    virtual void PollAudio();
    virtual void SetPaused(bool);
    virtual void SetRealtime(bool);
    virtual void SetMusicSpeed(float);
    virtual void Jump(float, bool);
    virtual void SetAutoplay(bool);
    virtual bool IsAutoplay() const;
    virtual void SetAutoOn(bool);
    virtual void HookupTrack();
    virtual void UnHookTrack();
    virtual void EnableFills(float, bool);
    virtual void DisableFills();
    virtual void EnableDrumFills(bool);
    virtual bool FillsEnabled(int);
    virtual bool AreFillsForced() const { return mForceFill; }
    virtual void EnterCoda();
    virtual void ResetCodaPoints();
    virtual void AddCodaPoints();
    virtual int GetCodaPoints() { return mCodaPoints; }
    virtual bool InFill() const { return mFill; }

    virtual void SetFillLogic(FillLogic);
    virtual bool DoneWithSong() const;
    virtual void Rollback(float, float);
    virtual void EnableController();
    virtual void DisableController();
    virtual void ConfigureBehavior();
    virtual int GetBaseMaxPoints() const;
    virtual int GetBaseMaxStreakPoints() const;
    virtual int GetBaseBonusPoints() const;
    virtual void SetSyncOffset(float);
    virtual bool GetCodaFreestyleExtents(Extent &) const;
    virtual void EnterAnnoyingMode() { mAnnoyingMode = true; }
    virtual void ChangeDifficulty(Difficulty);
    virtual void HandleNewSection(const PracticeSection &, int, int);
    virtual void LocalSetEnabledState(EnabledState, int, BandUser *, bool);
    virtual void EnableSwings(bool);
    virtual void IgnoreUntilRollback(float);
    virtual void UpdateLeftyFlip();
    virtual void ResetController(bool);

    virtual void SeeGem(int, float, int);
    virtual void Swing(int, int, float, bool, bool);
    virtual void Hit(int, float, int, unsigned int, GemHitFlags);
    virtual void Miss(int, int, float, int, int, GemHitFlags);
    virtual void SpuriousMiss(int, int, float, int);
    virtual void Pass(int, float, int, bool);
    virtual void Ignore(int, float, int, const UserGuid &);
    virtual void ImplicitGem(int, float, int, const UserGuid &);

    virtual void SetTrack(const UserGuid &, int) {}
    virtual void FretButtonDown(int, float);
    virtual void FretButtonUp(int, float);
    virtual void MercurySwitch(bool, float);
    virtual void FilteredWhammyBar(float);
    virtual void SwingAtHopo(int, float, int);
    virtual void Hopo(int, float, int);
    virtual void ReleaseGem(int, float, int, float);
    virtual void SetCurrentPhrase(int, const PhraseInfo &) {}
    virtual void NoCurrentPhrase(int) {}
    virtual void FillSwing(int, int, int, int, bool);
    virtual void FillReset();
    virtual void FillComplete(int, int);
    virtual void NoteOn(int);
    virtual void NoteOff(int);
    virtual void PlayNote(int);
    virtual void OutOfRangeSwing();

    virtual int GetNumRolls() const;
    virtual void GetRollInfo(int, int &, int &) const;
    virtual int GetNumTrills() const;
    virtual void GetTrillInfo(int, int &, int &) const;
    virtual void FillInProgress(int, int);
    virtual int GetTrackSlot(int) const;
    virtual void SwingHook(int, int, float, bool, bool) {}
    virtual void HitHook(int, float, int, unsigned int, GemHitFlags) {}
    virtual void MissHook(int, int, float, int, int) {}
    virtual void PassHook(int, float, int, bool) {}
    virtual void SeeGemHook(int, float, int) {}

    int GetRGFret(int) const;
    int GetMaxSlots() const;
    void PrintAddHead(int, int, int, int, int);
    void PrintMsg(const char *);
    void SetGuitarFx();
    void SetDrumKitBank(ObjectDir *);
    void UpdateGameCymbalLanes();
    void IgnoreGemsUntil(int);
    void DropIn(int);
    void InputReceived();
    void FinaleSwing(int);
    void LocalFinaleSwing(int);
    bool CanFlail(float);
    bool HandleSpecialMissScenarios(int, float);
    void PlayDrum(int, int, float, int);
    void PlayMissSound(int);
    void ShowFillHit(int);
    void LocalShowFillHit(int, int, bool);
    void OnRemoteFillHit(int, int, bool);
    void IgnoreGem(int);
    void ForceFill(bool);
    HeldNote *FindHeldNoteFromSlot(int);
    void OnSetWhammyOverdriveEnabled(bool);
    void OnSetMercurySwitchEnabled(bool);
    void OnResetCodaPoints();
    int OnGetPercentHit() const;
    int OnGetGemResult(int);
    bool OnGetGemIsSustained(int);
    void OnGameOver();
    void OnDisableController();
    void OnRemoteHit(int, int, float);
    void OnRemotePenalize(int, int, float);
    void OnRemoteCodaHit(int, int);
    void OnRemoteWhammy(float);
    void OnRemoteFill(bool);
    void OnRemoteHitLastCodaGem(int);
    void OnRemoteBlowCoda();
    void LocalSoloStart();
    void LocalSoloHit(int);
    void LocalSoloEnd(int, int);
    void LocalSetGuitarFx(int);
    void OnStartOverdrive();
    void OnStopOverdrive();
    void OnRefreshTrackButtons();
    bool InFillNow();
    bool InTrill(int) const;
    bool InRGTrill(int) const;
    bool InRoll(int) const;
    bool InRGRoll(int) const;
    void PrintHopoStats();
    void SetFilling(bool, int);
    void HandleCommonPhraseNote(int, int);
    void JumpReset(float);
    float OnGetPercentHitGemsPractice(int, float, float) const;
    void FinishAllHeldNotes(float);
    void SetReverb(bool);
    FxSendPitchShift *GetPitchShift();
    void GetPlayerState(PlayerState &) const;
    void DisableFillsCompletely();
    void ResetGemStates(float);
    void SetPitchShiftRatio(float);
    float CycleAutoplayAccuracy();
    void SetAutoplayAccuracy(float);
    bool HasDealtWithGem(int);
    void RecordTrillStats();
    void UpdateSectionStats();
    void GetGemsHit(int, int, int &, int &);
    void HandleFirstGemAfterRollback(int);
    bool HasAnyActiveHeldNotes() const;
    bool InIgnorableFill(int);
    bool IgnoreGemsAt(int);
    void UpdateCrowdMeter(float, int);
    void SendPenalize();
    void Penalize(float, int, float);
    void HandleSoloGem(int, bool, float, bool);
    bool ShouldPenalizeGem(int) const;
    void CheckHeldNotes(float);
    void FinishHeldNote(float, HeldNote &);
    void PrintFinishHeldNote();
    HeldNote &GetUnusedHeldNote();
    HeldNote *FindHeldNoteFromGemID(int);
    HeldNote *FindFirstActiveHeldNote();
    void AddHeadPoints(float, int, int, GemHitFlags);
    bool ToggleNoFills();
    int GetSoloData(int, float &, float &, int &);
    float GetCommonPhraseFraction(int);
    bool IsCodaMiss(float);
    void CheckSolo(float);
    void SoloEnd();
    void SendWhammyBar(float);
    bool AllCodaGemsHit() const;
    void CodaHit(float, int);
    void SetAnnoyingMode(bool);
    void SetRemoteAnnoyingMode(bool);
    void CheckFretReleases(float);
    void RemoveFretReleasesInSlot(int);
    GemStatus *GetGemStatus() const { return mGemStatus; }
    BeatMatchController *GetController() const { return mController; }
    Symbol GetControllerType() const { return mControllerType; }

    const SongPos &GetSongPos() const { return mMatcher->mSongPos; }

    Performer *mBandPerformer; // 0x304
    GemStatus *mGemStatus; // 0x308
    DataArray *mDrumSlotWeights; // 0x30c
    Symbol mDrumSlotWeightMapping; // 0x310
    DataArray *mDrumCymbalPointBonus; // 0x314
    unsigned int mGameCymbalLanes; // 0x318
    std::vector<HeldNote> mHeldNotes; // 0x31c
    bool mFill; // 0x328
    bool mForceFill; // 0x329
    int mLastFillHitTick; // 0x32c
    int unk2f4;
    int mNumFillSwings; // 0x334
    int mNumCrashFillReadyHits; // 0x338
    bool mUseFills; // 0x33c
    bool unk301;
    std::pair<int, int> mTrillSlots; // 0x340
    int unk30c;
    int unk310;
    bool unk314;
    bool unk315;
    bool unk316;
    int mCodaPoints; // 0x354
    float mLastCodaSwing[6]; // 0x358
    float mCodaPointRate; // 0x370
    float mCodaMashPeriod; // 0x374
    bool mMercurySwitchEnabled; // 0x378
    bool unk33d;
    bool mWhammyOverdriveEnabled; // 0x37a
    RndOverlay *mOverlay; // 0x37c
    RndOverlay *mGuitarOverlay; // 0x380
    bool unk348;
    float mWhammySpeedThreshold; // 0x388
    float mWhammySpeedTimeout; // 0x38c
    float unk354;
    float unk358;
    float unk35c;
    float mLastTimeWhammyVelWasHigh; // 0x39c
    float unk364;
    GemTrack *mTrack; // 0x3a4
    BeatMatchController *mController; // 0x3a8
    Symbol mControllerType; // 0x3ac
    BeatMatcher *mMatcher; // 0x3b0
    float mSyncOffset; // 0x3b4
    GuitarFx *mGuitarFx; // 0x3b8
    KeysFx *mKeysFx; // 0x3bc
    int mFxPos; // 0x3c0
    bool unk388;
    FxSendPitchShift *mPitchShift; // 0x3c8
    float unk390;
    float unk394;
    int unk398;
    // NOTE: retail Xbox drops the Wii guitar-FX-core block that lived here
    // (unk39c FXCore index, unk3a0 mFxPos cache, unk3a4/unk3a8 SetFX/SetReverb
    // gate bools). Xbox routes guitar FX through mPitchShift (0x38c) instead, so
    // these 16 bytes are absent in the retail layout — verified against the
    // retail disassembly (member-delta R1). Everything below is therefore 0x10
    // lower than the old Wii-derived comments.
    float unk3ac; // 0x3d8
    float mAutoMissSoundTimeoutMs; // 0x3dc
    float mFirstGemMs; // 0x3e0
    bool mAnnoyingMode; // 0x3e4
    bool unk3b9; // 0x3e5
    int unk3bc; // 0x3e8
    int unk3c0; // 0x3ec
    int mAutoMissSoundTimeoutGems; // 0x3f0
    int mAutoMissSoundTimeoutGemsRemote; // 0x3f4
    StatCollector mStatCollector; // 0x3f8
    bool unk3d8;
    int unk3dc;
    bool unk3e0;
    bool unk3e1;
    int mSectionStartHitCount; // 0x410
    int mSectionStartMissCount; // 0x414
    float mSectionStartScore; // 0x418
    int mSustainsReleased; // 0x41c
    int mSustainHeld; // 0x420
    int mSustainsReleasedBeforePopup; // 0x424
    std::vector<UpcomingFretRelease> mUpcomingFretReleases; // 0x428
    int unk404;
    bool unk408;
};