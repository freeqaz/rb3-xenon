#pragma once
#include "hamobj/DancerSkeleton.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "utl/MemMgr.h"

// size 0x2dc
struct DancerFrame {
    short mMoveIdx; // 0x0
    short mMoveFrameIdx; // 0x2
    DancerSkeleton mSkeleton; // 0x4
};

/** "Linear sequence of DancerFrame structs, animatable for preview in milo" */
class DancerSequence : public RndAnimatable {
public:
    // Hmx::Object
    OBJ_CLASSNAME(DancerSequence);
    OBJ_SET_TYPE(DancerSequence);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndAnimatable
    virtual void SetFrame(float frame, float blend);
    virtual float StartFrame() { return 0; }
    virtual float EndFrame();

    OBJ_MEM_OVERLOAD(0x1E);
    NEW_OBJ(DancerSequence);

    const std::vector<DancerFrame> &GetDancerFrames() const;
    const DancerSkeleton *CurSkeleton() const;

protected:
    DancerSequence();

    // RB3 retail's DancerSequence carries 0x2c (44) bytes of derived members
    // that DC3's newer source dropped, placed BEFORE mDancerFrames (not after
    // -- verified against the retail ctor's vbase/vector-init offsets: the
    // vbase-to-Object offset only needs +8 while mDancerFrames itself needs
    // +44, which only balances if the filler precedes the vector and no
    // padding follows it).
    char mRB3Pad[0x2c]; // 0x10
    std::vector<DancerFrame> mDancerFrames; // 0x3c
};
