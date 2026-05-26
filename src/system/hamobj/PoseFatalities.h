#pragma once
#include "FreestyleMoveRecorder.h"
#include "char/CharClip.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "hamobj/HamLabel.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "utl/MemMgr.h"

#define MAX_NUM_PLAYERS 2
#define NUM_FATALITIES 8

class PoseFatalities : public Hmx::Object {
public:
    // Hmx::Object
    PoseFatalities();
    virtual ~PoseFatalities() {}
    OBJ_CLASSNAME(PoseFatalities);
    OBJ_SET_TYPE(PoseFatalities);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    NEW_OBJ(PoseFatalities)

    void DrawDebug();
    void Enter();
    bool InFatality(int) const;
    void ActivateFatal(int);
    bool FatalActive() const;
    void SetJump(int, int);
    bool GotFullCombo(int) const;
    void Poll();

private:
    Symbol GetFatalityFace();
    bool InStrikeAPose();
    void SetCombo(int, int);
    void SetFatalitiesStart(int);
    void Reset();
    void PlayVO(Symbol);
    String GetCelebrationClip(int);
    void EndFatal(int);
    float BeatsLeftToMatch(int);
    void BeginFatal(int);
    void AddFatal(int);
    bool CheckMatchingPose(int);
    void LoadFatalityClips();
    void PollVO();
    void OnBeat(int);
    void UpdateClipDriver(int);
    void UpdateMatchingPose(int);
    void OnFatalResult(int, bool);

    bool mInFatality[2]; // 0x2c
    int mFatalStartBeats[2]; // 0x30 - per player
    int mFatalEndBeat; // 0x38
    int mComboStartBeat[2]; // 0x3c - beat-relative combo start per player
    int mFatalityPoseIndex[2]; // 0x44 - 1-8 random pose index
    Skeleton mPlayerSkeletons[2]; // 0x4c
    float mFatalityProgress[2]; // 0x15f4 - accumulates completion progress per player
    float mHoldDuration; // 0x15fc - default 0.5f, loaded from OSC /holdduration
    FreestyleMoveRecorder mRecorder; // 0x1600
    float unk1710[2];
    float unk1718[2];
    bool mMatchingActive[2]; // 0x1720 - tracks active matching state per player
    std::list<CharClip *> mAllFatalityClips; // 0x1724
    int mFatalityBeatLeadIn; // 0x172c
    HamLabel *mPoseComboLabels[kNumSkeletonSides]; // 0x1730
    int mCurrentCombo[2]; // 0x1738
    bool mGotFullCombo[2]; // 0x1740
    ObjectDir *mHudPanel; // 0x1744
    bool mValidPose; // 0x1748 - set to InStrikeAPose() result
    int mJumpStart; // 0x174c
    int mJumpEnd; // 0x1750
    int mCurrentBeat; // 0x1754 - current beat/frame counter for beat logic
    RndAnimatable *mPoseBeatAnims[kNumSkeletonSides]; // 0x1758
    float mAnimTimer; // 0x1760 - countdown from 4, triggers at 0
    float mDisplayCooldown; // 0x1764 - countdown from 5, controls display state
    int mFeedbackFlags; // 0x1768 - bitmask with bits 0,1,2,4 set for display states
    float mDisplayProgress; // 0x176c - accumulator clamped 0.0-1.0, incremented by DeltaSeconds()
};
