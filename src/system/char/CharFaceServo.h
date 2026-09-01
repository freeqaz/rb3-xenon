#pragma once
#include "char/CharBonesMeshes.h"
#include "char/CharClip.h"
#include "char/CharPollable.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

/** "BonesMeshes for facial blending" */
class CharFaceServo : public CharPollable, public CharBonesMeshes {
public:
    // Hmx::Object
    virtual ~CharFaceServo();
    OBJ_CLASSNAME(CharFaceServo);
    OBJ_SET_TYPE(CharFaceServo);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // CharPollable
    virtual void Poll();
    virtual void Enter();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &list) {
        StuffMeshes(list);
    }
    // CharBonesMeshes
    virtual void ScaleAdd(CharClip *, float, float, float);

    OBJ_MEM_OVERLOAD(0x16)
    NEW_OBJ(CharFaceServo)

    friend class CharEyes;

    void SetClips(ObjectDir *);
    void SetClipType(Symbol);
    void SetBlinkClipLeft(Symbol);
    void SetBlinkClipRight(Symbol);

    float BlinkWeightLeft() const;
    void ApplyProceduralWeights();
    CharClip *BaseClip() const { return mBaseClip; }
    void SetProceduralBlinkWeight(float weight) { mProceduralBlinkWeight = weight; }
    Symbol BlinkClipLeftName() const { return mBlinkClipLeftName; }
    Symbol BlinkClipRightName() const { return mBlinkClipRightName; }

protected:
    CharFaceServo();
    void TryScaleDown();

    virtual void ReallocateInternal() { CharBonesMeshes::ReallocateInternal(); }

    /** "pointer to visemes, must contain Blink and Base" */
    ObjPtr<ObjectDir> mClips; // 0x74
    /** "Which clip type it can support" */
    Symbol mClipType; // 0x80
    ObjPtr<CharClip> mBaseClip; // 0x84
    /** "Blink clip, used to close the left eye" */
    Symbol mBlinkClipLeftName; // 0x90
    /** "A second clip that contributes to closing the left eye" */
    Symbol mBlinkClipLeftName2; // 0x94
    /** "Blink clip, used to close the right eye" */
    Symbol mBlinkClipRightName; // 0x98
    /** "A second clip that contributes to closing the right eye" */
    Symbol mBlinkClipRightName2; // 0x9c
    ObjPtr<CharClip> mBlinkClipLeft; // 0xa0
    ObjPtr<CharClip> mBlinkClipLeft2; // 0xac
    ObjPtr<CharClip> mBlinkClipRight; // 0xb8
    ObjPtr<CharClip> mBlinkClipRight2; // 0xc4
    float mBlinkWeightLeft; // 0xd0
    float mBlinkWeightRight; // 0xd4
    bool mNeedScaleDown; // 0xd8
    float mProceduralBlinkWeight; // 0xdc
    bool mAppliedProceduralBlink; // 0xe0
};
