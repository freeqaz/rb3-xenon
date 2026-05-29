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
    virtual void SetName(const char *, ObjectDir *);

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(CharIKFoot)

    RndTransformable *GetData() const { return mData; }
    int GetDataIndex() const { return mDataIndex; }

protected:
    CharIKFoot();

    void DoFSM(Transform &);

    ObjPtr<RndTransformable> mFootBone;
    int mFootFsmState;
    ObjPtr<RndTransformable> mData;
    int mDataIndex;
    Vector3 mFootPosition;
    float mFootBlendTime;
    // RB3 has an ObjPtr<Character> here (set in SetName, used by DoFSM), NOT
    // DC3's Transform mFootTransform. DC3 is newer and diverged; rb3-Wii's
    // retail scratch (decomp.me/scratch/G9HMd) confirms the mMe member.
    ObjPtr<Character> mMe;
};
