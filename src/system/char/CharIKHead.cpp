#include "char/CharIKHead.h"
#include "char/Character.h"
#include "char/CharWeightable.h"
#include "math/Vec.h"
#include "math/Rot.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

void NormalizeScale(const Vector3 &, float, Vector3 &);

#pragma region CharIKHead

CharIKHead::CharIKHead()
    : mPoints(this), mHead(this), mSpine(this), mMouth(this), mTarget(this),
      mHeadFilter(0, 0, 0), mTargetRadius(0.75), mHeadMat(0.5), mOffset(this),
      mOffsetScale(1, 1, 1), mUpdatePoints(1) {}

CharIKHead::~CharIKHead() {}

void CharIKHead::Poll() {
    if (!mHead || !mTarget || !mSpine)
        return;
    UpdatePoints(false);
    Character *me = Character::Current();
    if (me && me->Teleported()) {
        mHeadFilter = mHead->WorldXfm().v;
    } else if (TheTaskMgr.DeltaSeconds() > 0) {
        Interp(mHeadFilter, mHead->WorldXfm().v, 0.5f, mHeadFilter);
    }
    float weight = Weight();
    if (weight != 0) {
        Hmx::Matrix3 origHeadMat(mHead->WorldXfm().m);
        Vector3 headOffset;
        Subtract(mHead->WorldXfm().v, mHeadFilter, headOffset);
        Vector3 targetPos = mTarget->WorldXfm().v;
        float headOffsetLen = Length(headOffset);
        if (headOffsetLen > 0) {
            float blendDist = Min(headOffsetLen, mTargetRadius * weight);
            ScaleAddEq(targetPos, headOffset, blendDist / headOffsetLen);
        }
        mDebugTarget = targetPos;
        Interp(mPoints[0].mBone->WorldXfm().v, targetPos, weight, targetPos);
        Vector3 spineToTarget;
        Subtract(targetPos, mSpine->TransParent()->WorldXfm().v, spineToTarget);
        float distSq = LengthSquared(spineToTarget);
        if (distSq > mSpineLength * mSpineLength) {
            ScaleAdd(
                mSpine->TransParent()->WorldXfm().v,
                spineToTarget,
                mSpineLength / sqrtf(distSq),
                targetPos
            );
        }
        Subtract(targetPos, mPoints[0].mBone->WorldXfm().v, spineToTarget);
        for (int i = 0; i < mPoints.size(); i++) {
            Point &pt = mPoints[i];
            ScaleAdd(pt.mBone->WorldXfm().v, spineToTarget, pt.mLenRatio, pt.mPos);
            pt.mWorldPos = pt.mPos;
        }
        Vector3 correction(0, 0, 0);
        for (int i = 1; i < mPoints.size(); i++) {
            mPoints[i].mPos += correction;
            Vector3 diff;
            Subtract(mPoints[i].mPos, mPoints[i - 1].mPos, diff);
            correction -= diff;
            NormalizeScale(diff, mPoints[i - 1].mLen, diff);
            correction += diff;
            Add(mPoints[i - 1].mPos, diff, mPoints[i].mPos);
        }
        for (int i = 1; i < mPoints.size(); i++) {
            ScaleAddEq(mPoints[i].mPos, correction, mPoints[i].mLenRatio - 1.0f);
        }
        for (int i = mPoints.size() - 1; i >= 0; i--) {
            Transform boneXfm(mPoints[i].mBone->WorldXfm().m, mPoints[i].mPos);
            if (i > 0) {
                Vector3 localVec;
                Hmx::Quat rotQuat;
                Multiply(mPoints[i - 1].mBone->LocalXfm().v, boneXfm.m, localVec);
                Vector3 targetVec;
                Subtract(mPoints[i - 1].mPos, mPoints[i].mPos, targetVec);
                MakeRotQuat(localVec, targetVec, rotQuat);
                Hmx::Matrix3 rotMat;
                MakeRotMatrix(rotQuat, rotMat);
                Multiply(boneXfm.m, rotMat, boneXfm.m);
            } else {
                Interp(boneXfm.m, origHeadMat, mHeadMat, boneXfm.m);
            }
            mPoints[i].mBone->SetWorldXfm(boneXfm);
        }
        if (mOffset) {
            Transform offsetXfm(mOffset->WorldXfm());
            spineToTarget *= mOffsetScale;
            offsetXfm.v += spineToTarget;
            mOffset->SetWorldXfm(offsetXfm);
        }
    }
}

BEGIN_HANDLERS(CharIKHead)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharIKHead)
    SYNC_PROP_MODIFY(head, mHead, UpdatePoints(true))
    SYNC_PROP_MODIFY(spine, mSpine, UpdatePoints(true))
    SYNC_PROP(mouth, mMouth)
    SYNC_PROP(target, mTarget)
    SYNC_PROP(target_radius, mTargetRadius)
    SYNC_PROP(head_mat, mHeadMat)
    SYNC_PROP(offset, mOffset)
    SYNC_PROP(offset_scale, mOffsetScale)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

INIT_REVS(3, 0)

BEGIN_LOADS(CharIKHead)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mHead;
    bs >> mSpine;
    bs >> mMouth;
    bs >> mTarget;
    if (d.rev > 1) {
        bs >> mTargetRadius;
        bs >> mHeadMat;
    }
    if (d.rev > 2) {
        bs >> mOffset;
        bs >> mOffsetScale;
    }
    mUpdatePoints = true;
END_LOADS

BEGIN_SAVES(CharIKHead)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mHead;
    bs << mSpine;
    bs << mMouth;
    bs << mTarget;
    bs << mTargetRadius;
    bs << mHeadMat;
    bs << mOffset;
    bs << mOffsetScale;
END_SAVES

BEGIN_COPYS(CharIKHead)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharIKHead)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mHead)
        COPY_MEMBER(mSpine)
        COPY_MEMBER(mMouth)
        COPY_MEMBER(mTarget)
        COPY_MEMBER(mTargetRadius)
        COPY_MEMBER(mHeadMat)
        COPY_MEMBER(mOffset)
        COPY_MEMBER(mOffsetScale)
        mUpdatePoints = true;
    END_COPYING_MEMBERS
END_COPYS

void CharIKHead::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mMouth);
    changedBy.push_back(mHead);
    changedBy.push_back(mTarget);
    if (GenerationCount(mSpine, mHead) != 0) {
#ifdef HX_NATIVE
        for (RndTransformable *t = mHead; t != nullptr && t != mSpine->TransParent();
#else
        for (RndTransformable *t = mHead; t > 0 && t != mSpine->TransParent();
#endif
             t = t->TransParent()) {
            change.push_back(t);
        }
    }
    change.push_back(mOffset);
}

void CharIKHead::UpdatePoints(bool b) {
    if (b || mUpdatePoints) {
        mUpdatePoints = false;
        mPoints.clear();
        int gencnt = GenerationCount(mSpine, mHead);
        if (gencnt != 0) {
            mPoints.resize(gencnt + 1);
            float f1 = 0.0f;
            int i;
            RndTransformable *curtrans = mHead;
            for (i = 0; i < mPoints.size(); i++) {
                Point &pt = mPoints[i];
                pt.mBone = curtrans;
                pt.mLen = Length(curtrans->LocalXfm().v);
                curtrans = curtrans->TransParent();
                f1 += pt.mLen;
            }
            mSpineLength = f1;
            float f2 = 1.0f / f1;
            for (int i = 0; i < mPoints.size(); i++) {
                Point &curPt = mPoints[i];
                curPt.mLenRatio = f1 * f2;
                f1 = f1 - mPoints[i].mLen;
            }
        }
    }
}

void CharIKHead::Highlight() {
    float weight = Weight();
    if (weight == 0 || !mHead || !mTarget || !mSpine)
        return;
    UtilDrawSphere(mDebugTarget, mTargetRadius, Hmx::Color(0, 1, 0), 0);
    UtilDrawString(MakeString("%.2f", weight), mDebugTarget, Hmx::Color(1, 1, 1));
    for (int i = 1; i < mPoints.size(); i++) {
        TheRnd.DrawLine(
            mPoints[i].mWorldPos, mPoints[i - 1].mWorldPos, Hmx::Color(1, 0, 0), false
        );
        TheRnd.DrawLine(
            mPoints[i].mPos, mPoints[i - 1].mPos, Hmx::Color(0, 1, 0), false
        );
    }
}

#pragma endregion CharIKHead
#pragma region CharIKHead::Point

CharIKHead::Point::Point(Hmx::Object *owner)
    : mBone(owner), mPos(0, 0, 0), mLen(0), mLenRatio(0) {}

CharIKHead::Point::Point(CharIKHead::Point const &point)
    : mBone(point.mBone), mPos(point.mPos), mLen(point.mLen), mLenRatio(point.mLenRatio) {}

#pragma endregion CharIKHead::Point
