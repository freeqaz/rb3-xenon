#pragma once
#include "gesture/ArchiveSkeleton.h"
#include "hamobj/FreestyleMoveRecorder.h"
#include "hamobj/HamLabel.h"
#include "hamobj/RhythmBattlePlayer.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <vector>

class RhythmBattlePlayer;

/** "Competition between two RhythmBattlePlayers." */
class RhythmBattle : public RndPollable {
public:
    // Hmx::Object
    virtual ~RhythmBattle();
    OBJ_CLASSNAME(RhythmBattle);
    OBJ_SET_TYPE(RhythmBattle);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();

    OBJ_MEM_OVERLOAD(0x23)
    NEW_OBJ(RhythmBattle)

    void End();
    void ResetCombo();
    void Begin();
    bool CanTrick(Symbol);
    bool InFullKTB() const { return mFullKTB; }
    bool IsPaused() const { return mPaused; }

protected:
    RhythmBattle();

private:
    bool GetGoofy() const;
    Symbol GetLeader() const;
    void OnPause();
    void OnUnpause();
    void CheckIsFinale();
    void PlayTanClip(int, bool);
    void PlayMindControlVO(Symbol);
    void UpdateMindControl();
    void UpdateFinaleVO(int &);
    void QueueFinaleVO(Symbol);
    void OnReset();
    void OnBeat();

    /** "instruction display" */
    ObjPtr<HamLabel> mCommandLabel; // 0x8
    ObjPtr<HamLabel> mIntroLine2Label; // 0x14
    /** "player 0 object" */
    ObjPtr<RhythmBattlePlayer> mPlayerOne; // 0x20
    /** "player 1 object" */
    ObjPtr<RhythmBattlePlayer> mPlayerTwo; // 0x2c
    ObjPtr<RndTransformable> mBoxyLeadHeadTrans; // 0x38
    ObjPtr<RndAnimatable> mIntroAnim; // 0x44
    ObjPtr<RndAnimatable> mBattleEndAnim; // 0x50
    ObjPtr<RndAnimatable> unk94; // 0x5c
    ObjPtr<RndAnimatable> mSwagJack1BarP2ToP1Anim; // 0x68
    ObjPtr<RndAnimatable> mSwagJack1BarP1ToP2Anim; // 0x74
    ObjPtr<RndAnimatable> mSwagJack2BarP2ToP1Anim; // 0x80
    ObjPtr<RndAnimatable> mSwagJack2BarP1ToP2Anim; // 0x8c
    bool mGoofy; // 0x98
    /** "is this keep the beat, or just groove tech experience" */
    bool mFullKTB; // 0x99
    bool mFinale; // 0x9a
    bool mActive; // 0x9b
    bool mIntroAnimStarted; // 0x9c
    bool mIsGrooveMode; // 0x9d
    bool mHalftimePlayed; // 0x9e
    bool mAlmostOverPlayed; // 0x9f
    bool mBattleStarted; // 0xa0
    bool mBattleFinished; // 0xa1
    bool mPaused; // 0xa2
    float mStartBeat; // 0xa4
    float mEndBeat; // 0xa8
    float mMindControlIntensity; // 0xac
    float mMindControlTimer; // 0xb0
    float mHalftimeBeat; // 0xb4
    float mAlmostOverBeat; // 0xb8
    int mMoveKeyCount; // 0xbc
    float unk120; // 0xc0
    int mSwagJackState; // 0xc4 - RhythmBattleJackState
    int mSwagJackCounter; // 0xc8
    Symbol mLeader; // 0xcc
    FreestyleMoveRecorder *mMoveRecorder; // 0xd0
    std::vector<ArchiveSkeleton> mSkeletonHistory; // 0xd4
    int mJackCooldown; // 0xe0
    int mLastBeatTracked; // 0xe4
    int mFinaleSequenceTimer; // 0xe8
    int mFinalePhaseIndex; // 0xec
    std::vector<Symbol> mFinaleVOQueue; // 0xf0
};

void SetJump(int, int);
void ClearJump();
