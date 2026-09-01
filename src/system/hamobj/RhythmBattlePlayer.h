#pragma once
#include "flow/Flow.h"
#include "gesture/DepthBuffer3D.h"
#include "hamobj/HamLabel.h"
#include "hamobj/HamMove.h"
#include "hamobj/HamPhraseMeter.h"
#include "hamobj/TransConstraint.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Dir.h"
#include "rndobj/Part.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"

class RhythmBattle;

enum RhythmBattleJackState {
};

/** "The state of a player in Rhythm Battle." */
class RhythmBattlePlayer : public RndPollable {
    friend class RhythmBattle;

public:
    // Hmx::Object
    virtual ~RhythmBattlePlayer();
    OBJ_CLASSNAME(RhythmBattlePlayer)
    OBJ_SET_TYPE(RhythmBattlePlayer)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndPollable
    virtual void Poll();
    virtual void Enter();

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(RhythmBattlePlayer)

    int InTheZone() const;
    void SetInTheZone(int, bool, bool);
    float InAnimBeatLength() const;
    void SetWindow(float, float);
    bool UpdateState();
    void HackPlayerQuit();
    int SwagJacked(Hmx::Object *, RhythmBattleJackState);
    void SwagJackedBonus(Hmx::Object *, RhythmBattleJackState, int);
    void AnimateIn();
    void SwapObjs(RhythmBattlePlayer *);
    void OnReset(class RhythmBattle *);
    void UpdateScore(Hmx::Object *);
    void UpdateComboProgress();
    void UpdateAnimations(Hmx::Object *);
    void ResetCombo();
    void SetActive(bool);
    void AnimateOut();
    bool ShouldAutoPass() const { return mAutoPass && mFramesSinceLastTrigger > 12; }
    int GetScore() const { return mScore; }
    int GetZoneLevel() const { return mZoneLevel; }
    int GetPrevInTheZone() const { return mPrevInTheZone; }
    float GetComboMeter() const { return mComboMeter; }
    void SetSuppressRhythm(bool b) { mSuppressRhythm = b; }
    void SetAutoPass(bool b) { mAutoPass = b; }
    int ZoneValue() const { return mInTheZone; }

protected:
    RhythmBattlePlayer();

private:
    void AnimateBoxyState(int, bool, bool);
    void UpdateScore(int);

    /** "instruction display" */
    ObjPtr<RndAnimatable> mComboPosAnim; // 0x8
    /** "instruction display" */
    ObjPtr<RndAnimatable> mComboColorAnim; // 0x14
    /** "instruction display" */
    ObjPtr<RndAnimatable> mResetComboAnim; // 0x20
    /** "instruction display" */
    ObjPtr<RndAnimatable> m2xMultAnim; // 0x2c
    /** "instruction display" */
    ObjPtr<RndAnimatable> m3xMultAnim; // 0x38
    /** "instruction display" */
    ObjPtr<RndAnimatable> m4xMultAnim; // 0x44
    ObjPtr<RndAnimatable> mRhythmBattleAnim; // 0x50
    ObjPtr<RndAnimatable> mBattleMeterAnim; // 0x5c
    ObjPtr<RndAnimatable> mBattleMeterStaleAnim; // 0x68
    ObjPtr<RndAnimatable> mBattleMeterInAnim; // 0x74
    ObjPtr<RndAnimatable> mShowScoreAnim; // 0x80
    ObjPtr<RndAnimatable> mBattleMeterOutAnim; // 0x8c
    /** "override the world boxydir" */
    ObjPtr<RndDir> mBoxyDir; // 0x98
    ObjPtr<HamLabel> mBattleLabel; // 0xa4
    /** "instruction display" */
    ObjPtr<HamLabel> mScoreLabel; // 0xb0
    ObjPtr<Flow> mInTheZoneFlow; // 0xbc
    ObjPtr<Flow> mOutTheZoneOkFlow; // 0xc8
    ObjPtr<Flow> mOutTheZoneBadFlow; // 0xd4
    ObjPtr<Flow> mSwagJackedFlow; // 0xe0
    ObjPtr<HamPhraseMeter> mPhraseMeter; // 0xec
    ObjPtr<TransConstraint> mTransConstraint; // 0xf8
    ObjPtr<RndTransformable> mBoxyWaistTrans; // 0x104
    ObjPtr<DepthBuffer3D> mBoxyman1; // 0x110
    ObjPtr<DepthBuffer3D> mBoxyman2; // 0x11c
    ObjPtr<ObjectDir> mTextFeedback; // 0x128
    ObjPtr<ObjectDir> mMoveFeedback; // 0x134
    ObjPtr<RndParticleSys> mStealPart; // 0x140
    ObjPtr<RndAnimatable> mStealAnim; // 0x14c
    /** "which player is this" */
    int mPlayer; // 0x158
    RhythmBattle *mRhythmBattle; // 0x15c
    bool mActive; // 0x160 - active?
    float mRhythmSuccessFraction; // 0x164 - 0.0-1.0 rhythm detection success from RhythmDetector
    float mFreshnessScore; // 0x168 - 0=stale, 1=fresh, from rd->Freshness()
    float mMaxRhythmInWindow; // 0x16c - peak rhythm fraction in current window
    float mFreshnessAccumulator; // 0x170 - cumulative freshness over time window
    float mMovePresenceAccumulator; // 0x174 - cumulative move presence counter
    float mWindowElapsedTime; // 0x178 - elapsed time in scoring window
    float mLastBeatTime; // 0x17c - previous beat time for deltaTime calculation
    int mZoneLevel; // 0x180
    int mPrevZoneLevel; // 0x184
    int mInTheZone; // 0x188
    int mPrevInTheZone; // 0x18c
    float mNormalizedRhythmScore; // 0x190 - 0.0-1.0 normalized rhythm score
    float mNormalizedFreshnessScore; // 0x194 - 0.0-1.0 normalized freshness average
    float mMoveConsistencyScore; // 0x198 - 0.0-1.0 move consistency metric
    Symbol mTrickSymbol; // 0x19c - trickpose/trickjump/none
    int mScore; // 0x1a0
    float mComboMeter; // 0x1a4
    bool mSwapped; // 0x1a8
    float mScoringWindowStart; // 0x1ac - start beat of scoring window
    float mScoringWindowEnd; // 0x1b0 - end beat of scoring window
    int mDebugScoreValue; // 0x1b4 - debug score value, -1=not logging
    Symbol mSwagJackedState; // 0x1b8
    int mGrooveCooldown; // 0x1bc
    float mPrevMaxFootY; // 0x1c0 - previous max foot Y for jump detection
    bool mSuppressRhythm; // 0x1c4
    bool mAutoPass; // 0x1c5
    int mFramesSinceLastTrigger; // 0x1c8 - incremented in UpdateScore
};
