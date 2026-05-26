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

CharIKHand::CharIKHand()
    : mHand(this), mFinger(this), mTargets(this), mOrientation(true), mStretch(true),
      mScalable(false), mMoveElbow(true), mElbowSwing(0), mAlwaysIKElbow(false),
      mPullShoulder(true), mAAPlusBB(0), mConstraintWrist(false), mWristRadians(0),
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
    SYNC_PROP(constraint_wrist, mConstraintWrist)
    SYNC_PROP(wrist_radians, mWristRadians)
    SYNC_PROP(elbow_collide, mElbowCollide)
    SYNC_PROP(clockwise, mClockwise)
    SYNC_PROP(pull_shoulder, mPullShoulder)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const CharIKHand::IKTarget &t) {
    bs << t.mTarget;
    bs << t.mExtent;
    return bs;
}

BinStream &operator<<(BinStream &bs, const CharBlendBone::ConstraintSystem &cs) {
    bs << cs.mTarget;
    bs << cs.mWeight;
    return bs;
}

BEGIN_SAVES(CharIKHand)
    SAVE_REVS(0xD, 0)
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
    bs << mPullShoulder;
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
        COPY_MEMBER(mPullShoulder)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0xC, 0)

BEGIN_LOADS(CharIKHand)
    LOAD_REVS(bs)
    ASSERT_REVS(0xD, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mHand;
    if (d.rev > 4)
        bs >> mFinger;
    else
        mFinger = 0;
    if (d.rev < 3) {
        ObjPtr<RndTransformable> tPtr(this, 0);
        bs >> tPtr;
        mTargets.clear();
        mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
    } else if (d.rev < 0xB) {
        ObjPtrList<RndTransformable> tList(this, kObjListNoNull);
        bs >> tList;
        mTargets.clear();
        for (ObjPtrList<RndTransformable>::iterator it = tList.begin(); it != tList.end();
             ++it) {
            ObjPtr<RndTransformable> tPtr(this, *it);
            mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
        }
    } else
        d >> mTargets;

    d >> mOrientation;
    d >> mStretch;
    if (d.rev > 1)
        d >> mScalable;
    else
        mScalable = false;

    if (d.rev > 3)
        d >> mMoveElbow;
    else
        mMoveElbow = true;

    if (d.rev > 5)
        bs >> mElbowSwing;
    else
        mElbowSwing = 0.0f;

    if (d.rev > 6)
        d >> mAlwaysIKElbow;
    if (d.rev > 7) {
        d >> mConstraintWrist;
        d >> mWristRadians;
    }
    if (d.rev == 9) {
        String s;
        d >> s;
        bool b;
        d >> b;
    }
    if (d.rev > 0xB) {
        d >> mElbowCollide;
        d >> mClockwise;
    }
    if (d.rev > 0xc)
        d >> mPullShoulder;
    SetHand(mHand);
END_LOADS

void CharIKHand::SetHand(RndTransformable *t) {
    mHand = t;
    mHandChanged = true;
}

void CharIKHand::PullShoulder(
    Vector3 &v, const Transform &tf, const Vector3 &vconst, float fff
) {
    if (mPullShoulder) {
        Subtract(vconst, tf.v, v);
        float f2 = fff * 0.95f;
        float lensq = LengthSquared(v);
        if (lensq > f2 * f2) {
            v *= 1.0f - f2 / (float)std::sqrt(lensq);
            return;
        }
    }
    v.x = 0;
    v.y = 0;
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
    static const float kMinWeight = 0.001f;

    static const float kMaxWeight = 144.0f;
    RndTransformable *hand = mHand;
    if (!hand || mTargets.empty())
        return;
    Vector3 destPos(0.0f, 0.0f, 0.0f);
    Hmx::Quat destQuat(0.0f, 0.0f, 0.0f, 0.0f);
    if (mScalable || mHandChanged) {
        MeasureLengths();
        mHandChanged = false;
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
        float localWeights[16];
        float *weightPtr = localWeights;
        auto endIt = mTargets.end();
        for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != endIt;
             ++it) {
            RndTransformable *targetTrans = it->mTarget;
            float extent = it->mExtent;
            if (targetTrans) {
                Vector3 targetVec(targetTrans->WorldXfm().v);
                if (extent <= 0.0f) {
                    *weightPtr = kMaxWeight / Max(kMinWeight, LengthSquared(targetVec));
                } else if (extent < -targetVec.z) {
                    *weightPtr = kMinWeight;
                } else {
                    targetVec.z = 0.0f;
                    *weightPtr = kMaxWeight / Max(kMinWeight, LengthSquared(targetVec));
                }
                totalWeight += *weightPtr++;
            }
        }
        if (totalWeight < 1.0f) {
            charWeight = charWeight - (charWeight * (1.0f - totalWeight));
        }
        weightPtr = localWeights;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
             ++it) {
            RndTransformable *targetTrans = it->mTarget;
            if (targetTrans) {
                float curWeight = *weightPtr;
                const Transform &worldXfm = targetTrans->WorldXfm();
                ScaleAdd(destPos, worldXfm.v, curWeight / totalWeight, destPos);
                if (mOrientation) {
                    Hmx::Matrix3 normMat;
                    Normalize(worldXfm.m, normMat);
                    Hmx::Quat q(normMat);
                    ScaleAddEq(destQuat, q, curWeight / totalWeight);
                }
            }
            weightPtr++;
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
        IKElbow(shoulderParent, elbowParent);
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
        float absAcosDot = acosDot;
        if (acosDot <= 0.0f)
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
            IKElbow(shoulderParent, elbowParent);
            mHand->SetWorldXfm(wristXfm);
        }
    }
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
    float cosAngle = mInv2ab * (LengthSquared(shoulderToWrist) - mAABB);
    ClampEq(cosAngle, -1.0f, 1.0f);
    float sinAngle = -std::sqrt(-(cosAngle * cosAngle - 1.0f));
    elbow->DirtyLocalXfm().m.Set(cosAngle, sinAngle, 0, -sinAngle, cosAngle, 0, 0, 0, 1);
    Vector3 handPos, targetPos;
    MultiplyTranspose(mHand->WorldXfm().v, shoulder->WorldXfm(), handPos);
    MultiplyTranspose(mWorldDst, shoulder->WorldXfm(), targetPos);
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
        MultiplyTranspose(mHand->WorldXfm().v, shoulder->WorldXfm(), handPos);
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
            if (Distance(sphereCenter, elbow->WorldXfm().v) < sphereRadius) {
                Vector3 shoulderPos(shoulder->WorldXfm().v);
                shoulderPos -= mWorldDst;
                Vector3 unitAxis;
                Normalize(shoulderPos, unitAxis);
                Vector3 elbowToTarget;
                Subtract(elbow->WorldXfm().v, mWorldDst, elbowToTarget);
                Vector3 axisProj;
                float elbowAxisDot = Dot(elbowToTarget, unitAxis);
                Scale(unitAxis, elbowAxisDot, axisProj);
                Add(axisProj, mWorldDst, axisProj);
                Vector3 elbowDir(elbow->WorldXfm().v);
                elbowDir -= axisProj;
                float elbowLen = Length(elbowDir);
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
                float sDistToAxis = Distance(sphereToMid, sphereCenter);
                MILO_ASSERT(sDistToAxis <= sphereRadius, 0x1A1);
                float sPerpDist = std::sqrt(sphereRadius * sphereRadius - sDistToAxis * sDistToAxis);
                sphereCenter.Set(sphereToMid.x, sphereToMid.y, sphereToMid.z);
                float sphereToAxisDist = Distance(sphereCenter, axisProj);
                float d = (sphereToAxisDist * sphereToAxisDist + -(sDistToAxis * sDistToAxis - sPerpDist * sPerpDist)) / (sphereToAxisDist * 2.0f);
                float sqrtTerm = std::sqrt(-(d * d - sPerpDist * sPerpDist));
                float tiltAngle = std::asin(sqrtTerm / elbowLen);
                if (IsNaN(tiltAngle))
                    return;
                Vector3 tiltDir(sphereCenter);
                tiltDir -= axisProj;
                Normalize(tiltDir, tiltDir);
                Scale(tiltDir, elbowLen, tiltDir);
                double halfAngle = tiltAngle / 2.0;
                float sinHalf = sin(halfAngle);
                float cosHalf = cos(halfAngle);
                Hmx::Quat quatDir(tiltDir.x, tiltDir.y, tiltDir.z, 0.0f);
                Hmx::Quat quatRot(axisDir.x * sinHalf, axisDir.y * sinHalf, axisDir.z * sinHalf, cosHalf);
                Hmx::Quat quatResult;
                Multiply(quatDir, quatRot, quatResult);
                Multiply(quatResult, quatRot, quatResult);
                Vector3 v1(quatResult.x, quatResult.y, quatResult.z);
                Add(v1, axisProj, v1);
                Multiply(quatRot, quatDir, quatResult);
                Multiply(quatRot, quatResult, quatResult);
                Vector3 v2(quatResult.x, quatResult.y, quatResult.z);
                Add(v2, axisProj, v2);
                Vector3 elbowLocal, targetLocal;
                MultiplyTranspose(elbow->WorldXfm().v, shoulder->WorldXfm(), elbowLocal);
                if (mClockwise)
                    MultiplyTranspose(v2, shoulder->WorldXfm(), targetLocal);
                else
                    MultiplyTranspose(v1, shoulder->WorldXfm(), targetLocal);
                Hmx::Quat finalQuat;
                MakeRotQuat(elbowLocal, targetLocal, finalQuat);
                Hmx::Matrix3 finalMat;
                MakeRotMatrix(finalQuat, finalMat);
                Multiply(finalMat, shoulder->LocalXfm().m, shoulder->DirtyLocalXfm().m);
                MultiplyTranspose(mHand->WorldXfm().v, elbow->WorldXfm(), elbowLocal);
                MultiplyTranspose(mWorldDst, elbow->WorldXfm(), targetLocal);
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
                    UtilDrawSphere(curWorld.v, normalized, Hmx::Color(1, 0, 0), nullptr);
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
        UtilDrawSphere(hand->WorldXfm().v, 1.0f, Hmx::Color(0, 1, 0), nullptr);
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
