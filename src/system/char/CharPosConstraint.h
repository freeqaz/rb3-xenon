#pragma once
#include "char/CharPollable.h"
#include "math/Geo.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Forces the targets to be within a world space bounding box relative to source." */
class CharPosConstraint : public CharPollable {
public:
    // Hmx::Object
    virtual ~CharPosConstraint();
    OBJ_CLASSNAME(CharPosConstraint);
    OBJ_SET_TYPE(CharPosConstraint);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // Retail CharPosConstraint::NewObject (registered by CharInit) evaluates
    // StaticClassName() and calls MemAlloc inline, the OBJ_MEM_OVERLOAD shape.
    // Its ??_G calls ?MemFree@@YAXPAX@Z directly, so delete is inlinable.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x18)
    NEW_OBJ(CharPosConstraint)

protected:
    CharPosConstraint();

    /** "Bone to be higher than" */
    ObjPtr<RndTransformable> mSrc; // 0x8
    /** "Bones to constrain" */
    ObjPtrList<RndTransformable> mTargets; // 0x14
    /** "Bounding box, make min > max to ignore that dimension" */
    Box mBox; // 0x28
};
