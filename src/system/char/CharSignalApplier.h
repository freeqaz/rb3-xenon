#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "stl/_vector.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

class CharSignalApplier : public CharPollable, public CharWeightable {
public:
    struct BoneOp {
        BoneOp(Hmx::Object *o);
        BoneOp &operator=(const BoneOp &);

        ObjPtr<RndTransformable> mBone; // 0x00 (retail ObjPtr = 0xc)
        int mOp;                        // 0x0c
        float mApplyPercent;            // 0x10
        float mMinAngle;                // 0x14
        // NOTE: DC3 (newer engine) added a 4th field mMaxAngle here, growing
        // sizeof(BoneOp) 0x18->0x1c. Retail RB3 TU5 has only 3 post-head fields
        // (element stride 0x18 in every vector<BoneOp> STL helper). Keep at 24B.
    };

    // Hmx::Object
    OBJ_CLASSNAME(CharSignalApplier);
    OBJ_SET_TYPE(CharSignalApplier);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // RndPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(CharSignalApplier);

protected:
    CharSignalApplier();

    float mSignal; // 0x28 - current signal value
    float mSignalMin; // 0x2c
    float mSignalMax; // 0x30
    bool mDoSmoothing; // 0x34
    float mSmoothIncrement; // 0x38
    float mSmoothedSignal; // 0x3c
    ObjVector<CharSignalApplier::BoneOp> mBoneOps; // 0x40
};

bool PropSync(CharSignalApplier::BoneOp &, DataNode &, DataArray *, int, PropOp);
