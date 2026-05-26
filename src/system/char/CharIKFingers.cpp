#include "char/CharIKFingers.h"
#include "char/CharWeightable.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"

CharIKFingers::CharIKFingers()
    : mHand(nullptr), mForeArm(nullptr), mUpperArm(nullptr), mBlendInFrames(0),
      mBlendOutFrames(0), mResetHandDest(1), mResetCurHandTrans(1),
      mFingerCurledLength(0.85), mHandMoveForward(1), mHandPinkyRotation(-0.06),
      mHandThumbRotation(0.23), mHandDestOffset(-0.4), mIsRightHand(1), mMoveHand(0),
      mIsSetup(0), mOutputTrans(this), mKeyboardRefBone(this) {
    mFingers.resize(5);
    mCurHandTrans.Zero();
    mDestHandTrans.Zero();
    mHandKeyboardOffset = Vector3(0.3f, -6.0f, 0.4f);
    mtx = Hmx::Matrix3();
}

CharIKFingers::~CharIKFingers() {}

BEGIN_HANDLERS(CharIKFingers)
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharIKFingers)
    SYNC_PROP(is_right_hand, mIsRightHand)
    SYNC_PROP(output_trans, mOutputTrans)
    SYNC_PROP(keyboard_ref_bone, mKeyboardRefBone)
    SYNC_PROP(hand_keyboard_offset, mHandKeyboardOffset)
    SYNC_PROP(hand_thumb_rotation, mHandThumbRotation)
    SYNC_PROP(hand_pinky_rotation, mHandPinkyRotation)
    SYNC_PROP(hand_move_forward, mHandMoveForward)
    SYNC_PROP(hand_dest_offset, mHandDestOffset)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharIKFingers)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mIsRightHand;
    bs << mOutputTrans;
    bs << mKeyboardRefBone;
    bs << mHandKeyboardOffset;
    bs << mHandThumbRotation;
    bs << mHandPinkyRotation;
    bs << mHandMoveForward;
    bs << mHandDestOffset;
END_SAVES

BEGIN_COPYS(CharIKFingers)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharIKFingers)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mIsRightHand)
        COPY_MEMBER(mOutputTrans)
        COPY_MEMBER(mKeyboardRefBone)
        COPY_MEMBER(mHandKeyboardOffset)
        COPY_MEMBER(mHandThumbRotation)
        COPY_MEMBER(mHandPinkyRotation)
        COPY_MEMBER(mHandMoveForward)
        COPY_MEMBER(mHandDestOffset)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(5, 0)

BEGIN_LOADS(CharIKFingers)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    if (d.rev > 1)
        d >> mIsRightHand;
    if (d.rev > 2)
        bs >> mOutputTrans;
    if (d.rev > 3)
        bs >> mKeyboardRefBone;
    if (d.rev > 4) {
        bs >> mHandKeyboardOffset;
        bs >> mHandThumbRotation;
        bs >> mHandPinkyRotation;
        bs >> mHandMoveForward;
        bs >> mHandDestOffset;
    }
END_LOADS

void CharIKFingers::SetName(const char *name, ObjectDir *dir) {
    Hmx::Object::SetName(name, dir);
    if (dir) {
        for (int i = 0; i < 5; i++) {
            mFingers[i] = FingerDesc();
        }
        if (mIsRightHand) {
            mHand = dir->Find<RndTransformable>("bone_R-hand.mesh", false);
            mForeArm = dir->Find<RndTransformable>("bone_R-foreArm.mesh", false);
            mUpperArm = dir->Find<RndTransformable>("bone_R-upperArm.mesh", false);
            mFingers[kFingerThumb].mFinger01 =
                dir->Find<RndTransformable>("bone_R-thumb01.mesh", false);
            mFingers[kFingerThumb].mFinger02 =
                dir->Find<RndTransformable>("bone_R-thumb02.mesh", false);
            mFingers[kFingerThumb].mFinger03 =
                dir->Find<RndTransformable>("bone_R-thumb03.mesh", false);
            mFingers[kFingerThumb].mFingertip =
                dir->Find<RndTransformable>("spot_R-thumb_tip.mesh", false);
            mFingers[kFingerIndex].mFinger01 =
                dir->Find<RndTransformable>("bone_R-index01.mesh", false);
            mFingers[kFingerIndex].mFinger02 =
                dir->Find<RndTransformable>("bone_R-index02.mesh", false);
            mFingers[kFingerIndex].mFinger03 =
                dir->Find<RndTransformable>("bone_R-index03.mesh", false);
            mFingers[kFingerIndex].mFingertip =
                dir->Find<RndTransformable>("spot_R-index_tip.mesh", false);
            mFingers[kFingerMiddle].mFinger01 =
                dir->Find<RndTransformable>("bone_R-middlefinger01.mesh", false);
            mFingers[kFingerMiddle].mFinger02 =
                dir->Find<RndTransformable>("bone_R-middlefinger02.mesh", false);
            mFingers[kFingerMiddle].mFinger03 =
                dir->Find<RndTransformable>("bone_R-middlefinger03.mesh", false);
            mFingers[kFingerMiddle].mFingertip =
                dir->Find<RndTransformable>("spot_R-middlefinger_tip.mesh", false);
            mFingers[kFingerRing].mFinger01 =
                dir->Find<RndTransformable>("bone_R-ringfinger01.mesh", false);
            mFingers[kFingerRing].mFinger02 =
                dir->Find<RndTransformable>("bone_R-ringfinger02.mesh", false);
            mFingers[kFingerRing].mFinger03 =
                dir->Find<RndTransformable>("bone_R-ringfinger03.mesh", false);
            mFingers[kFingerRing].mFingertip =
                dir->Find<RndTransformable>("spot_R-ringfinger_tip.mesh", false);
            mFingers[kFingerPinky].mFinger01 =
                dir->Find<RndTransformable>("bone_R-pinky01.mesh", false);
            mFingers[kFingerPinky].mFinger02 =
                dir->Find<RndTransformable>("bone_R-pinky02.mesh", false);
            mFingers[kFingerPinky].mFinger03 =
                dir->Find<RndTransformable>("bone_R-pinky03.mesh", false);
            mFingers[kFingerPinky].mFingertip =
                dir->Find<RndTransformable>("spot_R-pinky_tip.mesh", false);
            mtx = Hmx::Matrix3(
                -0.023f,
                0.97899997f,
                0.201f,
                -0.228f,
                0.191f,
                -0.95499998f,
                -0.972,
                -0.068f,
                0.21799999f
            );
            Normalize(mtx, mtx);
            mIsSetup = true;
        } else {
            mHand = dir->Find<RndTransformable>("bone_L-hand.mesh", false);
            mForeArm = dir->Find<RndTransformable>("bone_L-foreArm.mesh", false);
            mUpperArm = dir->Find<RndTransformable>("bone_L-upperArm.mesh", false);
            mFingers[kFingerThumb].mFinger01 =
                dir->Find<RndTransformable>("bone_L-thumb01.mesh", false);
            mFingers[kFingerThumb].mFinger02 =
                dir->Find<RndTransformable>("bone_L-thumb02.mesh", false);
            mFingers[kFingerThumb].mFinger03 =
                dir->Find<RndTransformable>("bone_L-thumb03.mesh", false);
            mFingers[kFingerThumb].mFingertip =
                dir->Find<RndTransformable>("spot_L-thumb_tip.mesh", false);
            mFingers[kFingerIndex].mFinger01 =
                dir->Find<RndTransformable>("bone_L-index01.mesh", false);
            mFingers[kFingerIndex].mFinger02 =
                dir->Find<RndTransformable>("bone_L-index02.mesh", false);
            mFingers[kFingerIndex].mFinger03 =
                dir->Find<RndTransformable>("bone_L-index03.mesh", false);
            mFingers[kFingerIndex].mFingertip =
                dir->Find<RndTransformable>("spot_L-index_tip.mesh", false);
            mFingers[kFingerMiddle].mFinger01 =
                dir->Find<RndTransformable>("bone_L-middlefinger01.mesh", false);
            mFingers[kFingerMiddle].mFinger02 =
                dir->Find<RndTransformable>("bone_L-middlefinger02.mesh", false);
            mFingers[kFingerMiddle].mFinger03 =
                dir->Find<RndTransformable>("bone_L-middlefinger03.mesh", false);
            mFingers[kFingerMiddle].mFingertip =
                dir->Find<RndTransformable>("spot_L-middlefinger_tip.mesh", false);
            mFingers[kFingerRing].mFinger01 =
                dir->Find<RndTransformable>("bone_L-ringfinger01.mesh", false);
            mFingers[kFingerRing].mFinger02 =
                dir->Find<RndTransformable>("bone_L-ringfinger02.mesh", false);
            mFingers[kFingerRing].mFinger03 =
                dir->Find<RndTransformable>("bone_L-ringfinger03.mesh", false);
            mFingers[kFingerRing].mFingertip =
                dir->Find<RndTransformable>("spot_L-ringfinger_tip.mesh", false);
            mFingers[kFingerPinky].mFinger01 =
                dir->Find<RndTransformable>("bone_L-pinky01.mesh", false);
            mFingers[kFingerPinky].mFinger02 =
                dir->Find<RndTransformable>("bone_L-pinky02.mesh", false);
            mFingers[kFingerPinky].mFinger03 =
                dir->Find<RndTransformable>("bone_L-pinky03.mesh", false);
            mFingers[kFingerPinky].mFingertip =
                dir->Find<RndTransformable>("spot_L-pinky_tip.mesh", false);
            mtx = Hmx::Matrix3(
                -0.067f,
                0.985f,
                0.156f,
                0.224f,
                0.167f,
                -0.95999998f,
                -0.972f,
                -0.028999999f,
                -0.23199999f
            );
            Normalize(mtx, mtx);
            mIsSetup = true;
        }
        for (int i = 0; i < 5; i++) {
            FingerDesc cur = mFingers[i];
            if (!cur.mFinger01 || !cur.mFinger02 || !cur.mFinger03 || !cur.mFingertip) {
                mIsSetup = false;
                break;
            }
        }
    }
    MeasureLengths();
}

void CharIKFingers::CalculateHandDest(int engagedCount, int firstEngaged) {
    auto _tmp0 = mHand->WorldXfm();
    Transform curHandXfm(_tmp0);
    auto& _ref0 = mMoveHand;
    if (_ref0) {
        if (engagedCount > 0) {
            Vector3 destOffset(0, 0, 0);
            FingerDesc firstDesc = mFingers[firstEngaged];
            Vector3 avgPos;
            avgPos.Zero();
            bool hasSpecialRotation = false;
            Vector3 sideOffsetBase(mHandDestOffset, 0, 0);
            if (!mIsRightHand) {
                Scale(sideOffsetBase, -1.0f, sideOffsetBase);
            }
            Multiply(sideOffsetBase, mKeyboardRefBone->WorldXfm().m, sideOffsetBase);
            Hmx::Matrix3 refRotMat;
            Multiply(mtx, mKeyboardRefBone->WorldXfm().m, refRotMat);
            Normalize(refRotMat, mDestHandTrans.m);
            for (int i = 0; i < 5; i++) {
                FingerDesc &finger = mFingers[i];
                if (finger.mIsEngaged) {
                    Add(finger.mTargetWorldPos, avgPos, avgPos);
                    Vector3 sideScaled;
                    Scale(sideOffsetBase, i - 2.0f, sideScaled);
                    Add(sideScaled, avgPos, avgPos);
                    if (i == 0) {
                        Hmx::Matrix3 thumbRotMat;
                        float thumbRot = mHandThumbRotation;
                        if (!mIsRightHand)
                            thumbRot *= -1.0f;
                        thumbRotMat.RotateAboutY(thumbRot);
                        Multiply(thumbRotMat, refRotMat, mDestHandTrans.m);
                        hasSpecialRotation = true;
                    } else if (i == 4) {
                        Hmx::Matrix3 pinkyRotMat;
                        float pinkyRot = mHandPinkyRotation;
                        if (!mIsRightHand)
                            pinkyRot *= -1.0f;
                        pinkyRotMat.RotateAboutY(pinkyRot);
                        Multiply(pinkyRotMat, refRotMat, mDestHandTrans.m);
                        hasSpecialRotation = true;
                    }
                }
            }
            Scale(avgPos, 1.0f / engagedCount, avgPos);
            if (hasSpecialRotation)
                destOffset.y += mHandMoveForward;
            Add(mHandKeyboardOffset, destOffset, destOffset);
            Multiply(destOffset, mKeyboardRefBone->WorldXfm().m, destOffset);
            Vector3 finalPos;
            Add(avgPos, destOffset, finalPos);
            mDestHandTrans.v.Set(finalPos.x, finalPos.y, finalPos.z);
        }
        _ref0 = false;
    }
}

void CharIKFingers::CalculateFingerDest(FingerNum num) {
    if (mOutputTrans) {
        FingerDesc &finger = mFingers[num];
        if (finger.mNeedsUpdate) {
            Transform finger01Xfm;
            Multiply(finger.mFinger01->LocalXfm(), mOutputTrans->WorldXfm(), finger01Xfm);
            finger.mCurOrientVec = finger01Xfm.m.x;
            Vector3 eulerFinger02, eulerFinger03;
            MakeEuler(finger.mFinger02->LocalXfm().m, eulerFinger02);
            MakeEuler(finger.mFinger03->LocalXfm().m, eulerFinger03);
            finger.mCurFinger02Angle = eulerFinger02.z;
            finger.mCurFinger03Angle = eulerFinger03.z;
            finger.mNeedsUpdate = false;
        }
        if (finger.mNeedsIKSolve) {
            if (finger.mIsEngaged) {
                Transform f1Xfm, f2Xfm, f3Xfm, tipXfm;
                if (finger.mFinger01->TransParent() != mHand) {
                    Transform parentXfm;
                    Multiply(
                        finger.mFinger01->TransParent()->LocalXfm(), mDestHandTrans, parentXfm
                    );
                    Multiply(finger.mFinger01->LocalXfm(), parentXfm, f1Xfm);
                } else {
                    Multiply(finger.mFinger01->LocalXfm(), mDestHandTrans, f1Xfm);
                }

                Multiply(finger.mFinger02->LocalXfm(), f1Xfm, f2Xfm);
                Multiply(finger.mFinger03->LocalXfm(), f2Xfm, f3Xfm);
                Multiply(finger.mFingertip->LocalXfm(), f3Xfm, tipXfm);
                Vector3 targetPos;
                if (Distance(tipXfm.v, finger.mTargetWorldPos) > Distance(tipXfm.v, finger.mRefWorldPos)) {
                    targetPos = finger.mTargetWorldPos;
                } else
                    targetPos = finger.mRefWorldPos;

                Vector3 f1x(f1Xfm.m.x);
                Vector3 f1y(f1Xfm.m.y);
                Vector3 f1z(f1Xfm.m.z);
                Vector3 f1pos(f1Xfm.v);
                Vector3 toTarget;
                Subtract(targetPos, f1pos, toTarget);

                float len02 = Length(finger.mFinger02->LocalXfm().v);
                float lenTip = Length(finger.mFingertip->LocalXfm().v);
                float len03 = Length(finger.mFinger03->LocalXfm().v);
                float toTargetLen = Length(toTarget);
                float angle03 = std::acos(
                    ((len02 * len02 + lenTip * lenTip) - (toTargetLen - len03) * (toTargetLen - len03))
                    / (len02 * 2.0f * lenTip)
                );
                angle03 = -angle03;
                if (angle03 < 0.87f)
                    angle03 = 0.87f;
                float angle02 = angle03 * 0.5f + 1.5707964f;
                if (IsNaN(angle02)) {
                    angle02 = 2.96f;
                }
                finger.mDestFinger02Angle = PI - angle02;
                finger.mDestFinger03Angle = PI - angle02;
                Hmx::Quat curl03(f1z, -(angle02 * 2.0f - 2 * PI));
                Multiply(f1x, curl03, f1x);
                Hmx::Quat alignQuat;
                MakeRotQuat(f1x, toTarget, alignQuat);
                Multiply(f1Xfm.m.x, alignQuat, finger.mDestOrientVec);
                finger.mNeedsIKSolve = false;
            } else {
                Transform f1Xfm;
                Multiply(finger.mFinger01->LocalXfm(), mOutputTrans->WorldXfm(), f1Xfm);
                finger.mDestOrientVec = f1Xfm.m.x;
                Vector3 eulerF2, eulerF3;
                MakeEuler(finger.mFinger02->LocalXfm().m, eulerF2);
                MakeEuler(finger.mFinger03->LocalXfm().m, eulerF3);
                finger.mDestFinger02Angle = eulerF2.z;
                finger.mDestFinger03Angle = eulerF3.z;
                finger.mNeedsIKSolve = false;
            }
        }
    }
}

void CharIKFingers::MoveFinger(FingerNum num) {
    FingerDesc &finger = mFingers[num];
    if (finger.mIsEngaged || finger.mBlendFrames > 0 || finger.mBlendOutFrames > 0) {
        Transform baseXfm;
        Transform parentXfm(mDestHandTrans);
        if (finger.mFinger01->TransParent() != mHand) {
            Multiply(finger.mFinger01->TransParent()->LocalXfm(), mDestHandTrans, parentXfm);
        }
        Multiply(finger.mFinger01->LocalXfm(), parentXfm, baseXfm);

        float blendFactor = 1.0f;
        if (finger.mBlendFrames > 0 || finger.mBlendOutFrames > 0) {
            if (finger.mBlendFrames > 0) {
                blendFactor = 1.0f - finger.mBlendFrames / 5.0f;
            } else if (finger.mBlendOutFrames > 0) {
                blendFactor = 1.0f - finger.mBlendOutFrames / 5.0f;
            }
        }

        Interp(finger.mCurFinger02Angle, finger.mDestFinger02Angle, blendFactor, finger.mCurFinger02Angle);
        Interp(finger.mCurFinger03Angle, finger.mDestFinger03Angle, blendFactor, finger.mCurFinger03Angle);
        Hmx::Matrix3 rotMat;
        Vector3 euler02(0, 0, finger.mCurFinger02Angle);
        MakeRotMatrix(euler02, rotMat, true);
        finger.mFinger02->SetLocalRot(rotMat);
        euler02.Set(0, 0, finger.mCurFinger03Angle);
        MakeRotMatrix(euler02, rotMat, true);
        finger.mFinger03->SetLocalRot(rotMat);
        Interp(finger.mCurOrientVec, finger.mDestOrientVec, blendFactor, finger.mCurOrientVec);
        Hmx::Quat orientQuat;
        MakeRotQuat(baseXfm.m.x, finger.mCurOrientVec, orientQuat);
        Transform orientedXfm;
        Multiply(baseXfm.m.x, orientQuat, orientedXfm.m.x);
        Multiply(baseXfm.m.y, orientQuat, orientedXfm.m.y);
        Multiply(baseXfm.m.z, orientQuat, orientedXfm.m.z);
        Normalize(orientedXfm.m, orientedXfm.m);
        orientedXfm.v = baseXfm.v;
        Transform invParent;
        Invert(parentXfm, invParent);
        Multiply(orientedXfm, invParent, finger.mFinger01->DirtyLocalXfm());
        if (finger.mBlendOutFrames > 0)
            finger.mBlendOutFrames--;
        if (finger.mBlendFrames > 0)
            finger.mBlendFrames--;
    }
}

void CharIKFingers::FixSingleFinger(
    RndTransformable *t1, RndTransformable *t2, RndTransformable *t3
) {
    Vector3 avgX;
    if (t3) {
        Add(t1->WorldXfm().m.x, t3->WorldXfm().m.x, avgX);
        Scale(avgX, 0.5f, avgX);
    } else {
        avgX = t1->WorldXfm().m.x;
    }

    Hmx::Quat alignQuat;
    MakeRotQuat(t2->WorldXfm().m.x, avgX, alignQuat);

    Transform alignedXfm;
    Multiply(t2->WorldXfm().m.x, alignQuat, alignedXfm.m.x);
    Multiply(t2->WorldXfm().m.y, alignQuat, alignedXfm.m.y);
    Multiply(t2->WorldXfm().m.z, alignQuat, alignedXfm.m.z);
    alignedXfm.v = t2->WorldXfm().v;

    Transform invParent;
    Invert(t2->TransParent()->WorldXfm(), invParent);
    Multiply(alignedXfm, invParent, t2->DirtyLocalXfm());
}

void CharIKFingers::MeasureLengths() {
    for (int i = 0; i < 5; i++) {
        auto& _sub0 = mFingers[i];
        RndTransformable *f2 = _sub0.mFinger02;
        RndTransformable *f3 = _sub0.mFinger03;
        RndTransformable *tip = _sub0.mFingertip;
        if (f2 && f3 && tip) {
            float &totalLen = _sub0.mBoneTotalLength;
            totalLen = (Length(f2->LocalXfm().v) + (Length(f3->LocalXfm().v) + Length(tip->LocalXfm().v)));
        }
    }

    if (mHand && mHand->TransParent() && mHand->TransParent()->TransParent()) {
        mInv2ab = 2.0f;
        mAAPlusBB = 0;
        float handLen = Length(mHand->LocalXfm().v);
        mAAPlusBB += handLen * handLen;
        mInv2ab *= handLen;
        float foreArmLen = Length(mHand->TransParent()->LocalXfm().v);
        mAAPlusBB += foreArmLen * foreArmLen;
        mInv2ab = 1.0f / (mInv2ab * foreArmLen);
    }
}

void CharIKFingers::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mHand);
    changedBy.push_back(mHand);
    for (int i = 0; i < 5; i++) {
        FingerDesc desc(mFingers[i]);
        if (desc.mFinger01) {
            changedBy.push_back(desc.mFinger01);
        }
        if (desc.mFinger02) {
            changedBy.push_back(desc.mFinger02);
        }
        if (desc.mFinger03) {
            changedBy.push_back(desc.mFinger03);
        }
        if (desc.mFingertip) {
            changedBy.push_back(desc.mFingertip);
        }
    }
    if (mForeArm) {
        change.push_back(mForeArm);
        changedBy.push_back(mForeArm);
    }
    if (mUpperArm) {
        change.push_back(mUpperArm);
        changedBy.push_back(mUpperArm);
    }
}

void CharIKFingers::Highlight() {
    for (int i = 0; i < 5; i++) {
        FingerDesc desc(mFingers[i]);
        if (desc.mIsEngaged) {
            UtilDrawSphere(desc.mTargetWorldPos, 0.2f, Hmx::Color(1, 0, 0), 0);
            UtilDrawSphere(desc.mRefWorldPos, 0.2f, Hmx::Color(0, 1, 0), 0);
            UtilDrawAxes(desc.mFinger01->WorldXfm(), 1.0f, Hmx::Color(1, 1, 1));
            TheRnd.DrawLine(
                desc.mFinger01->WorldXfm().v,
                desc.mFinger02->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
            TheRnd.DrawLine(
                desc.mFinger02->WorldXfm().v,
                desc.mFinger03->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
            TheRnd.DrawLine(
                desc.mFinger03->WorldXfm().v,
                desc.mFingertip->WorldXfm().v,
                Hmx::Color(1, 1, 1),
                false
            );
        }
    }
}

void CharIKFingers::Poll() {
    if (!mHand || !mIsSetup)
        return;
    else {
        Hmx::Matrix3 mtx58;
        Hmx::Matrix3 mtx7c;
        Invert(mKeyboardRefBone->WorldXfm().m, mtx58);
        Multiply(mHand->WorldXfm().m, mtx58, mtx7c);
        Vector3 v88;
        Subtract(mKeyboardRefBone->WorldXfm().v, mHand->WorldXfm().v, v88);
        float weight = Weight();
        if (weight < 1.0) {
            if (mOutputTrans) {
                mOutputTrans->SetWorldXfm(mHand->WorldXfm());
            }
        } else {
            if (mResetCurHandTrans) {
                mCurHandTrans.Set(mHand->WorldXfm().m, mHand->WorldXfm().v);
                mDestHandTrans.Set(mHand->WorldXfm().m, mHand->WorldXfm().v);
                mResetCurHandTrans = false;
            }
            int i3 = 0;
            float f8 = 1.0f;
            int i1 = -1;
            for (int i = 0; i < 5; i++) {
                if (mFingers[i].mIsEngaged) {
                    if (i1 == -1)
                        i1 = i;
                    i3++;
                }
            }
            CalculateHandDest(i3, i1);
            if (mBlendInFrames > 0) {
                f8 = 1.0f - mBlendInFrames / 5.0f;
            } else if (mBlendOutFrames > 0) {
                f8 = 1.0f - mBlendOutFrames / 5.0f;
            }
            Interp(mCurHandTrans.v, mDestHandTrans.v, f8, mCurHandTrans.v);
            Interp(mCurHandTrans.m, mDestHandTrans.m, f8, mCurHandTrans.m);
            if (mOutputTrans) {
                mOutputTrans->SetWorldXfm(mCurHandTrans);
            }
            for (int i = 0; i < 5; i++) {
                CalculateFingerDest((FingerNum)i);
                MoveFinger((FingerNum)i);
            }
            if (i3 > 0) {
                for (int i = 2; i <= 4; i++) {
                    FingerDesc &prevFinger = mFingers[i - 1];
                    FingerDesc &curFinger = mFingers[i];
                    if (!curFinger.mIsEngaged) {
                        if (i == 4) {
                            FixSingleFinger(
                                prevFinger.mFinger01, curFinger.mFinger01, nullptr
                            );
                        } else {
                            FixSingleFinger(
                                prevFinger.mFinger01,
                                curFinger.mFinger01,
                                mFingers[i + 1].mFinger01
                            );
                        }
                    }
                }
            }
            if (mBlendInFrames > 0)
                mBlendInFrames--;
            if (mBlendOutFrames > 0)
                mBlendOutFrames--;
        }
    }
}
