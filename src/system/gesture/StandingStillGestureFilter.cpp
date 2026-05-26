#include "gesture/StandingStillGestureFilter.h"
#include "StandingStillGestureFilter.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "math/Vec.h"
#include "obj/Object.h"

StandingStillGestureFilter::StandingStillGestureFilter()
    : mRequiredMs(500), mForwardFacingCutoff(0.4f), unk48(false) {
    Clear();
}

StandingStillGestureFilter::~StandingStillGestureFilter() {}

BEGIN_HANDLERS(StandingStillGestureFilter)
    HANDLE_ACTION(clear, Clear())
    HANDLE_ACTION(update, Update(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(check, mStandingStill)
    HANDLE_EXPR(raised_ms, mRaisedMs)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(StandingStillGestureFilter)
    SYNC_PROP(required_ms, mStandingStill)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void StandingStillGestureFilter::SetForwardFacingCutoff(float cutoff) {
    mForwardFacingCutoff = cutoff;
}

void StandingStillGestureFilter::RestoreDefaultForwardFacingCutoff() {
    mForwardFacingCutoff = 0.4f;
}

void StandingStillGestureFilter::Update(const Skeleton &skeleton, int ms) {
    int idx = skeleton.SkeletonIndex();
    if (idx < 0 || idx >= 6)
        return;

    SkeletonQualityFilter &filter = TheGestureMgr->GetSkeletonQualityFilter(idx);
    filter.Update(skeleton, TheGestureMgr->mInShellMode);

    if (!skeleton.IsTracked() && !TheGestureMgr->IsTrackingAllSkeletons()) {
        TheGestureMgr->unk30[idx] = 2;
        mRaisedMs = 0;
        mStandingStill = false;
        return;
    }

    if (unk48)
        goto StandingStillLogic;

    {
        int state = 0;
        if (filter.Sitting()) {
            state = 3;
        } else if (!filter.IsConfident()) {
            state = (int)filter.Confidence() + 400;
        } else if (filter.Sideways()) {
            state = 5;
        } else {
            const TrackedJoint &leftShoulder = skeleton.ShoulderJoint(kSkeletonLeft);
            const TrackedJoint &rightShoulder = skeleton.ShoulderJoint(kSkeletonRight);
            Vector3 shoulderDiff;
            shoulderDiff.x = rightShoulder.mJointPos[0].x - leftShoulder.mJointPos[0].x;
            shoulderDiff.y = rightShoulder.mJointPos[0].y - leftShoulder.mJointPos[0].y;
            shoulderDiff.z = rightShoulder.mJointPos[0].z - leftShoulder.mJointPos[0].z;
            Normalize(shoulderDiff, shoulderDiff);
            float shoulderFacing =
                ((shoulderDiff.y + shoulderDiff.x) * 0.0f) + shoulderDiff.z;
            if (std::fabs(shoulderFacing) > mForwardFacingCutoff) {
                state = 6;
            } else {
                Vector2 handLeftPos, handRightPos;
                skeleton.ScreenPos(kJointHandLeft, handLeftPos);
                skeleton.ScreenPos(kJointHandRight, handRightPos);
                if (handLeftPos.x > handRightPos.x) {
                    state = 7;
                } else {
                    Vector2 kneeLeftPos, kneeRightPos;
                    skeleton.ScreenPos(kJointKneeLeft, kneeLeftPos);
                    skeleton.ScreenPos(kJointKneeRight, kneeRightPos);
                    if (kneeLeftPos.x > kneeRightPos.x) {
                        state = 8;
                    } else {
                        const TrackedJoint *joints = skeleton.TrackedJoints();
                        Vector3 v1, v2, v3, v4;
                        const Vector3 &kneeR = joints[kJointKneeRight].mJointPos[0];
                        v1.x = kneeR.x - joints[kJointHipRight].mJointPos[0].x;
                        v1.y = kneeR.y - joints[kJointHipRight].mJointPos[0].y;
                        v1.z = kneeR.z - joints[kJointHipRight].mJointPos[0].z;
                        v2.x = kneeR.x - joints[kJointAnkleRight].mJointPos[0].x;
                        v2.y = kneeR.y - joints[kJointAnkleRight].mJointPos[0].y;
                        v2.z = kneeR.z - joints[kJointAnkleRight].mJointPos[0].z;
                        Normalize(v1, v1);
                        Normalize(v2, v2);

                        const Vector3 &kneeL = joints[kJointKneeLeft].mJointPos[0];
                        v3.x = kneeL.x - joints[kJointHipLeft].mJointPos[0].x;
                        v3.y = kneeL.y - joints[kJointHipLeft].mJointPos[0].y;
                        v3.z = kneeL.z - joints[kJointHipLeft].mJointPos[0].z;
                        v4.x = kneeL.x - joints[kJointAnkleLeft].mJointPos[0].x;
                        v4.y = kneeL.y - joints[kJointAnkleLeft].mJointPos[0].y;
                        v4.z = kneeL.z - joints[kJointAnkleLeft].mJointPos[0].z;
                        Normalize(v3, v3);
                        Normalize(v4, v4);

                        float dotR = v2.x * v1.x + v2.y * v1.y + v2.z * v1.z;
                        if (dotR > -0.75f) {
                            state = 9;
                        } else {
                            float dotL = v4.x * v3.x + v4.y * v3.y + v4.z * v3.z;
                            if (dotL > -0.75f) {
                                state = 9;
                            } else {
                                goto StandingStillLogic;
                            }
                        }
                    }
                }
            }
        }
        TheGestureMgr->unk30[idx] = state;
        mRaisedMs = 0;
        mStandingStill = false;
        return;
    }

StandingStillLogic:
    if (mRaisedMs == 0) {
        unk38 = skeleton.GetUnkab0();
    }
    Vector3 savedPos = unk38;
    const Vector3 &pos = skeleton.GetUnkab0();
    float dx = savedPos.x - pos.x;
    float dy = savedPos.y - pos.y;
    float dz = savedPos.z - pos.z;
    if (dx * dx + dy * dy + dz * dz < 0.0625f) {
        mRaisedMs += ms;
    } else {
        TheGestureMgr->unk30[idx] = 10;
        mStandingStill = false;
        mRaisedMs = 0;
    }
    if (mRaisedMs > mRequiredMs) {
        mStandingStill = true;
    } else {
        TheGestureMgr->unk30[idx] = 11;
        mStandingStill = false;
    }
}

void StandingStillGestureFilter::Update(int trackingID, int j) {
    const Skeleton *skeleton = TheGestureMgr->GetSkeletonByTrackingID(trackingID);
    if (skeleton) {
        Update(*skeleton, j);
    } else {
        Clear();
    }
}

void StandingStillGestureFilter::Clear() {
    mStandingStill = 0;
    mRaisedMs = 0;
}
