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
        ObjPtr<RndTransformable> mFinger02; // 0x3c
        ObjPtr<RndTransformable> mFinger03; // 0x50
        ObjPtr<RndTransformable> mFingertip; // 0x64
        float mDestFinger02Angle; // 0x78
        float mDestFinger03Angle; // 0x7c
        float mCurFinger02Angle; // 0x80
        float mCurFinger03Angle; // 0x84
        int mBlendFrames; // 0x88
        int mBlendOutFrames; // 0x8c
        bool mNeedsUpdate; // 0x90
        Vector3 mDestOrientVec; // 0x94
        Vector3 mCurOrientVec; // 0xa4
        bool mNeedsIKSolve; // 0xb4
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

    ObjPtr<RndTransformable> mHand; // 0x30
    ObjPtr<RndTransformable> mForeArm; // 0x44
    ObjPtr<RndTransformable> mUpperArm; // 0x58
    int mBlendInFrames; // 0x6c
    int mBlendOutFrames; // 0x70
    bool mResetHandDest; // 0x74
    bool mResetCurHandTrans; // 0x75
    Transform mCurHandTrans; // 0x78
    Transform mDestHandTrans; // 0xb8
    float mFingerCurledLength; // 0xf8
    Vector3 mDestForwardVector; // 0xfc
    Vector3 mCurForwardVector; // 0x10c
    /** "Starting hand offset from keyboard." */
    Vector3 mHandKeyboardOffset; // 0x11c
    Hmx::Matrix3 mtx; // 0x12c
    /** "how much to move forward when pinky or thumb is engaged" */
    float mHandMoveForward; // 0x15c
    /** "how much to rotate the hand (radians) when pinky is engaged" */
    float mHandPinkyRotation; // 0x160
    /** "how much to rotate the hand (radians) when thumb is engaged" */
    float mHandThumbRotation; // 0x164
    /** "x offset for right/left hands from average destination position for fingers" */
    float mHandDestOffset; // 0x168
    /** "Does this run the right or left hand?" */
    bool mIsRightHand; // 0x16c
    bool mMoveHand; // 0x16d
    bool mIsSetup; // 0x16e
    std::vector<FingerDesc> mFingers; // 0x170
    float mInv2ab; // 0x17c
    float mAAPlusBB; // 0x180
    /** "This trans will be set to the desired hand position." */
    ObjPtr<RndTransformable> mOutputTrans; // 0x184
    /** "A keyboard bone so we can calculate in local space. use rh/lh targets." */
    ObjPtr<RndTransformable> mKeyboardRefBone; // 0x198
};
