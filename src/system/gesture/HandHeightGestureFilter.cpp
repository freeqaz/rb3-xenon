#include "gesture/HandHeightGestureFilter.h"
#include "BaseSkeleton.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"

HandHeightGestureFilter::HandHeightGestureFilter(SkeletonSide side)
    : mSide(side), mHeightOffset(0.15), mTrackingState(0) {}

void HandHeightGestureFilter::Clear() { mTrackingState = 0; }

void HandHeightGestureFilter::Update(const Skeleton &skeleton, int i2) {
    if (!skeleton.IsTracked()) {
        mTrackingState = 0;
        mHandHeight = 0.5;
    } else {
        const TrackedJoint &shoulder = skeleton.ShoulderJoint(mSide);
        const TrackedJoint &hip = skeleton.HipJoint(mSide);
        const TrackedJoint &hand = skeleton.HandJoint(mSide);

        float f2 = shoulder.mJointPos[kCoordCamera].y - hip.mJointPos[kCoordCamera].y;
        if (f2 != 0) {
            mHandHeight =
                (shoulder.mJointPos[kCoordCamera].y - hand.mJointPos[kCoordCamera].y) / f2
                + mHeightOffset;
        } else
            mHandHeight = 0;

        skeleton.HipJoint(mSide);
        skeleton.KneeJoint(mSide);
        skeleton.ElbowJoint(mSide);
        mTrackingState = 2;
    }
}
