#pragma once
#include "math/Mtx.h"
#include "obj/Data.h"
#include "rndobj/Highlight.h"
#include "utl/MemMgr.h"

/**
 * @brief An object controlling position, rotation, etc. for any derivatives.
 * Original _objects description:
 * "Base class for transformable objects. Trans objects have
 * a 3D position, rotation, and scale."
 */
class RndText;

class RndAmbientOcclusion;

class RndTransformable : public virtual RndHighlightable {
    friend class RndText;
    friend class LightPreset;
    friend class CharForeTwist;
    friend class CharUpperTwist;
    friend class HamIKEffector;
    friend class BandIKEffector;
    friend class RndAmbientOcclusion;
public:
    enum Constraint {
        /** "no constraint" */
        kConstraintNone = 0,
        /** "Uses own local rotation as world rotation" */
        kConstraintLocalRotate = 1,
        /** "Uses [trans_parent] world xfm as own" */
        kConstraintParentWorld = 2,
        /** "Points Y axis at [trans_target]" */
        kConstraintLookAtTarget = 3,
        /** "Flattens self onto the plane specified by [trans_target]" */
        kConstraintShadowTarget = 4,
        /** "Billboard about Z axis only, relative to [trans_target] if set, otherwise
         * relative to current camera" */
        kConstraintBillboardZ = 5,
        /** "Billboard about XZ axes only (no roll), relative to [trans_target] if set,
         * otherwise relative to current camera" */
        kConstraintBillboardXZ = 6,
        /** "Billboards about XYZ axes, relative to [trans_target] if set, otherwise
         * relative to current camera" */
        kConstraintBillboardXYZ = 7,
        /** "Billboards w/o perspective correction, relative to [trans_target] if set,
         * otherwise relative to current camera" */
        kConstraintFastBillboardXYZ = 8,
        /** "Uses [trans_target] world xfm as own" */
        kConstraintTargetWorld = 9,
        /** "Ignores [trans_parent] rotation" */
        kConstraintNoParentRotation = 10,
        /** "Uses current camera as though it were [trans_parent], ignoring its rotation,
         * relative to [trans_target] if set, otherwise relative to current camera" */
        kConstraintSkyBox = 11,
        /** "Uses current camera as though it were [trans_parent], ignoring its rotation,
         * and ignoring the z value, good for skybox bases, relative to [trans_target] if
         * set, otherwise relative to current camera" */
        kConstraintSkyBoxXY = 12
    };

    virtual ~RndTransformable();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Trans)
    OBJ_SET_TYPE(Trans)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void Highlight();
    virtual void Print();

    OBJ_MEM_OVERLOAD(0x1C);

    const Transform &LocalXfm() const { return mLocalXfm; }
    RndTransformable *TransParent() const { return mParent; }
    bool Dirty() const { return mDirty; }
    Constraint TransConstraint() const { return mConstraint; }
    const std::list<RndTransformable *> &Children() const { return mChildren; }
    // dc3 lineage stores children as std::list (RB3-Wii used std::vector).
    // Alias for RB3 game-code callers; same underlying container.
    const std::list<RndTransformable *> &TransChildren() const { return mChildren; }
    void ResetLocalXfm() {
        mLocalXfm.Reset();
        SetDirty();
    }

    Transform &DirtyLocalXfm() {
        SetDirty();
        return mLocalXfm;
    }

    void SetLocalXfm(const Transform &tf) {
        mLocalXfm = tf;
        SetDirty();
    }

    // RB3-era 3-float overload. rb3-Wii kept this because the Vector3& form
    // "doesn't inline nicely and results in a stack related mismatch". Unused
    // inline in TUs that don't call it (no COMDAT emitted), so codegen-neutral
    // for existing units; required by RB3 game TUs (e.g. VocalTrackDir).
    void SetLocalPos(float x, float y, float z) {
        mLocalXfm.v.Set(x, y, z);
        SetDirty();
    }

    void SetLocalPos(const Vector3 &vec) {
        mLocalXfm.v = vec;
        SetDirty();
    }

    void SetLocalRot(const Hmx::Matrix3 &mtx) {
        mLocalXfm.m = mtx;
        SetDirty();
    }

    __forceinline const Transform &WorldXfm() {
        return !mDirty ? mWorldXfm : WorldXfm_Force();
    }

    bool HasDynamicConstraint() {
        return mConstraint >= kConstraintBillboardZ
            || (mConstraint >= kConstraintLookAtTarget && mTarget);
    }

    void GetLocalRot(Vector3 &) const;
    void SetWorldXfm(const Transform &);
    void SetWorldPos(const Vector3 &);
    void SetTransConstraint(Constraint, RndTransformable *, bool);
    void SetTransParent(RndTransformable *, bool);
    void SetLocalRot(Vector3);
    void SetLocalRotIndex(int, float);
    void ComputeLocalXfm(const Transform &tf);
    void DistributeChildren(bool, float);
    void TransformTransAnims(const Transform &);

    static void Init();
    NEW_OBJ(RndTransformable);

public:
    void SetDirty() {
        if (!mDirty)
            SetDirty_Force();
    }

private:
    static Plane sShadowPlane;

    void SetDirty_Force();
    const Transform &WorldXfm_Force();
    void ApplyDynamicConstraint();

    DataNode OnCopyLocalTo(const DataArray *);
    DataNode OnGetLocalPos(const DataArray *);
    DataNode OnGetLocalPosIndex(const DataArray *);
    DataNode OnGetLocalRot(const DataArray *);
    DataNode OnGetLocalRotIndex(const DataArray *);
    DataNode OnSetLocalPos(const DataArray *);
    DataNode OnSetLocalPosIndex(const DataArray *);
    DataNode OnSetLocalRot(const DataArray *);
    DataNode OnSetLocalRotIndex(const DataArray *);
    DataNode OnSetLocalRotMat(const DataArray *);
    DataNode OnSetTransConstraint(const DataArray *);
    DataNode OnSetLocalScale(const DataArray *);
    DataNode OnSetLocalScaleIndex(const DataArray *);
    DataNode OnGetLocalScale(const DataArray *);
    DataNode OnGetLocalScaleIndex(const DataArray *);
    DataNode OnGetWorldForward(const DataArray *);
    DataNode OnGetWorldRight(const DataArray *);
    DataNode OnGetWorldUp(const DataArray *);
    DataNode OnGetWorldPos(const DataArray *);
    DataNode OnGetWorldRot(const DataArray *);
    DataNode OnGetChildren(const DataArray *);
    DataNode OnCopyWorldTransFrom(const DataArray *);
    DataNode OnCopyWorldPosFrom(const DataArray *);

protected:
    RndTransformable();

    virtual void UpdatedWorldXfm() {}

    // Retail RB3-360 member order (verified from binary fn_823E6E98 constructor):
    // Parent/children/target pointers come before and after transforms.
    // mDirty precedes mConstraint (different from DC3).
    ObjOwnerPtr<RndTransformable> mParent; // 0x8  (ObjOwnerPtr = 0xc retail)
    std::list<RndTransformable *> mChildren; // 0x14 (list = 0x8)
    Transform mLocalXfm; // 0x1c
    Transform mWorldXfm; // 0x5c
    bool mDirty; // 0x9c  (before mConstraint in retail, unlike DC3)
    Constraint mConstraint; // 0xa0
    bool mPreserveScale; // 0xa4
    ObjPtr<RndTransformable> mTarget; // 0xa8  (ObjPtr = 0xc retail)

#ifdef HX_NATIVE
    // ─── X18 NATIVE-ONLY DIAGNOSTIC: WHO LAST WROTE mWorldXfm ──────────────
    //
    // WHY THIS EXISTS. The hand-pose oracle checks `W == L*parentW`. That is
    // NOT the engine's world rule -- it is only one of the ways mWorldXfm gets
    // its value. WorldXfm() (Trans.h:118) returns the CACHED mWorldXfm whenever
    // !mDirty, so a bone whose world was written by any path OTHER than
    // WorldXfm_Force will fail the check even though the engine is behaving
    // exactly as designed. X17 flagged this as the gate's blind spot and could
    // not settle whether the residual it reports is a defect or an artifact.
    //
    // A last-writer tag settles it, because it is the ONLY thing the deviation
    // number cannot tell you. mWorldXfm is private to this class and is written
    // in exactly five places in Trans.cpp (verified by grep over src/ + native/:
    // no other TU touches it). All five are tagged, so no writer can be
    // silently misclassified as another.
    //
    // ⛔ NOT INVENTED: this changes no transform, no pose, no traversal and no
    // control flow. It is a write-only byte, read only by the audit path.
    // X360 blast radius is zero BY CONSTRUCTION -- the member, its
    // initialisation and every assignment are inside `#ifdef HX_NATIVE`, which
    // the X360 arm does not define, so the retail class layout is untouched.
public:
    enum WorldWriter {
        kWorldNeverWritten = 0, // still the ctor's Reset() identity
        kWorldComposed = 1, // WorldXfm_Force -- the ONLY path the gate models
        kWorldPublished = 2, // SetWorldXfm / SetWorldPos -- IK, placement
        kWorldLoaded = 3, // read straight off the .milo stream
        kWorldCopied = 4, // Copy() from another RndTransformable
    };
    unsigned char WorldWriterTag() const { return mWorldWriterTag; }
    // Return address of whoever last called SetWorldXfm on this bone. Resolved
    // to a symbol offline with addr2line -- names the publisher without
    // guessing from bone names.
    void *WorldPubCaller() const { return mWorldPubCaller; }
    // ⚠ TEST-ONLY NEGATIVE-CONTROL HOOK, never called outside the audit.
    // Forges the ONE state the corrected gate exists to catch: a bone that is
    // marked clean and tagged COMPOSED while its cached world is stale with
    // respect to its parent -- i.e. a dirty-propagation failure. Without this
    // the corrected gate has only ever been observed PASSING, and a gate never
    // seen to fail is not evidence of anything.
    void NativeMakeStaleForTest() {
        mDirty = false;
        mWorldWriterTag = kWorldComposed;
    }
    static const char *WorldWriterName(unsigned char w) {
        switch (w) {
        case kWorldComposed:
            return "COMPOSED";
        case kWorldPublished:
            return "PUBLISHED";
        case kWorldLoaded:
            return "LOADED";
        case kWorldCopied:
            return "COPIED";
        default:
            return "NEVER";
        }
    }

protected:
    unsigned char mWorldWriterTag;
    void *mWorldPubCaller;
#endif
};

class RndTransformableRemover : public RndTransformable {
public:
    RndTransformableRemover() {}
    virtual Symbol ClassName() const { return ""; }
};
