#pragma once
#include "char/CharPollable.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "moves a bone based on the position of the hand, nut, and bridge" */
class CharGuitarString : public CharPollable {
public:
    // Hmx::Object
    virtual ~CharGuitarString();
    OBJ_CLASSNAME(CharGuitarString);
    OBJ_SET_TYPE(CharGuitarString);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // Retail CharGuitarString::NewObject (registered by CharInit) evaluates
    // StaticClassName() and calls MemAlloc inline, the OBJ_MEM_OVERLOAD shape.
    // Its ??_G calls ?MemFree@@YAXPAX@Z directly, so delete is inlinable.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x15)
    NEW_OBJ(CharGuitarString)

protected:
    CharGuitarString();

    bool mOpen; // 0x8
    /** "nut object" */
    ObjPtr<RndTransformable> mNut; // 0xc
    /** "bridge object" */
    ObjPtr<RndTransformable> mBridge; // 0x18
    /** "object to move between nut and bridge" */
    ObjPtr<RndTransformable> mBend; // 0x24
    /** "object to follow" */
    ObjPtr<RndTransformable> mTarget; // 0x30
};
