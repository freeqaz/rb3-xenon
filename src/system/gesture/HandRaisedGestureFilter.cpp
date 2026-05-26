#include "gesture/HandRaisedGestureFilter.h"
#include "StandingStillGestureFilter.h"
#include "gesture/GestureMgr.h"
#include "obj/Object.h"

HandRaisedGestureFilter::HandRaisedGestureFilter()
    : mHandRaised(false), mRaisedMs(0), mRequiredMs(500) {
    Clear();
}

HandRaisedGestureFilter::~HandRaisedGestureFilter() {}

BEGIN_HANDLERS(HandRaisedGestureFilter)
    HANDLE_ACTION(clear, Clear())
    HANDLE_ACTION(update, Update(_msg->Int(2), _msg->Int(3)))
    HANDLE_EXPR(check, mHandRaised)
    HANDLE_EXPR(raised_ms, mRaisedMs)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(HandRaisedGestureFilter)
    SYNC_PROP(required_ms, mRequiredMs)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

void HandRaisedGestureFilter::SetRequiredMs(int ms) {
    mRequiredMs = ms;
    mStandingStillFilter.SetRequiredMs(ms / 2);
}

void HandRaisedGestureFilter::SetForwardFacingCutoff(float cutoff) {
    mStandingStillFilter.SetForwardFacingCutoff(cutoff);
}

void HandRaisedGestureFilter::RestoreDefaultForwardFacingCutoff() {
    mStandingStillFilter.RestoreDefaultForwardFacingCutoff();
}

void HandRaisedGestureFilter::Update(int trackingID, int j) {
    const Skeleton *skeleton = TheGestureMgr->GetSkeletonByTrackingID(trackingID);
    if (skeleton) {
        Update(*skeleton, j);
    } else {
        Clear();
    }
}

void HandRaisedGestureFilter::Clear() {
    mHandRaised = false;
    mRaisedMs = 0;
}

float kScreenMargin = 0.05f;
float kShoulderHeightOffset = 0.1f;
float kSideOffset = 0.3f;

void HandRaisedGestureFilter::Update(const Skeleton &skel, int deltaMs) {
    mHandRaised = false;
    int skelIdx = skel.SkeletonIndex();
    TheGestureMgr->unk30[skelIdx] = 1;
    mStandingStillFilter.Update(skel, deltaMs);
    if ((float)mStandingStillFilter.RaisedMs() <= 0.0f) {
        mRaisedMs = 0;
    } else {
        if (mRaisedMs == 0) {
            mRaisedMs = 1;
        }
        if (mStandingStillFilter.StandingStill()) {
            bool anyHandRaised = false;
            TheGestureMgr->unk30[skelIdx] = 12;
            for (int i = 0; i < 2; i++) {
                SkeletonSide side = (SkeletonSide)(i != 0);
                const TrackedJoint &hand = skel.HandJoint(side);
                const TrackedJoint &elbow = skel.ElbowJoint(side);
                const TrackedJoint &shoulder = skel.ShoulderJoint(side);
                Vector2 screenPos;
                skel.ScreenPos(
                    side == kSkeletonLeft ? kJointHandLeft : kJointHandRight, screenPos
                );
                bool inBounds = screenPos.x > kScreenMargin
                    && screenPos.x < 1.0f - kScreenMargin;
                if (inBounds
                    && hand.mJointPos[0].y
                        > shoulder.mJointPos[0].y + kShoulderHeightOffset
                    && hand.mJointPos[0].y > elbow.mJointPos[0].y) {
                    if ((side == kSkeletonRight
                         && hand.mJointPos[0].x
                             > shoulder.mJointPos[0].x - kSideOffset)
                        || (side == kSkeletonLeft
                            && hand.mJointPos[0].x
                                < shoulder.mJointPos[0].x + kSideOffset)) {
                        anyHandRaised = true;
                    }
                }
            }
            if (anyHandRaised) {
                TheGestureMgr->unk30[skelIdx] = 13;
                mRaisedMs += deltaMs;
            } else {
                mRaisedMs = 0;
            }
            if (mRaisedMs > mRequiredMs) {
                TheGestureMgr->unk30[skelIdx] = 14;
                mHandRaised = true;
            }
        }
    }
}
