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
    ObjPtr<RndAnimatable> mComboColorAnim; // 0x1c
    /** "instruction display" */
    ObjPtr<RndAnimatable> mResetComboAnim; // 0x30
    /** "instruction display" */
    ObjPtr<RndAnimatable> m2xMultAnim; // 0x44
    /** "instruction display" */
    ObjPtr<RndAnimatable> m3xMultAnim; // 0x58
    /** "instruction display" */
    ObjPtr<RndAnimatable> m4xMultAnim; // 0x6c
    ObjPtr<RndAnimatable> mRhythmBattleAnim; // 0x80
    ObjPtr<RndAnimatable> mBattleMeterAnim; // 0x94
    ObjPtr<RndAnimatable> mBattleMeterStaleAnim; // 0xa8
    ObjPtr<RndAnimatable> mBattleMeterInAnim; // 0xbc
    ObjPtr<RndAnimatable> mShowScoreAnim; // 0xd0
    ObjPtr<RndAnimatable> mBattleMeterOutAnim; // 0xe4
    /** "override the world boxydir" */
    ObjPtr<RndDir> mBoxyDir; // 0xf8
    ObjPtr<HamLabel> mBattleLabel; // 0x10c
    /** "instruction display" */
    ObjPtr<HamLabel> mScoreLabel; // 0x120
    ObjPtr<Flow> mInTheZoneFlow; // 0x134
    ObjPtr<Flow> mOutTheZoneOkFlow; // 0x148
    ObjPtr<Flow> mOutTheZoneBadFlow; // 0x15c
    ObjPtr<Flow> mSwagJackedFlow; // 0x170
    ObjPtr<HamPhraseMeter> mPhraseMeter; // 0x184
    ObjPtr<TransConstraint> mTransConstraint; // 0x198
    ObjPtr<RndTransformable> mBoxyWaistTrans; // 0x1ac
    ObjPtr<DepthBuffer3D> mBoxyman1; // 0x1c0
    ObjPtr<DepthBuffer3D> mBoxyman2; // 0x1d4
    ObjPtr<ObjectDir> mTextFeedback; // 0x1e8
    ObjPtr<ObjectDir> mMoveFeedback; // 0x1fc
    ObjPtr<RndParticleSys> mStealPart; // 0x210
    ObjPtr<RndAnimatable> mStealAnim; // 0x224
    /** "which player is this" */
    int mPlayer; // 0x238
    RhythmBattle *mRhythmBattle; // 0x23c
    bool mActive; // 0x240 - active?
    float mRhythmSuccessFraction; // 0x244 - 0.0-1.0 rhythm detection success from RhythmDetector
    float mFreshnessScore; // 0x248 - 0=stale, 1=fresh, from rd->Freshness()
    float mMaxRhythmInWindow; // 0x24c - peak rhythm fraction in current window
    float mFreshnessAccumulator; // 0x250 - cumulative freshness over time window
    float mMovePresenceAccumulator; // 0x254 - cumulative move presence counter
    float mWindowElapsedTime; // 0x258 - elapsed time in scoring window
    float mLastBeatTime; // 0x25c - previous beat time for deltaTime calculation
    int mZoneLevel; // 0x260
    int mPrevZoneLevel; // 0x264
    int mInTheZone; // 0x268
    int mPrevInTheZone; // 0x26c
    float mNormalizedRhythmScore; // 0x270 - 0.0-1.0 normalized rhythm score
    float mNormalizedFreshnessScore; // 0x274 - 0.0-1.0 normalized freshness average
    float mMoveConsistencyScore; // 0x278 - 0.0-1.0 move consistency metric
    Symbol mTrickSymbol; // 0x27c - trickpose/trickjump/none
    int mScore; // 0x280
    float mComboMeter; // 0x284
    bool mSwapped; // 0x288
    float mScoringWindowStart; // 0x28c - start beat of scoring window
    float mScoringWindowEnd; // 0x290 - end beat of scoring window
    int mDebugScoreValue; // 0x294 - debug score value, -1=not logging
    Symbol mSwagJackedState; // 0x298
    int mGrooveCooldown; // 0x29c
    float mPrevMaxFootY; // 0x2a0 - previous max foot Y for jump detection
    bool mSuppressRhythm; // 0x2a4
    bool mAutoPass; // 0x2a5
    int mFramesSinceLastTrigger; // 0x2a8 - incremented in UpdateScore
};
