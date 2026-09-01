#pragma once
#include "char/CharBones.h"
#include "char/CharPollable.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "hamobj/ErrorNode.h"
#include "hamobj/HamCharacter.h"
#include "math/Mtx.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Class to convert from a camera to a character skeleton" */
class HamSkeletonConverter : public CharPollable,
                             public RndHighlightable,
                             public SkeletonCallback {
public:
    // Hmx::Object
    virtual ~HamSkeletonConverter();
    OBJ_CLASSNAME(HamSkeletonConverter);
    OBJ_SET_TYPE(HamSkeletonConverter);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void SetName(const char *, ObjectDir *);
    // CharPollable
    virtual void Enter();
    virtual void Exit();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);
    // RndHighlightable
    virtual void Highlight();
    // SkeletonCallback
    virtual void Clear() {}
    virtual void Update(const struct SkeletonUpdateData &) {}
    virtual void PostUpdate(const struct SkeletonUpdateData *);
    virtual void Draw(const BaseSkeleton &, class SkeletonViz &) {}

    OBJ_MEM_OVERLOAD(0x1C)
    NEW_OBJ(HamSkeletonConverter)

    void Set(const BaseSkeleton *);

protected:
    HamSkeletonConverter();

    void CalcQuatBone(SkeletonJoint, SkeletonJoint, SkeletonJoint);
    void CalcRotzBone(SkeletonJoint, SkeletonJoint, SkeletonJoint);
    void SetLeg(SkeletonJoint, SkeletonJoint, SkeletonJoint, SkeletonJoint, SkeletonJoint, const BaseSkeleton *, int);
    void SetArm(SkeletonJoint, SkeletonJoint, SkeletonJoint, SkeletonJoint);
    void ScaleBone(SkeletonJoint, SkeletonJoint, SkeletonCoordSys, const Vector3 &, const Vector3 &, const Vector3 &, Vector3 &);
    void GetParentWorldXfm(RndTransformable *, Transform &, SkeletonJoint);
    void SetQuatBoneValue(String, Hmx::Quat);
    void SetRotzBoneValue(String, float);
    void SetPosBoneValue(String, Vector3);
    void RotateTowards(const Vector3 &, const Vector3 &, float, Vector3 &);

    /** "The CharBones object to add into." */
    ObjPtr<CharBonesObject> mBones; // 0x14
    int unk28; // 0x20
    ObjPtr<HamCharacter> mCharacter; // 0x24
    Transform unk40; // 0x30
    PaddedJointPos mJointPositions[kNumJoints]; // 0x70
    Transform mBoneTransforms[kNumJoints]; // 0x1b0
    std::vector<RndTransformable *> mBoneMeshes; // 0x6b0
    RndTransformable *mPelvisMesh; // 0x6bc
    Transform mPelvisTransform; // 0x6c0
    PaddedJointPos mLeftHipZAxisInit; // 0x700
    PaddedJointPos mRightHipZAxisInit; // 0x710
    PaddedJointPos mLeftHipZAxis; // 0x720
    PaddedJointPos mRightHipZAxis; // 0x730
    bool mIsActive; // 0x740
    bool unk751; // 0x741
    float unk754;
    float mPelvisInitialZ;
};
