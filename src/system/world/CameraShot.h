#pragma once
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Platform.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Trans.h"
#include "rndobj/TransAnim.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "world/Crowd.h"
#include "world/Spotlight.h"

class CamShot;

class CamShotFrame {
public:
    enum BlendEaseMode {
        /** "blend in and out the same amount" */
        kBlendEaseInAndOut = 0,
        /** "slow rate of change, then fast" */
        kBlendEaseIn = 1,
        /** "fast rate of change, then slow" */
        kBlendEaseOut = 2
    };
    CamShotFrame(Hmx::Object *);
    CamShotFrame(Hmx::Object *, const CamShotFrame &);

    void Save(BinStream &) const;
    void Load(BinStreamRev &);
    bool SameTargets(const CamShotFrame &) const;
    void GetCurrentTargetPosition(Vector3 &) const;
    void ApplyScreenOffset(Transform &, RndCam *) const;
    void UpdateTarget() const;
    void BuildTransform(RndCam *, Transform &, bool) const;
    void Interp(const CamShotFrame &, float, float, RndCam *);
    bool
    OnSyncTargets(ObjPtrList<RndTransformable> &, DataNode &, DataArray *, int, PropOp);
    bool OnSyncParent(ObjPtr<RndTransformable> &, DataNode &, DataArray *, int, PropOp);
    bool HasTargets() const;

    float GetDuration() { return mDuration; }
    float GetBlend() { return mBlend; }
    float GetFrame() { return mFrame; }
    void SetFrame(float f) { mFrame = f; }

    /** "Duration this keyframe holds steady" */
    float mDuration; // 0x0
    /** "Duration this keyframe blends into the next one" */
    float mBlend; // 0x4
    /** "Amount to ease into this keyframe". Ranges from 0 to 1000. */
    float mBlendEase; // 0x8
    /** "Amount to ease out to the next keyframe" */
    BlendEaseMode mBlendEaseMode; // 0xc
    float mFrame; // 0x10
    /** "Field of view, in degrees, for this keyframe.
        Same as setting lens focal length below". Ranges from 0 to 360. */
    float mFOV; // 0x14
    /** "Field of view adjustment (not affected by target reframing" */
    float mZoomFOV; // 0x18
    /** "Camera position for this keyframe" */
    Transform mWorldOffset; // 0x1c
    /** "Screen space offset of target for this keyframe" */
    Vector2 mScreenOffset; // 0x5c
    /** "Noise frequency for camera shake" */
    float mShakeNoiseFreq; // 0x64
    /** "Noise amplitude for camera shake" */
    float mShakeNoiseAmp; // 0x68
    /** "Maximum angle for camera shake" */
    Vector2 mShakeMaxAngle; // 0x6c
    /** "0 to 1 scale representing the Depth size of the blur valley
        (offset from the focal target + focus_blur_multiplier) in the Camera Frustrum.
        Zero puts everything in Blur. 1 puts everything in the Blur falloff valley."
        Ranges from 0 to 1. */
    float mBlurDepth; // 0x74
    /** "Maximum blurriness". Ranges from 0 to 1. */
    float mMaxBlur; // 0x78
    /** "Minimum blurriness". Ranges from 0 to 1. */
    float mMinBlur; // 0x7c
    /** "Multiplier of distance from camere to focal target.
        Offsets focal point of blur." */
    float mFocusBlurMultiplier; // 0x80
    /** "Target(s) that the camera should look at" */
    ObjPtrList<RndTransformable> mTargets; // 0x84
    Vector3 mLastTargetPos; // 0x98
    /** "Parent that the camera should attach itself to" */
    ObjPtr<RndTransformable> mParent; // 0xa8
    Transform mTargetXfm; // 0xbc
    /** "The focal point when calculated depth of field" */
    ObjPtr<RndTransformable> mFocalTarget; // 0xfc
    /** "Whether to take the parent object's rotation into account" */
    bool mUseParentRotation; // 0x100
    /** "Only parent on the first frame" */
    bool mParentFirstFrame; // 0x101
    CamShot *mCamShot; // 0x104
};

inline BinStream &operator<<(BinStream &bs, const CamShotFrame &f) {
    f.Save(bs);
    return bs;
}

class CamShotCrowd {
public:
    CamShotCrowd(Hmx::Object *);
    CamShotCrowd(Hmx::Object *, const CamShotCrowd &);

    void Save(BinStream &) const;
    void Load(BinStream &);
    void AddCrowdChars();
    void SetCrowdChars();
    void ClearCrowdChars();
    void GetSelectedCrowd(
        std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> > &
    );
    void AddCrowdChars(
        const std::list<std::pair<RndMultiMesh *, std::list<RndMultiMesh::Instance>::iterator> > *
    );

    /** "The crowd to show for this shot" */
    ObjPtr<WorldCrowd> mCrowd; // 0x0
    /** "How to rotate crowd" */
    CrowdRotate mCrowdRotate; // 0xc
    std::vector<std::pair<int, int> > m3DCharIndices; // 0x18
    CamShot *mCamShot; // 0x1c
};

inline BinStream &operator<<(BinStream &bs, const CamShotCrowd &f) {
    f.Save(bs);
    return bs;
}

class AutoPrepTarget;

/** "A camera shot. This is an animated camera path with keyframed settings." */
// NB(rb3-xenon): retail CamShot is single-inheritance RndAnimatable only
// (per rb3-Wii's CameraShot.h and Ghidra of BandCamShot::GetTotalDuration
// reading mDuration at this+0x190). DC3 derives from RndAnimatable +
// RndTransformable; retail does not.
class CamShot : public RndAnimatable {
public:
    friend class AutoPrepTarget;
    friend class CamShotFrame;
    friend class WorldDir;
    // Hmx::Object
    virtual ~CamShot();
    OBJ_CLASSNAME(CamShot);
    OBJ_SET_TYPE(CamShot);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndAnimatable
    virtual void StartAnim();
    virtual void EndAnim();
    virtual void SetFrame(float, float);
    virtual float StartFrame() { return 0; }
    virtual float EndFrame() { return mDuration; }
    virtual Hmx::Object *AnimTarget() { return sAnimTarget; }
    virtual void ListAnimChildren(std::list<RndAnimatable *> &) const;
    // CamShot
    virtual void SetPreFrame(float, float) {}
    virtual CamShot *CurrentShot() { return nullptr; }

    static void Init();

    OBJ_MEM_OVERLOAD(0xAD)
    NEW_OBJ(CamShot)

    float Duration() const { return mDuration; }
    float GetDurationSeconds() const;
    bool PlatformOk() const;
    void StartAnims(ObjPtrList<RndAnimatable> &);
    void EndAnims(ObjPtrList<RndAnimatable> &);
    void SetFrames(ObjPtrList<RndAnimatable> &, float);
    Symbol Category() const { return mCategory; }
    int Flags() const { return mFlags; }
    int Disabled() const { return mDisabled; }
    bool ShotOver() const { return mShotOver; }
    void Disable(bool, int);
    bool ShotOk(CamShot *);
    bool SetPos(CamShotFrame &, RndCam *);
    RndCam *GetCam();
    void SetParent(RndDir *d) { mParentDir = d; }
    class WorldDir *GetCrowdDir() const;
    void AddAnim(RndAnimatable *);
    void ClearCrowds();
    bool AddCrowd(CamShotCrowd &);

protected:
    CamShot();

    static Hmx::Object *sAnimTarget;

    virtual bool CheckShotStarted();
    virtual bool CheckShotOver(float);
    // ⛔ ApplyDynamicOffsetPreLookAt / ApplyDynamicOffsetPostLookAt /
    // ApplyFinalCamTransform / ZoomFovOffset USED TO BE DECLARED HERE, as the
    // last four virtuals, carrying "these three could be re-ordered, unsure of
    // current order rn".  Retail settles it more strongly than a reordering:
    // THEY ARE NOT IN THE VTABLE AT ALL.  Retail's CamShot RndAnimatable-
    // subobject vtable at 0x820791dc holds 13 slots and ENDS at CheckShotOver
    // (0x824bd750); the next word is 0xffffffff followed by (VA, index) pairs
    // -- a handler table, not slots.  Ours held 17.  The family corroborates
    // without consulting the map: BandCamShot is 14 retail vs 18 ours, i.e.
    // the same 4 extras plus its own SetFrameEx, and retail's last slot and
    // ours are both SetFrameEx -- the two tables agree at BOTH ends and differ
    // by exactly these four insertions.
    //
    // They were a DC3-era addition (absent from the rb3-Wii oracle's CamShot
    // entirely), had ZERO overrides and ZERO call sites tree-wide, and their
    // retail call sites were already removed by an earlier lane -- see the
    // NB(idx233) notes in CameraShot.cpp's BuildTransform and Interp.  Only
    // the declarations were left, and a declaration alone still burns a slot.
    // Do not restore them from dc3-decomp: it is the NEWER engine.

    void CacheFrames();
    void UnHide();
    void DoHide();
    void SetShotOver();
    void GetKey(float, CamShotFrame *&, CamShotFrame *&, float &);
    void Shake(float, float, const Vector2 &, Vector3 &, Vector3 &);

    DataNode OnHasTargets(DataArray *);
    DataNode OnSetPos(DataArray *);
    DataNode OnSetCrowdChars(DataArray *);
    DataNode OnAddCrowdChars(DataArray *);
    DataNode OnClearCrowdChars(DataArray *);
    DataNode OnGetOccluded(DataArray *);
    DataNode OnSetAllCrowdChars3D(DataArray *);
    DataNode OnRadio(DataArray *);

    /** The collection of keyframes. */
    ObjVector<CamShotFrame> mKeyframes; // 0x10
    /** "Whether the animation should loop." */
    bool mLooping; // 0x20
    /** "If looping true, which keyframe to loop to." */
    int mLoopKeyframe; // 0x24
    /** "Near clipping plane for the camera" */
    float mNearPlane; // 0xe8
    /** "Far clipping plane for the camera" */
    float mFarPlane; // 0xec
    /** "Whether to use depth-of-field effect on platforms that support it" */
    bool mUseDepthOfField; // 0xf0
    /** "Filter amount" */
    float mFilter; // 0xf4
    /** "Height above target's base at which to clamp camera" */
    float mClampHeight; // 0xf8
    /** "Category for shot-picking" */
    Symbol mCategory; // 0xfc
    // NB(rb3-xenon): member ORDER below is reconstructed from retail
    // CamShot::Save's own this-relative offsets (r30 == this + 0x260, so
    // member_off == 0x260 - subtrahend). Retail has a 4-byte member at 0x100
    // and a 0xc member at 0x168, and *nothing* between mPostProcOverrides
    // (ends 0x19c) and mCrowds (starts 0x19c). The only two members that can
    // fill those holes are mCrowdStateOverride (Symbol, 4) and mParentDir
    // (ObjPtr, 0xc) — a unique size-wise assignment that is byte-neutral, so
    // the object does NOT grow and every member from mCrowds onward keeps the
    // offset it already had (mCrowds 0x19c / mPS3PerPixel 0x1ac /
    // mGlowSpot 0x1b0 / mFlags 0x1bc all already matched retail).
    // ⚠ This supersedes the old "VBASE_WALL / add int mShotStartedPending"
    // note: that hypothesis put a NEW member at 0x100, which grows the object
    // by 4 and shifts the already-correct tail (mEndHideList..mSetFrameActive)
    // — which is exactly why it measured net -3 whole-binary. CheckShotOver()
    // reads mDuration (0x278) and is at 100%, proving the tail is correct and
    // must not move.
    /** "Force the crowd into a particular state".
        Options are: (none bad ok great
            skills_bad skills_ok skills_great
            realtime_idle realtime_bad realtime_ok realtime_great) */
    Symbol mCrowdStateOverride; // 0x100
    /** "animatables to be driven with the same frame" */
    ObjPtrList<RndAnimatable> mAnims; // 0x104
    /** "Optional camera path to use" */
    ObjPtr<RndTransAnim> mPath; // 0x118
    float mPathFrame; // 0x12c
    /** "Limit this shot to given platform" - the options are kPlatformNone/PS3/Xbox */
    Platform mPlatform; // 0x130
    /** "List of objects to hide while this camera shot is active,
        shows them when done" */
    ObjPtrList<RndDrawable> mHideList; // 0x134
    /** "List of objects to show while this camera shot is active,
        hides them when done" */
    ObjPtrList<RndDrawable> mShowList; // 0x148
    /** "Automatically generated list of objects to hide while this camera shot is active,
        shows them when done.  Not editable" */
    ObjPtrList<RndDrawable> mGenHideList; // 0x15c
#ifdef HX_NATIVE
    // NB(rb3-xenon): DC3-only std::vector mirror used by HamCamShot in the
    // native build. Retail RB3 has no HamCamShot, so this field is absent in
    // the matching layout — guarded out to keep CamShot size correct.
    std::vector<RndDrawable *> mGenHideVector;
#endif
    ObjPtr<RndDir> mParentDir; // 0x168
    /** "List of objects to draw in order instead of whole world" */
    ObjPtrList<RndDrawable> mDrawOverrides; // 0x174
    /** "List of objects to draw after post-processing" */
    ObjPtrList<RndDrawable> mPostProcOverrides; // 0x188
    ObjVector<CamShotCrowd> mCrowds; // 0x19c
    /** "global per-pixel setting for PS3" */
    bool mPS3PerPixel; // 0x1cc
    /** "The spotlight to get glow settings from" */
    ObjPtr<Spotlight> mGlowSpot; // 0x1d0
    int mFlags; // 0x1e4
    ObjPtrList<RndDrawable> mEndHideList; // 0x1e8
    ObjPtrList<RndDrawable> mEndShowList; // 0x1fc
    // ⛔ ORDER IS LOAD-BEARING AND WAS WRONG UNTIL 2026-09-01.  These four
    // Vector3s were declared Desired-first, which is invisible to sizeof (four
    // 0x10 vectors in either order) and therefore survived a 100.0%,
    // zero-mismatch `?NewObject@CamShot@@` whose `li r3, 0x1c8` matches retail
    // exactly.  ?Shake@CamShot@@ is the witness: retail accumulates the random
    // shake into 0x148/0x158 and springs it into 0x128/0x138, while our build
    // did the mirror image -- the two PAIRS swapped, with mShakeVelocity
    // (0x168) and mShakeAngVelocity (0x178) identical on both sides, which is
    // what rules out a whole-class shift.
    Vector3 mLastShakeOffset; // 0x128
    Vector3 mLastShakeAngOffset; // 0x138
    Vector3 mLastDesiredShakeOffset; // 0x148
    Vector3 mLastDesiredShakeAngOffset; // 0x158
    Vector3 mShakeVelocity; // 0x168
    Vector3 mShakeAngVelocity; // 0x178
    CamShotFrame *mLastNext; // 0x270
    CamShotFrame *mLastPrev; // 0x274
    /** "duration of the camshot" */
    float mDuration; // 0x190
    /** "disabled bits" */
    int mDisabled; // 0x27c
    bool mShotStarted; // 0x280
    bool mShotOver; // 0x281
    bool mHidden; // 0x282
    bool mSetFrameActive;
};

class AutoPrepTarget {
public:
    AutoPrepTarget(CamShotFrame &);
    ~AutoPrepTarget();

    static bool sChanging;

private:
    CamShotFrame *mFrame; // 0x0
    CamShot *mShot; // 0x4
    float mOldFilter; // 0x8
    float mOldCamHeight; // 0xc
    float mOldZoomFov; // 0x10
};
