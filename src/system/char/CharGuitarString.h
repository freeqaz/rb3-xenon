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

    // laneAT-f4: retail keeps THIS class's operator new out-of-line + ICF-folded
    // (target CharGuitarString::NewObject calls the folded `??2CriticalSection@@SAPAXI@Z`
    // thunk with NO StaticClassName call), unlike the OBJ_MEM_OVERLOAD majority.
    // MEM_OVERLOAD is the literal-name, noinline, foldable form.
    MEM_OVERLOAD(CharGuitarString, 0x15)
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
