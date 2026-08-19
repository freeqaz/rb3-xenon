#pragma once
#include "char/CharCollide.h"
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Pins a hand bone to another RndTransformable, bending the elbow to make it reach.
    Optionally aligns orientations and stretches" */
class CharIKHand : public RndHighlightable, public CharWeightable, public CharPollable {
public:
    struct IKTarget {
        IKTarget(Hmx::Object *);
        IKTarget(ObjPtr<RndTransformable>, float);

        /** "Where to move the hand to" */
        ObjPtr<RndTransformable> mTarget; // 0x0
        /** "Distance along the negative z axis of the transform to snap to" */
        float mExtent; // 0xc
    };
    // Hmx::Object
    virtual ~CharIKHand();
    OBJ_CLASSNAME(CharIKHand);
    OBJ_SET_TYPE(CharIKHand);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndHighlightable
    virtual void Highlight();
    // CharPollable
    virtual void Poll();
    virtual void PollDeps(std::list<Hmx::Object *> &, std::list<Hmx::Object *> &);

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(CharIKHand)

    void SetHand(RndTransformable *);
    void MeasureLengths();

protected:
    CharIKHand();
    void PullShoulder(Vector3 &, Transform const &, Vector3 const &, float);
    void IKElbow(RndTransformable *, RndTransformable *);

    /** "The hand to be moved, must be child of elbow" */
    ObjPtr<RndTransformable> mHand; // 0x28
    /** "If non null, will be the thing that actually hits the target,
        the hand will be moved into such a location as to make it hit.
        You probably always want to turn on orientation in this case, as otherwise,
        the hand will be in a somewhat random orientation,
        which will probably mean that the finger will miss the mark." */
    ObjPtr<RndTransformable> mFinger; // 0x34
    /** "Targets for the hand" */
    ObjVector<IKTarget> mTargets; // 0x40
    /** "Orient the hand to the dest" */
    bool mOrientation; // 0x50
    /** "Stretch the hand to the dest" */
    bool mStretch; // 0x51
    /** "Recalculate bone length every frame, needed for bones which scale" */
    bool mScalable; // 0x52
    /** "Moves the elbow and shoulder to position the hand,
        if false, just teleports the hand" */
    bool mMoveElbow; // 0x53
    /** "Range to swing the elbow in radians to hit target, better looking suggest .7" */
    float mElbowSwing; // 0x54
    /** "Turn this on to do IK calcs even if weight is 0" */
    bool mAlwaysIKElbow; // 0x58
    bool mHandChanged; // 0x71
    /** "Are we allowed to pull the shoulder to reach goal,
        or do we lock the elbow when goal is too far?" */
    bool mPullShoulder; // 0x72
    Vector3 mWorldDst; // 0x74
    float mInv2ab; // 0x84 - precomputed: 1/(2*forearm*hand), law-of-cosines IK denominator inverse
    float mAABB; // 0x88 - precomputed: forearm^2 + hand^2, law-of-cosines constant
    float mAAPlusBB; // 0x8c - total arm reach: forearm + hand
    /** "Constrain the wrist rotation to be believable" */
    bool mConstraintWrist; // 0x78
    /** "Constrain wrist rotation to this angle (in radians)" */
    float mWristRadians; // 0x7c
    /** "Collision sphere that elbow won't enter." */
    ObjPtr<CharCollide> mElbowCollide; // 0x80
    /** "Choose the clockwise solution for the collision detection" */
    bool mClockwise; // 0x8c
};

BinStream &operator>>(BinStream &, CharIKHand::IKTarget &);
