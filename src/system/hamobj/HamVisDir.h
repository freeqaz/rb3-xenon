#pragma once
#include "hamobj/Pose.h"
#include "gesture/SkeletonDir.h"
#include "gesture/FreestyleMotionFilter.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

struct PoseOwner {
    PoseOwner();
    ~PoseOwner();

    Pose *pose; // 0x0
    Pose *holder; // 0x4
    bool in_pose; // 0x8
    Symbol name; // 0xc
};

/** "panel dir that handles the visualizer" */
class HamVisDir : public SkeletonDir {
public:
    // Hmx::Object
    virtual ~HamVisDir();
    OBJ_CLASSNAME(HamVisDir);
    OBJ_SET_TYPE(HamVisDir);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // RndPollable
    virtual void Enter();
    // SkeletonCallback
    virtual void PostUpdate(const struct SkeletonUpdateData *);

    OBJ_MEM_OVERLOAD(0x1D)
    NEW_OBJ(HamVisDir)

    void Run(bool);
    void SetGrooviness(float);

protected:
    HamVisDir();
    void CheckPose(int, PoseOwner &);
    void CalcArmLengths(std::vector<float> &, const Skeleton &);
    void UpdateGestureFilter(const Skeleton &, int);

    Transform unk284; // 0x24c
    FreestyleMotionFilter *mFilter; // 0x28c
    bool mRunning; // 0x290

    // maybe this all here is a struct in itself
    std::vector<unsigned int> unk2cc; // 0x294
    int unk2d8; // 0x2a0
    int unk2dc; // 0x2a4

    /** "Animated from 0 - 100, depending on player one's hand height" */
    ObjPtr<RndAnimatable> mPlayer1Right; // 0x2a8
    /** "Animated from 0 - 100, depending on player one's hand height" */
    ObjPtr<RndAnimatable> mPlayer1Left; // 0x2b4
    /** "Animated from 0 - 100, depending on player two's hand height" */
    ObjPtr<RndAnimatable> mPlayer2Right; // 0x2c0
    /** "Animated from 0 - 100, depending on player two's hand height" */
    ObjPtr<RndAnimatable> mPlayer2Left; // 0x2cc
    /** "Allow Milo anim bar to drive the gesture propanim frame,
        not the player's skeleton." */
    bool mMiloManualFrame; // 0x2d8
    float mGrooviness; // 0x2dc
    PoseOwner mSquatPoses[2]; // 0x2e0
    PoseOwner mYPoses[2]; // 0x300
};
