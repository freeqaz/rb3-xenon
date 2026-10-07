// Retail INLINES the owner-only ObjPtr<RndTransformable> ctor in CharIKHand::Load
// (target: `lis r10,lbl_82017A34` + three stores); without this we bind the
// out-of-line two-arg body. Must precede every include.
#define RB3_OBJPTR_INLINE_OWNER_CTOR
// ...and stores it owner, object, then vtable (ObjVector<IKTarget>::resize's
// fill value, PropSync's appended element).
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT

#include "char/CharBlendBone.h"
#include "char/CharIKHand.h"
#include "char/CharWeightable.h"
#include "math/Color.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "rndobj/Utl.h"

#pragma region CharIKHand

// mPullShoulder (0x5a) is deliberately NOT in this init list: retail's ctor
// asm stores mAlwaysIKElbow(0x58) then jumps straight to mConstraintWrist
// (0x78) with no store to 0x5a at all, so there is no mPullShoulder
// initializer here
// (retail predates the `pull_shoulder` property; see the
// SYNC_PROP comment above). The member is left uninitialized on retail, same
// as here.
CharIKHand::CharIKHand()
    : mHand(this), mFinger(this), mTargets(this), mOrientation(true), mStretch(true),
      mScalable(false), mMoveElbow(true), mElbowSwing(0), mAlwaysIKElbow(false),
      mAAPlusBB(0), mConstraintWrist(false), mWristRadians(0),
      mElbowCollide(this), mClockwise(false) {}

CharIKHand::~CharIKHand() {}

BEGIN_HANDLERS(CharIKHand)
    HANDLE_ACTION(measure_lengths, MeasureLengths())
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharIKHand::IKTarget)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(extent, o.mExtent)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharIKHand)
    SYNC_PROP_SET(hand, mHand.Ptr(), SetHand(_val.Obj<RndTransformable>()))
    SYNC_PROP(finger, mFinger)
    SYNC_PROP(targets, mTargets)
    SYNC_PROP(orientation, mOrientation)
    SYNC_PROP(stretch, mStretch)
    SYNC_PROP(scalable, mScalable)
    SYNC_PROP(move_elbow, mMoveElbow)
    SYNC_PROP(elbow_swing, mElbowSwing)
    SYNC_PROP(always_ik_elbow, mAlwaysIKElbow)
    // RB3 spells this property `constrain_wrist`; `constraint_wrist` is DC3's rename.
    // Retail band.exe: "constrain_wrist" x1, "constraint_wrist" x0 (positive controls
    // in the same scan: "elbow_collide" x1, "wrist_radians" x1).  The metric cannot
    // see this -- objdiff masks the .rdata reloc arg -- so it is a correctness fix.
    // Member name left as mConstraintWrist to avoid a header/Save/Load/Copy cascade.
    SYNC_PROP(constrain_wrist, mConstraintWrist)
    SYNC_PROP(wrist_radians, mWristRadians)
    SYNC_PROP(elbow_collide, mElbowCollide)
    SYNC_PROP(clockwise, mClockwise)
#ifdef HX_NATIVE
    // DC3-only property: retail RB3 has no `pull_shoulder` prop at
    // all -- only the PullShoulder() method.  Retail's SyncProperty COMDAT holds 13
    // ??0Symbol@@QAA@PBD@Z relocs, exactly the 13-property list, against our 14.
    SYNC_PROP(pull_shoulder, mPullShoulder)
#endif
    SYNC_SUPERCLASS(CharWeightable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const CharIKHand::IKTarget &t) {
    bs << t.mTarget;
    bs << t.mExtent;
    return bs;
}

BEGIN_SAVES(CharIKHand)
    SAVE_REVS(0xC, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mHand;
    bs << mFinger;
    bs << mTargets;
    bs << mOrientation;
    bs << mStretch;
    bs << mScalable;
    bs << mMoveElbow;
    bs << mElbowSwing;
    bs << mAlwaysIKElbow;
    bs << mConstraintWrist;
    bs << mWristRadians;
    bs << mElbowCollide;
    bs << mClockwise;
END_SAVES

BEGIN_COPYS(CharIKHand)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharIKHand)
    BEGIN_COPYING_MEMBERS
        SetHand(c->mHand);
        COPY_MEMBER(mFinger)
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mOrientation)
        COPY_MEMBER(mStretch)
        COPY_MEMBER(mScalable)
        COPY_MEMBER(mMoveElbow)
        COPY_MEMBER(mElbowSwing)
        COPY_MEMBER(mAlwaysIKElbow)
        COPY_MEMBER(mConstraintWrist)
        COPY_MEMBER(mWristRadians)
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mElbowCollide)
        COPY_MEMBER(mClockwise)
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharIKHand;
BEGIN_LOADS(CharIKHand)
    int rev;
    bs >> rev;
    gRevs_CharIKHand.rev = getHmxRev(rev);
    gRevs_CharIKHand.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    CharWeightable::Load(bs);
    bs >> mHand;
    if (gRevs_CharIKHand.rev > 4)
        bs >> mFinger;
    else
        mFinger = 0;
    if (gRevs_CharIKHand.rev < 3) {
        // ONE-ARG spelling: retail INLINES this ctor (three stores + the vtable
        // materialization `lis r10,lbl_82017A34`). The two-arg spelling binds the
        // out-of-line body and RB3_OBJPTR_INLINE_OWNER_CTOR cannot reach it --
        // that lever only splits the OWNER-ONLY ctor.
        ObjPtr<RndTransformable> tPtr(this);
        bs >> tPtr;
        mTargets.clear();
        mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
    } else if (gRevs_CharIKHand.rev < 0xB) {
        ObjPtrList<RndTransformable> tList(this, kObjListNoNull);
        bs >> tList;
        mTargets.clear();
        for (ObjPtrList<RndTransformable>::iterator it = tList.begin(); it != tList.end();
             ++it) {
            ObjPtr<RndTransformable> tPtr(this, *it);
            mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
        }
    } else
        bs >> mTargets;

    bs >> mOrientation;
    bs >> mStretch;
    if (gRevs_CharIKHand.rev > 1)
        bs >> mScalable;
    else
        mScalable = false;

    if (gRevs_CharIKHand.rev > 3)
        bs >> mMoveElbow;
    else
        mMoveElbow = true;

    if (gRevs_CharIKHand.rev > 5)
        bs >> mElbowSwing;
    else
        mElbowSwing = 0.0f;

    if (gRevs_CharIKHand.rev > 6)
        bs >> mAlwaysIKElbow;
    if (gRevs_CharIKHand.rev > 7) {
        bs >> mConstraintWrist;
        bs >> mWristRadians;
    }
    if (gRevs_CharIKHand.rev == 9) {
        // NOTE: tPtr and this String share a stack slot pair that is PERMUTED vs
        // retail (target r31+0x68, ours r31+0x78) -- 4 `addi` diff_args.
        // CORRECTION (lane MATCH-G): the older note here said "mpn reads 100
        // (function counted, bytes withheld)".  That is WRONG -- report.json has
        // fuzzy 99.9837 AND mpn 99.9837, so the row is NOT counted on either
        // ruler and is worth +1 function AND +980 B, not bytes alone.  These are
        // plain immediates, not relocation args, so mpn cannot mask them.
        //
        // Slot picture: retail A.copytemp=0x68, B.tPtr=0x68, B.copytemp=0x78,
        // s=0x68; ours moves A.copytemp and s to 0x78.  Both frames are 0x120.
        // Tried and REFUTED (MATCH-G): making branch B's ObjPtr an unnamed
        // temporary -- `IKTarget(ObjPtr<RndTransformable>(this, *it), 0)` -- on
        // the theory that a named local was reserving 0x68 and pushing the temp
        // pool up.  MSVC instead ELIDES the copy ctor (3 deletes) and shrinks the
        // frame to 0x110: 99.41 -> 97.35 raw.  So retail really does construct
        // twice, and the named local is load-bearing.  Declaring `b` before `s`
        // was tried earlier and changed nothing.  This is MSVC slot coloring,
        // i.e. permuter territory, and the permuter is OFF by directive.
        String s;
        bs >> s;
        bool b;
        bs >> b;
    }
    if (gRevs_CharIKHand.rev > 0xB) {
        bs >> mElbowCollide;
        bs >> mClockwise;
    }
    SetHand(mHand);
END_LOADS

void CharIKHand::SetHand(RndTransformable *t) {
    mHand = t;
    mHandChanged = true;
}

void CharIKHand::PullShoulder(
    Vector3 &v, const Transform &tf, const Vector3 &vconst, float fff
) {
    Subtract(vconst, tf.v, v);
    float f2 = fff * 0.95f;
    float lensq = LengthSquared(v);
    if (lensq > f2 * f2) {
        v *= 1.0f - f2 / (float)std::sqrt(lensq);
        return;
    }
    v.y = 0;
    v.x = 0;
    v.z = 0;
}

void CharIKHand::MeasureLengths() {
    if (mHand) {
        if (mHand->TransParent()) {
            if (mHand->TransParent()->TransParent()) {
                float len = Length(mHand->LocalXfm().v);
                float parentlen = Length(mHand->TransParent()->LocalXfm().v);
                mInv2ab = parentlen * 2.0f * len;
                mAABB = (parentlen * parentlen) + len * len;
                if (mInv2ab != 0.0f)
                    mInv2ab = 1.0f / mInv2ab;
                mAAPlusBB = len + parentlen;
            }
        }
    }
}

void CharIKHand::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mHand);
    changedBy.push_back(mHand);
    change.push_back(mFinger);
    changedBy.push_back(mFinger);
    for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
         ++it) {
        changedBy.push_back(it->mTarget);
    }
    if (mMoveElbow && mHand) {
        RndTransformable *handParent = mHand->TransParent();
        if (handParent) {
            change.push_back(handParent);
            changedBy.push_back(handParent);
            handParent = handParent->TransParent();
            if (handParent) {
                change.push_back(handParent);
                changedBy.push_back(handParent);
            }
        }
    }
}

void CharIKHand::Poll() {
    float charWeight = Weight();
    static const float kHalfPi = 1.570796370506287f;

    static const float kMaxWeight = 144.0f;
    RndTransformable *hand = mHand;
    if (!hand || mTargets.empty())
        return;
    // Retail reads both flags through `self`; reading mScalable off the
    // adjusted `this` schedules `li r28, 0` ahead of `subi r27, r3, 0x20`.
    CharIKHand *self = this;
    Vector3 destPos(0.0f, 0.0f, 0.0f);
    Hmx::Quat destQuat(0.0f, 0.0f, 0.0f, 0.0f);
    if (self->mScalable || self->mHandChanged) {
        self->MeasureLengths();
        self->mHandChanged = false;
    }
    if (mTargets.size() == 1) {
        RndTransformable *frontTarget = mTargets.front().mTarget;
        if (frontTarget) {
            destPos = frontTarget->WorldXfm().v;
            if (mOrientation) {
                Hmx::Matrix3 normMat;
                Normalize(frontTarget->WorldXfm().m, normMat);
                destQuat.Set(normMat);
            }
        }
    } else {
        float totalWeight = 0.0f;
        // Indexed weights with end() re-read in each loop condition: retail
        // reloads begin in this branch, copies end for the first loop and
        // forms the weight pointer inside each loop guard.  The 0.001f stays
        // a literal so its pool load is scheduled before kMaxWeight's.
        float localWeights[16];
        int i = 0;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
             ++it, i++) {
            RndTransformable *targetTrans = it->mTarget;
            float extent = it->mExtent;
            if (targetTrans) {
                Vector3 targetVec(targetTrans->LocalXfm().v);
                if (extent > 0.0f) {
                    if (-targetVec.z <= extent) {
                        targetVec.z = 0.0f;
                        localWeights[i] =
                            kMaxWeight / Max(0.001f, LengthSquared(targetVec));
                    } else {
                        localWeights[i] = 0.001f;
                    }
                } else {
                    localWeights[i] = kMaxWeight / Max(0.001f, LengthSquared(targetVec));
                }
                totalWeight += localWeights[i];
            }
        }
        if (totalWeight < 1.0f) {
            charWeight = charWeight - (charWeight * (1.0f - totalWeight));
        }
        i = 0;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
             ++it, i++) {
            RndTransformable *targetTrans = it->mTarget;
            if (targetTrans) {
                float curWeight = localWeights[i] / totalWeight;
                const Transform &worldXfm = targetTrans->WorldXfm();
                ScaleAdd(destPos, worldXfm.v, curWeight, destPos);
                if (mOrientation) {
                    Hmx::Matrix3 normMat;
                    Normalize(worldXfm.m, normMat);
                    Hmx::Quat q(normMat);
                    ScaleAddEq(destQuat, q, curWeight);
                }
            }
        }
        if (mOrientation)
            Normalize(destQuat, destQuat);
    }
    if (mFinger) {
        Transform tf;
        tf.v = destPos;
        MakeRotMatrix(destQuat, tf.m);
        Transform invFingerXfm;
        Invert(mFinger->WorldXfm(), invFingerXfm);
        Multiply(mHand->WorldXfm(), invFingerXfm, invFingerXfm);
        Multiply(invFingerXfm, tf, tf);
        destPos = tf.v;
        destQuat.Set(tf.m);
    }
    Interp(mHand->WorldXfm().v, destPos, charWeight, mWorldDst);
    RndTransformable *elbowParent = 0;
    RndTransformable *shoulderParent = mHand->TransParent();
    if (!mMoveElbow)
        shoulderParent = 0;
    if (charWeight != 0 || mAlwaysIKElbow) {
        if (shoulderParent) {
            elbowParent = shoulderParent->TransParent();
            if (!elbowParent)
                shoulderParent = 0;
        }
        self->IKElbow(shoulderParent, elbowParent);
    }
    if (charWeight != 0 && (!shoulderParent || mOrientation || mStretch)) {
        Transform handXfm(mHand->WorldXfm());
        if (!shoulderParent || mStretch) {
            handXfm.v = mWorldDst;
        }
        if (mOrientation) {
            if (charWeight < 1.0f) {
                Hmx::Quat curQuat(mHand->WorldXfm().m);
                Interp(curQuat, destQuat, charWeight, destQuat);
            }
            MakeRotMatrix(destQuat, handXfm.m);
        }
        mHand->SetWorldXfm(handXfm);
    }

    if (mConstraintWrist && charWeight > 0.0f && shoulderParent) {
        Vector3 fingerSavedPos(mFinger->WorldXfm().v);
        Hmx::Matrix3 elbowMat(shoulderParent->WorldXfm().m);
        Hmx::Matrix3 handMat(mHand->WorldXfm().m);
        Vector3 handX(handMat.x);
        Vector3 handY(handMat.y);
        Vector3 handZ(handMat.z);
        float acosDot = acosf(Dot(elbowMat.x, handZ)) - kHalfPi;
        float absAcosDot;
        if (acosDot > 0.0f)
            absAcosDot = acosDot;
        else
            absAcosDot = -acosDot;
        float maxRads = mWristRadians;
        if (absAcosDot > maxRads) {
            if (acosDot > 0.0f)
                acosDot -= maxRads;
            else
                acosDot += maxRads;
            Hmx::Quat wristQuat;
            Transform wristXfm;
            Hmx::Matrix3 wristMat;
            wristQuat.Set(handY, acosDot);
            MakeRotMatrix(wristQuat, wristMat);
            Multiply(handX, wristMat, handX);
            Cross(handX, handY, handZ);
            wristXfm.m.Set(handX, handY, handZ);
            wristXfm.v = mHand->WorldXfm().v;
            mHand->SetWorldXfm(wristXfm);
            Vector3 newFingerPos(mFinger->WorldXfm().v);
            Subtract(newFingerPos, fingerSavedPos, newFingerPos);
            Subtract(wristXfm.v, newFingerPos, wristXfm.v);
            mHand->SetWorldXfm(wristXfm);
            mWorldDst = wristXfm.v;
            self->IKElbow(shoulderParent, elbowParent);
            mHand->SetWorldXfm(wristXfm);
        }
    }
}

// IKElbow: fuzzy 97.92, frame 0x280 == retail.  Every float expression below
// that is written out by hand (cosAngle, elbowAxisDot, elbowLen, IKDistance,
// `d`, the four quaternion products) is the same value as the math/Vec.h /
// math/Mtx.h helper it replaces; the helpers are canonicalised by MSVC in a
// different term order at these call sites, and the image's order is the one
// spelled here.  The quaternion block is Multiply(quatDir, quatRot) then
// Multiply(q, quatRot), and Multiply(quatRot, quatDir) then
// Multiply(quatRot, q), expanded in the image's association (no fneg; the
// `-(a - (b + c + d))` spelling in Mtx.h costs the frame 0x10 and a save).
// Remaining residual: the 16 products of the two quatDir*quatRot expansions
// are scheduled in a different order (register-only, ~60 rows), plus four
// commutative fadds/fmuls operand orders (swapping the source operands is
// inert).  sphereToAxisDist keeps Vec.h's Distance(): the image's z,x,y
// spelling there (as IKDistance) reschedules the quaternion block and nets
// lower (97.18 vs 97.92).
//
// (FileMerger.cpp unity-build-includes this file; that is deliberate, so the
// unit reads `default/FileMerger` in objdiff — not a mis-pin.)
// The distances in IKElbow subtract z, x, y and sum `dy*dy + (dx*dx + dz*dz)`;
// math/Vec.h's Distance() is emitted x, y, z at these call sites.
inline float IKDistance(const Vector3 &a, const Vector3 &b) {
    float dz = a.z - b.z;
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dy * dy + (dx * dx + dz * dz));
}

void CharIKHand::IKElbow(RndTransformable *elbow, RndTransformable *shoulder) {
    if (!elbow || !shoulder)
        return;
    Vector3 shoulderAdj;
    PullShoulder(shoulderAdj, shoulder->WorldXfm(), mWorldDst, mAAPlusBB);
    Transform shoulderXfm(shoulder->WorldXfm());
    shoulderXfm.v += shoulderAdj;
    shoulder->SetWorldXfm(shoulderXfm);
    Vector3 shoulderToWrist;
    Subtract(shoulder->WorldXfm().v, mWorldDst, shoulderToWrist);
    float cosAngle = mInv2ab
        * ((shoulderToWrist.y * shoulderToWrist.y
            + (shoulderToWrist.z * shoulderToWrist.z + shoulderToWrist.x * shoulderToWrist.x))
           - mAABB);
    ClampEq(cosAngle, -1.0f, 1.0f);
    float sinAngle = -std::sqrt(-(cosAngle * cosAngle - 1.0f));
    elbow->DirtyLocalXfm().m.Set(cosAngle, sinAngle, 0, -sinAngle, cosAngle, 0, 0, 0, 1);
    Vector3 handPos, targetPos;
    MultiplyTranspose(shoulder->WorldXfm(), mHand->WorldXfm().v, handPos);
    MultiplyTranspose(shoulder->WorldXfm(), mWorldDst, targetPos);
    if (mElbowSwing > 0) {
        Vector2 handYZ(handPos.y, handPos.z);
        Vector2 targetYZ(targetPos.y, targetPos.z);
        float handYZSq = handYZ.x * handYZ.x + handYZ.y * handYZ.y;
        float targetYZSq = targetYZ.x * targetYZ.x + targetYZ.y * targetYZ.y;
        float maxTargetYZ = Max<float>(targetYZSq, 16.0f);
        float maxHandYZ = Max<float>(handYZSq, 16.0f);
        float crossDenom = std::sqrt(maxHandYZ * maxTargetYZ);
        float crossVal = Cross(targetYZ, handYZ);
        float swingAngle = Clamp(-mElbowSwing, mElbowSwing, crossVal / crossDenom);
        Transform &dirtyElbow = elbow->DirtyLocalXfm();
        RotateAboutX(dirtyElbow.m, -swingAngle, dirtyElbow.m);
        MultiplyTranspose(shoulder->WorldXfm(), mHand->WorldXfm().v, handPos);
    }
    Hmx::Quat rotQuat;
    MakeRotQuat(handPos, targetPos, rotQuat);
    Hmx::Matrix3 rotMat;
    MakeRotMatrix(rotQuat, rotMat);
    Multiply(rotMat, shoulder->LocalXfm().m, shoulder->DirtyLocalXfm().m);
    if (mElbowCollide) {
        PullShoulder(shoulderAdj, shoulder->WorldXfm(), mWorldDst, mAAPlusBB);
        Transform shoulderXfm2(shoulder->WorldXfm());
        shoulderXfm2.v += shoulderAdj;
        shoulder->SetWorldXfm(shoulderXfm2);
        if (mElbowCollide->GetShape() != CharCollide::kCollideSphere)
            MILO_NOTIFY("%s: elbow collision object not sphere.\n", Name());
        else {
            Vector3 sphereCenter(mElbowCollide->WorldXfm().v);
            float sphereRadius = mElbowCollide->GetCurRadius();
            if (IKDistance(sphereCenter, elbow->WorldXfm().v) < sphereRadius) {
                Vector3 shoulderPos(shoulder->WorldXfm().v);
                shoulderPos -= mWorldDst;
                Vector3 unitAxis;
                Normalize(shoulderPos, unitAxis);
                Vector3 elbowToTarget;
                Subtract(elbow->WorldXfm().v, mWorldDst, elbowToTarget);
                Vector3 axisProj;
                float elbowAxisDot = elbowToTarget.z * unitAxis.z
                    + (elbowToTarget.y * unitAxis.y + elbowToTarget.x * unitAxis.x);
                Scale(unitAxis, elbowAxisDot, axisProj);
                Add(axisProj, mWorldDst, axisProj);
                Vector3 elbowDir(elbow->WorldXfm().v);
                elbowDir -= axisProj;
                float elbowLen = std::sqrt(
                    elbowDir.y * elbowDir.y + (elbowDir.z * elbowDir.z + elbowDir.x * elbowDir.x)
                );
                Vector3 axisDir(shoulder->WorldXfm().v);
                axisDir -= axisProj;
                Normalize(axisDir, axisDir);
                Vector3 midPt;
                Add(axisProj, axisDir, midPt);
                Vector3 sphereToMid;
                Subtract(axisProj, sphereCenter, sphereToMid);
                float midAxisDot = Dot(axisDir, sphereToMid);
                Scale(axisDir, midAxisDot, sphereToMid);
                Add(sphereCenter, sphereToMid, sphereToMid);
                float sDistToAxis = IKDistance(sphereToMid, sphereCenter);
                MILO_ASSERT(sDistToAxis <= sphereRadius, 0x1A1);
                float sPerpDist = std::sqrt(sphereRadius * sphereRadius - sDistToAxis * sDistToAxis);
                sphereCenter.Set(sphereToMid.x, sphereToMid.y, sphereToMid.z);
                float sphereToAxisDist = Distance(sphereCenter, axisProj);
                float d = (sphereToAxisDist * sphereToAxisDist + (sPerpDist * sPerpDist - elbowLen * elbowLen)) / (sphereToAxisDist * 2.0f);
                float sqrtTerm = std::sqrt(-(d * d - sPerpDist * sPerpDist));
                float tiltAngle = std::asin(sqrtTerm / elbowLen);
                if (IsNaN(tiltAngle))
                    return;
                Vector3 tiltDir(sphereCenter);
                tiltDir -= axisProj;
                Normalize(tiltDir, tiltDir);
                Scale(tiltDir, elbowLen, tiltDir);
                float sinHalf = sin(tiltAngle / 2.0);
                float cosHalf = cos(tiltAngle / 2.0);
                Hmx::Quat quatDir(tiltDir.x, tiltDir.y, tiltDir.z, 0.0f);
                Hmx::Quat quatRot(axisDir.x * sinHalf, axisDir.y * sinHalf, axisDir.z * sinHalf, cosHalf);
                Hmx::Quat quatResult;
                const Hmx::Quat &qd = quatDir;
                const Hmx::Quat &qr = quatRot;
                Hmx::Quat &q = quatResult;
                q.Set(
                    ((qr.z * qd.y + qr.w * qd.x) + qr.x * qd.w) - qr.y * qd.z,
                    ((qr.w * qd.y + qr.y * qd.w) + qr.x * qd.z) - qr.z * qd.x,
                    ((qr.w * qd.z + qr.z * qd.w) + qr.y * qd.x) - qr.x * qd.y,
                    ((qr.w * qd.w - qr.x * qd.x) - qr.y * qd.y) - qr.z * qd.z
                );
                q.Set(
                    ((qr.w * q.x + q.w * qr.x) + qr.z * q.y) - qr.y * q.z,
                    ((q.w * qr.y + qr.w * q.y) + qr.x * q.z) - qr.z * q.x,
                    ((qr.w * q.z + qr.y * q.x) + q.w * qr.z) - qr.x * q.y,
                    ((q.w * qr.w - q.x * qr.x) - q.y * qr.y) - q.z * qr.z
                );
                Vector3 v1(quatResult.x, quatResult.y, quatResult.z);
                Add(v1, axisProj, v1);
                q.Set(
                    ((qr.y * qd.z + qr.w * qd.x) + qr.x * qd.w) - qr.z * qd.y,
                    ((qr.z * qd.x + qr.w * qd.y) + qr.y * qd.w) - qr.x * qd.z,
                    ((qr.x * qd.y + qr.w * qd.z) + qr.z * qd.w) - qr.y * qd.x,
                    (-(qr.x * qd.x) + qr.w * qd.w) - qr.y * qd.y - qr.z * qd.z
                );
                q.Set(
                    ((qr.w * q.x + q.w * qr.x) + qr.y * q.z) - qr.z * q.y,
                    ((qr.w * q.y + qr.z * q.x) + q.w * qr.y) - qr.x * q.z,
                    ((q.w * qr.z + qr.w * q.z) + qr.x * q.y) - qr.y * q.x,
                    ((qr.w * q.w - qr.x * q.x) - qr.y * q.y) - qr.z * q.z
                );
                Vector3 v2(quatResult.x, quatResult.y, quatResult.z);
                Add(v2, axisProj, v2);
                Vector3 elbowLocal, targetLocal;
                MultiplyTranspose(shoulder->WorldXfm(), elbow->WorldXfm().v, elbowLocal);
                if (mClockwise)
                    MultiplyTranspose(shoulder->WorldXfm(), v2, targetLocal);
                else
                    MultiplyTranspose(shoulder->WorldXfm(), v1, targetLocal);
                Hmx::Quat finalQuat;
                MakeRotQuat(elbowLocal, targetLocal, finalQuat);
                Hmx::Matrix3 finalMat;
                MakeRotMatrix(finalQuat, finalMat);
                Multiply(finalMat, shoulder->LocalXfm().m, shoulder->DirtyLocalXfm().m);
                MultiplyTranspose(elbow->WorldXfm(), mHand->WorldXfm().v, elbowLocal);
                MultiplyTranspose(elbow->WorldXfm(), mWorldDst, targetLocal);
                MakeRotQuat(elbowLocal, targetLocal, finalQuat);
                MakeRotMatrix(finalQuat, finalMat);
                Multiply(finalMat, elbow->LocalXfm().m, elbow->DirtyLocalXfm().m);
            }
        }
    }
    PullShoulder(shoulderAdj, shoulder->WorldXfm(), mWorldDst, mAAPlusBB);
    shoulderXfm = shoulder->WorldXfm();
    shoulderXfm.v += shoulderAdj;
    shoulder->SetWorldXfm(shoulderXfm);
}

void CharIKHand::Highlight() {
    float charWeight = Weight();
    float leftover = 0;
    float localWeights[16];

    auto& hand = mHand;
    if (charWeight == 0 || !hand || mTargets.empty())
        return;
    else {
        if (mTargets.size() != 1) {
            float *fp = &localWeights[0];
            for (ObjVector<IKTarget>::iterator it = mTargets.begin();
                 it != mTargets.end();
                 ++it, fp++) {
                RndTransformable *curTarget = it->mTarget;
                if (curTarget) {
                    float w = 144.0f / LengthSquared(curTarget->LocalXfm().v);
                    *fp = w;
                    leftover += w;
                }
            }
            float unusedWeight = 0;
            if (leftover < 1.0f) {
                unusedWeight = charWeight * (1.0f - leftover);
                charWeight -= unusedWeight;
            }
            TheRnd.DrawString(
                MakeString("weight %g", charWeight),
                Vector2(100.0f, 100.0f),
                Hmx::Color(1, 1, 1),
                true
            );
            TheRnd.DrawString(
                MakeString("leftover %g", unusedWeight),
                Vector2(100.0f, 114.0f),
                Hmx::Color(1, 1, 1),
                true
            );
            fp = &localWeights[0];
            int idx = 0;
            for (ObjVector<IKTarget>::iterator it = mTargets.begin();
                 it != mTargets.end();
                 ++it, fp++, idx++) {
                float w = *fp;
                float normalized = w / leftover;
                if (it->mTarget) {
                    const Transform &curWorld = it->mTarget->WorldXfm();
                    TheRnd.DrawString(
                        MakeString("%s %g", it->mTarget->Name(), charWeight * normalized),
                        Vector2(100.0f, (idx + 2) * 14.0f + 100.0f),
                        Hmx::Color(1, 1, 1),
                        true
                    );
                    UtilDrawAxes(curWorld, 1.0f, Hmx::Color(1, 1, 1));
                    UtilDrawSphere(curWorld.v, normalized, Hmx::Color(1, 0, 0));
                    TheRnd.DrawLine(
                        curWorld.v,
                        it->mTarget->TransParent()->WorldXfm().v,
                        Hmx::Color(1, 0, 0),
                        false
                    );
                }
            }
        }
        UtilDrawAxes(hand->WorldXfm(), 1.0f, Hmx::Color(1, 1, 1));
        UtilDrawSphere(hand->WorldXfm().v, 1.0f, Hmx::Color(0, 1, 0));
    }
}

#pragma endregion CharIKHand
#pragma region CharIKHand::IKTarget

CharIKHand::IKTarget::IKTarget(Hmx::Object *owner) : mTarget(owner), mExtent(0) {}

CharIKHand::IKTarget::IKTarget(ObjPtr<RndTransformable> t, float e)
    : mTarget(t), mExtent(e) {}

BinStream &operator>>(BinStream &bs, CharIKHand::IKTarget &t) {
    bs >> t.mTarget;
    bs >> t.mExtent;
    return bs;
}

#pragma endregion CharIKHand::IKTarget
