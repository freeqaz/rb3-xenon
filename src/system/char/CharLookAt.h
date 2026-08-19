#pragma once
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "math/Geo.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

/** "Makes a source point at dest by rotating a pivot, points along Y axis of source" */
class CharLookAt : public RndHighlightable, public CharWeightable, public CharPollable {
public:
    // Hmx::Object
    virtual ~CharLookAt();
    OBJ_CLASSNAME(CharLookAt);
    OBJ_SET_TYPE_ENGINE(CharLookAt);
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

    OBJ_MEM_OVERLOAD(0x19)
    NEW_OBJ(CharLookAt)

    void SetMinYaw(float);
    void SetMaxYaw(float);
    void SetMinPitch(float);
    void SetMaxPitch(float);

    static bool sDisableJitter;

    friend class CharEyes;

    RndTransformable *GetSource() const {
        const ObjPtr<RndTransformable> &ptr = mSource ? mSource : mPivot;
        return ptr;
    }
    // dc3 RENAMED rb3-Wii's `mDest` to `mTarget`; it was never dropped. The
    // compiler layout report puts mTarget at 0x40 (mSource 0x28, mPivot 0x34),
    // matching the rb3-Wii oracle exactly, and retail's BandCharacter::Poll
    // loads the ObjPtr's object word at 0x48 == 0x40+8. The old
    // `return GetSource()` alias made us emit GetSource's two-way
    // mSource?mSource:mPivot select instead of retail's single load.
    RndTransformable *GetDest() const { return mTarget; }

protected:
    CharLookAt();

    void SyncLimits();

    /** "If non null, the bone which looks at along its Y axis,
        otherwise equal to the pivot" */
    ObjPtr<RndTransformable> mSource; // 0x30
    /** "The thing that pivots" */
    ObjPtr<RndTransformable> mPivot; // 0x44
    /** "The thing to look at" */
    ObjPtr<RndTransformable> mTarget; // 0x58
    /** "Seconds of lag when moving the source" */
    float mHalfTime; // 0x6c
    /** "Degrees of min allowable yaw, looking left". Ranges from -80 to 80. */
    float mMinYaw; // 0x50
    /** "Degrees of max allowable yaw, looking right". Ranges from -80 to 80. */
    float mMaxYaw; // 0x54
    /** "Degrees of min allowable pitch, looking down". Ranges from -80 to 80. */
    float mMinPitch; // 0x58
    /** "Degrees of max allowable pitch, looking up". Ranges from -80 to 80. */
    float mMaxPitch; // 0x5c
    /** "Degrees of yaw to start auto-weight, -1 means no auto-weight" */
    float mMinWeightYaw; // 0x80
    /** "Degrees of yaw to stop auto-weight, must be greater than mMinWeightYaw" */
    float mMaxWeightYaw; // 0x84
    /** "Max speed in weight/sec that the auto-weight can change" */
    float mWeightYawSpeed; // 0x88
    Vector3 mPivotLookTarget; // 0x8c
    float mPivotLookWeight; // 0x9c
    /** "radius in degrees of filtered source motion that's allowed through" */
    float mSourceRadius; // 0xa0
    Vector3 unka4; // 0xa4
    Box mLookLimits; // 0xb4
    /** "Graphically show the extreme ranges of motion" */
    bool mShowRange; // 0xb4
    // NOTE: retail RB3-360 (MILO_DEBUG off) does NOT contain the three
    // debug-only members that rb3-Wii gates under #ifdef MILO_DEBUG here
    // (bool mTestRange; float mTestRangePitch; float mTestRangeYaw;).
    // Retail-verified via CharLookAt::Copy @0x823a95d0 and RTTI COL
    // 0x821cefd4: tail bools are consecutive at 0xb5-0xb7, jitter floats at
    // 0xb8/0xbc, virtual base Hmx::Object at 0xc4. Multiple TUs see this
    // layout (Char/CharEyes/Character/BandCharacter/HamCharacter), so the
    // members are removed outright — do not re-add or re-gate (our build
    // force-defines MILO_DEBUG in macros.h).
    /** "If true allows rolling, if false,
        keeps the local pivot z axis down to prevent rolling.
        Eyeballs can't roll, for instance, but heads can." */
    bool mAllowRoll; // 0xb5
    bool mDisableRoll; // 0xb6
    /** "If enabled, high frequency noise is added to pitch and/or yaw each frame" */
    bool mEnableJitter; // 0xb7
    /** "if enable_jitter is on, random noise from
        [-yaw_jitter_limit, yaw_jitter_limit] (in degrees)
        is applied to the yaw of the lookat". Ranges from 0 to 10. */
    float mYawJitterLimit; // 0xb8
    /** "if enable_jitter is on, random noise from
        [-pitch_jitter_limit, pitch_jitter_limit] (in degrees)
        is applied to the pitch of the lookat". Ranges from 0 to 10. */
    float mPitchJitterLimit; // 0xbc
};
