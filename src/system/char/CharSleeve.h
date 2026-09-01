#pragma once
#include "char/CharPollable.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

class Character;

class CharSleeve : public RndHighlightable, public CharPollable {
public:
    // Hmx::Object
    virtual ~CharSleeve();
    OBJ_CLASSNAME(CharSleeve)
    OBJ_SET_TYPE(CharSleeve)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(Hmx::Object const *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // RndPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // RndHighlightable
    virtual void Highlight();

    OBJ_MEM_OVERLOAD(0x1E)
    NEW_OBJ(CharSleeve)

    ObjPtr<RndTransformable> mSleeve; // 0x10
    ObjPtr<RndTransformable> mTopSleeve; // 0x1c
    Vector3 mPos; // 0x28
    Vector3 mLastPos; // 0x38
    float mLastDT; // 0x48
    float mInertia; // 0x4c
    float mGravity; // 0x50
    float mRange; // 0x54
    float mNegLength; // 0x58
    float mPosLength; // 0x5c
    float mStiffness; // 0x60
    ObjPtr<Character> mMe; // 0x64

protected:
    CharSleeve();
};
