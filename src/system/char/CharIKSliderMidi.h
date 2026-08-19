#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"

class Character;

/** "Moves an RndTransformable (bone) to a percentage of the way between two spots." */
class CharIKSliderMidi : public RndHighlightable,
                         public CharWeightable,
                         public CharPollable {
public:
    // Hmx::Object
    virtual ~CharIKSliderMidi();
    OBJ_CLASSNAME(CharIKSliderMidi);
    OBJ_SET_TYPE(CharIKSliderMidi);
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

    OBJ_MEM_OVERLOAD(0x1E)
    NEW_OBJ(CharIKSliderMidi)

    void SetFraction(float, float);
    void SetupTransforms();

protected:
    CharIKSliderMidi();

    /** "The bone to move" */
    ObjPtr<RndTransformable> mTarget; // 0x28
    /** "Spot at 0%" */
    ObjPtr<RndTransformable> mFirstSpot; // 0x34
    /** "Spot at 100%" */
    ObjPtr<RndTransformable> mSecondSpot; // 0x40
    Vector3 mDestPos; // 0x6c
    Vector3 mOldPos; // 0x5c
    Vector3 mCurPos; // 0x6c
    float mTargetPercentage; // 0x7c
    float mOldPercentage; // 0x80
    float mFrac; // 0x84
    float mFracPerBeat; // 0x88
    bool mPercentageChanged; // 0x8c
    bool mResetAll; // 0x8d
    ObjPtr<Character> mMe; // 0x90
    /** "Only move if percentage changes more than this (0.0 - 1.0)" */
    float mTolerance; // 0x9c
};
