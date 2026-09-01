#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "math/Mtx.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Moves an RndTransformable (bone) to another RndTransformable (spot) over time,
    blending from where it was relative to the parent of the spot." */
class CharIKMidi : public RndHighlightable, public CharPollable {
public:
    // Hmx::Object
    virtual ~CharIKMidi();
    OBJ_CLASSNAME(CharIKMidi);
    OBJ_SET_TYPE(CharIKMidi);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndHighlightable
    virtual void Highlight();
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(CharIKMidi)

    void NewSpot(RndTransformable *, float);
    float Frac() const { return mFrac; }

protected:
    CharIKMidi();

    /** "The bone to move" */
    ObjPtr<RndTransformable> mBone; // 0x10
    /** "Spot to go to, zero indexed" */
    ObjPtr<RndTransformable> mCurSpot; // 0x1c
    ObjPtr<RndTransformable> mNewSpot; // 0x28
    Transform mLocalXfm; // 0x34
    Transform mOldLocalXfm; // 0x74
    float mFrac; // 0xb4
    float mFracPerBeat; // 0xb8
    bool mSpotChanged; // 0xbc
    /** "Weightable to change animation between frets" */
    ObjPtr<CharWeightable> mAnimBlender; // 0xc0
    /** "Max weight for animation change" */
    float mMaxAnimBlend; // 0xcc
    float mAnimFracPerBeat; // 0xd0
    float mAnimFrac; // 0xd4
};
