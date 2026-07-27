#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Rotate a bone to point towards targets" */
class CharBoneTwist : public CharPollable, public CharWeightable {
public:
    // Hmx::Object
    OBJ_CLASSNAME(CharBoneTwist);
    OBJ_SET_TYPE(CharBoneTwist);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // laneAT-f4: retail keeps THIS class's operator new out-of-line + ICF-folded
    // (target CharBoneTwist::NewObject calls the folded `??2CriticalSection@@SAPAXI@Z`
    // thunk with NO StaticClassName call), unlike the OBJ_MEM_OVERLOAD majority.
    // MEM_OVERLOAD is the literal-name, noinline, foldable form.
    MEM_OVERLOAD(CharBoneTwist, 0x19)
    NEW_OBJ(CharBoneTwist)

protected:
    CharBoneTwist();

    /** "Bone to move" */
    ObjPtr<RndTransformable> mBone; // 0x28
    /** "Targets to average to point bone at" */
    ObjPtrList<RndTransformable> mTargets; // 0x3c
};
