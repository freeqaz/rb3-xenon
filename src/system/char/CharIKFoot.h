#pragma once
#include "char/CharIKHand.h"
#include "char/Character.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Remedial foot skate ik, not yet ready for prime time." */
class CharIKFoot : public CharIKHand {
    friend class HamRegulate;
public:
    // Hmx::Object
    virtual ~CharIKFoot();
    OBJ_CLASSNAME(CharIKFoot);
    OBJ_SET_TYPE(CharIKFoot);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndHighlightable
    virtual void Highlight() {}
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(CharIKFoot)

    RndTransformable *GetData() const { return mData; }
    int GetDataIndex() const { return mDataIndex; }

protected:
    CharIKFoot();

    void DoFSM(Character *, Transform &);

    ObjPtr<RndTransformable> mFootBone; // 0xb0
    int mFootFsmState; // 0xc4
    ObjPtr<RndTransformable> mData; // 0xc8
    int mDataIndex; // 0xdc
    Vector3 mFootPosition; // 0xe0
    float mFootBlendTime; // 0xf0
    Transform mFootTransform; // 0xf4
};
