#include "gesture/HandsUpGestureFilter.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonQualityFilter.h"
#include "math/Mtx.h"
#include "obj/Object.h"

HandsUpGestureFilter::HandsUpGestureFilter() : mRequiredMs(500) { Clear(); }

HandsUpGestureFilter::~HandsUpGestureFilter() {}

BEGIN_PROPSYNCS(HandsUpGestureFilter)
    SYNC_PROP(required_ms, mRequiredMs)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void HandsUpGestureFilter::Update(Skeleton const &skeleton, int elapsed) {
    static float sHandsUpXThresh = -0.1f;
    static bool sForceHandsUp = false;
    if (sForceHandsUp) {
        mHandsUp = true;
        mRaisedMs = 1;
        return;
    }

    int idx = skeleton.SkeletonIndex();
    if (idx < 0)
        return;
    if (idx >= 6)
        return;

    SkeletonQualityFilter &qualityFilter = TheGestureMgr->GetSkeletonQualityFilter(idx);
    if (!skeleton.IsTracked() || qualityFilter.Sitting() || !qualityFilter.IsConfident()) {
        goto reset;
    }

    {
        const TrackedJoint &lShoulder = skeleton.ShoulderJoint(kSkeletonLeft);
        const TrackedJoint &rShoulder = skeleton.ShoulderJoint(kSkeletonRight);

        Vector3 shoulderDiff;
        shoulderDiff.x = lShoulder.mJointPos[kCoordCamera].x - rShoulder.mJointPos[kCoordCamera].x;
        shoulderDiff.y = lShoulder.mJointPos[kCoordCamera].y - rShoulder.mJointPos[kCoordCamera].y;
        shoulderDiff.z = lShoulder.mJointPos[kCoordCamera].z - rShoulder.mJointPos[kCoordCamera].z;
        Normalize(shoulderDiff, shoulderDiff);

        if (fabsf((shoulderDiff.y + shoulderDiff.x) * 0.0f + shoulderDiff.z) > 0.8f)
            goto reset;

        const TrackedJoint &rHand = skeleton.HandJoint(kSkeletonRight);
        const TrackedJoint &rElbow = skeleton.ElbowJoint(kSkeletonRight);
        const TrackedJoint &rShoulderR = skeleton.ShoulderJoint(kSkeletonRight);
        const TrackedJoint &lHand = skeleton.HandJoint(kSkeletonLeft);
        const TrackedJoint &lElbow = skeleton.ElbowJoint(kSkeletonLeft);
        const TrackedJoint &lShoulderL = skeleton.ShoulderJoint(kSkeletonLeft);

        Vector2 rightScreenPos, leftScreenPos;
        skeleton.ScreenPos(kJointHandRight, rightScreenPos);
        skeleton.ScreenPos(kJointHandLeft, leftScreenPos);

        static float sScreenBoundary = 0.05f;
        bool rightValid = rightScreenPos.x > sScreenBoundary && rightScreenPos.x < 1.0f - sScreenBoundary;
        bool leftValid = leftScreenPos.x > sScreenBoundary && leftScreenPos.x < 1.0f - sScreenBoundary;

        if (!rightValid || !leftValid)
            goto reset;

        static float sHandsUpYThresh = 0.1f;
        if (rHand.mJointPos[kCoordCamera].y <= rShoulderR.mJointPos[kCoordCamera].y + sHandsUpYThresh)
            goto reset;
        if (rHand.mJointPos[kCoordCamera].y <= rElbow.mJointPos[kCoordCamera].y)
            goto reset;
        if (rHand.mJointPos[kCoordCamera].x > rShoulderR.mJointPos[kCoordCamera].x + sHandsUpXThresh) {
            if (lHand.mJointPos[kCoordCamera].x <= lShoulderL.mJointPos[kCoordCamera].x + sHandsUpXThresh)
                goto reset;
        }
        if (lHand.mJointPos[kCoordCamera].y <= lShoulderL.mJointPos[kCoordCamera].y + sHandsUpYThresh)
            goto reset;
        if (lHand.mJointPos[kCoordCamera].y <= lElbow.mJointPos[kCoordCamera].y)
            goto reset;

        mRaisedMs += elapsed;
        if (mRaisedMs >= mRequiredMs) {
            mHandsUp = true;
        }
        return;
    }

reset:
    mHandsUp = false;
    mRaisedMs = 0;
}

void HandsUpGestureFilter::Update(int i, int j) {
    Skeleton *skel = TheGestureMgr->GetSkeletonByTrackingID(i);
    if (skel)
        Update(*skel, j);
    else {
        mHandsUp = false;
        mRaisedMs = 0;
    }
}

void HandsUpGestureFilter::Clear() {
    mHandsUp = false;
    mRaisedMs = 0;
}

BEGIN_HANDLERS(HandsUpGestureFilter)
    HANDLE_ACTION(clear, Clear())
    HANDLE_ACTION(update, Update(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(check, mHandsUp)
    HANDLE_EXPR(raised_ms, mRaisedMs)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
