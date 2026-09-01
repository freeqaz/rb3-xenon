#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "math/Mtx.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Pins fingers to world positions" */
class CharIKFingers : public RndHighlightable,
                      public CharWeightable,
                      public CharPollable {
public:
    enum FingerNum {
        kFingerThumb,
        kFingerIndex,
        kFingerMiddle,
        kFingerRing,
        kFingerPinky,
        kNumFingers
    };
    struct FingerDesc {
        FingerDesc()
            : mIsEngaged(0), mTargetWorldPos(0, 0, 0), mRefWorldPos(0, 0, 0), mFinger01(nullptr),
              mFinger02(nullptr), mFinger03(nullptr), mFingertip(nullptr), mBlendFrames(0),
              mBlendOutFrames(0), mNeedsUpdate(1) {}
        bool mIsEngaged; // 0x0
        float mBoneTotalLength; // 0x4
        Vector3 mTargetWorldPos; // 0x8
        Vector3 mRefWorldPos; // 0x18
        ObjPtr<RndTransformable> mFinger01; // 0x28
        ObjPtr<RndTransformable> mFinger02; // 0x34
        ObjPtr<RndTransformable> mFinger03; // 0x40
        ObjPtr<RndTransformable> mFingertip; // 0x4c
        float mDestFinger02Angle; // 0x58
        float mDestFinger03Angle; // 0x5c
        float mCurFinger02Angle; // 0x60
        float mCurFinger03Angle; // 0x64
        int mBlendFrames; // 0x68
        int mBlendOutFrames; // 0x6c
        bool mNeedsUpdate; // 0x70
        Vector3 mDestOrientVec; // 0x74
        Vector3 mCurOrientVec; // 0x84
        bool mNeedsIKSolve; // 0x94
    };
    // Hmx::Object
    virtual ~CharIKFingers();
    OBJ_CLASSNAME(CharIKFingers);
    OBJ_SET_TYPE(CharIKFingers);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetName(const char *, class ObjectDir *);
    // RndHighlightable
    virtual void Highlight();
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x1F)
    NEW_OBJ(CharIKFingers)

    void MeasureLengths();

protected:
    CharIKFingers();
    void CalculateHandDest(int, int);
    void CalculateFingerDest(FingerNum);
    void MoveFinger(FingerNum);
    void FixSingleFinger(RndTransformable *, RndTransformable *, RndTransformable *);

    ObjPtr<RndTransformable> mHand; // 0x28
    ObjPtr<RndTransformable> mForeArm; // 0x34
    ObjPtr<RndTransformable> mUpperArm; // 0x40
    int mBlendInFrames; // 0x4c
    int mBlendOutFrames; // 0x50
    bool mResetHandDest; // 0x54
    bool mResetCurHandTrans; // 0x55
    Transform mCurHandTrans; // 0x58
    Transform mDestHandTrans; // 0x98
    float mFingerCurledLength; // 0xd8
    Vector3 mDestForwardVector; // 0xdc
    Vector3 mCurForwardVector; // 0xec
    /** "Starting hand offset from keyboard." */
    Vector3 mHandKeyboardOffset; // 0xfc
    Hmx::Matrix3 mtx; // 0x10c
    /** "how much to move forward when pinky or thumb is engaged" */
    float mHandMoveForward; // 0x13c
    /** "how much to rotate the hand (radians) when pinky is engaged" */
    float mHandPinkyRotation; // 0x140
    /** "how much to rotate the hand (radians) when thumb is engaged" */
    float mHandThumbRotation; // 0x144
    /** "x offset for right/left hands from average destination position for fingers" */
    float mHandDestOffset; // 0x148
    /** "Does this run the right or left hand?" */
    bool mIsRightHand; // 0x14c
    bool mMoveHand; // 0x14d
    bool mIsSetup; // 0x14e
    std::vector<FingerDesc> mFingers; // 0x150
    float mInv2ab; // 0x15c
    float mAAPlusBB; // 0x160
    /** "This trans will be set to the desired hand position." */
    ObjPtr<RndTransformable> mOutputTrans; // 0x164
    /** "A keyboard bone so we can calculate in local space. use rh/lh targets." */
    ObjPtr<RndTransformable> mKeyboardRefBone; // 0x170
};
