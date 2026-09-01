#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

/** "Group of Charpollable, polled in the order given,
    use when the automatic CharPollable sorting is not correct or sufficient." */
class CharPollGroup : public CharPollable, public CharWeightable {
public:
    // Hmx::Object
    OBJ_CLASSNAME(CharPollGroup)
    OBJ_SET_TYPE(CharPollGroup)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void ListPollChildren(std::list<RndPollable *> &) const;
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    // laneAT-f4: retail keeps THIS class's operator new out-of-line + ICF-folded
    // (target CharPollGroup::NewObject calls the folded `??2CriticalSection@@SAPAXI@Z`
    // thunk with NO StaticClassName call), unlike the OBJ_MEM_OVERLOAD majority.
    // MEM_OVERLOAD is the literal-name, noinline, foldable form.
    MEM_OVERLOAD(CharPollGroup, 0x15)
    NEW_OBJ(CharPollGroup);

    void SortPolls();

    /** "Ordered list of CharPollables, will be polled in this order." */
    ObjPtrList<CharPollable> mPolls; // 0x20
    /** "Explicit thing I am changed by, to force sorting, if set, ignores polls" */
    ObjPtr<CharPollable> mChangedBy; // 0x34
    /** "Explicit thing I change, to force sorting, if set, ignores polls" */
    ObjPtr<CharPollable> mChanges; // 0x40

protected:
    CharPollGroup();
};
