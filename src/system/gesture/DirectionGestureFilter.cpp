#include "gesture/DirectionGestureFilter.h"
#include "BaseSkeleton.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/GestureMgr.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonViz.h"
#include "gesture/StandingStillGestureFilter.h"
#include "obj/Task.h"
#include "rndobj/Overlay.h"

float DirectionGestureFilter::sLastSwipeTime[6] = { -100, -100, -100, -100, -100, -100 };

#pragma region DirectionGestureFilterSingleUser

DirectionGestureFilterSingleUser::DirectionGestureFilterSingleUser(
    SkeletonSide s1, SkeletonSide s2, float f3, float f4
)
    : mHandSide(s1), mSwipeSide(s2), mSwipeAmt(0), mPercentPulled(0), mEngaged(0), mAllowAboveShoulder(1),
      mHighButtonMode(0), mSwipeCooldown(0.5) {
    Clear();
    SkeletonJoint wristJoint, elbowJoint;
    if (s1 == kSkeletonRight) {
        elbowJoint = kJointElbowRight;
        wristJoint = kJointWristRight;
    } else {
        elbowJoint = kJointElbowLeft;
        wristJoint = kJointWristLeft;
    }
    mArcDetector.Initialize(s2, wristJoint, elbowJoint, f3);
}

DirectionGestureFilterSingleUser::~DirectionGestureFilterSingleUser() {
    RndOverlay *swipeOverlay = RndOverlay::Find("swipe_direction", false);
    if (swipeOverlay && swipeOverlay->GetCallback() == this) {
        swipeOverlay->SetCallback(nullptr);
    }
}

void DirectionGestureFilterSingleUser::Clear() {
    mConfidence = kConfidenceNotTracked;
    ClearSwipe();
}

void DirectionGestureFilterSingleUser::Update(const Skeleton &skeleton, int elapsedMs) {
    static bool print = false;
    static float sVal = 0.2f;
    mConfidence = kConfidenceTracked;
    if (IsHandValid(skeleton)) {
        mArcDetector.Update(skeleton, elapsedMs);
    }
    if (!skeleton.IsTracked()) {
        mHasDirection = false;
        mConfidence = kConfidenceNotTracked;
    } else if (!mHasDirection) {
        mSwipeAmt = mArcDetector.GetSwipeAmount();
        if (sLastSwipeTime[skeleton.SkeletonIndex()] + mSwipeCooldown > TheTaskMgr.UISeconds()
            && sLastSwipeTime[skeleton.SkeletonIndex()] < TheTaskMgr.UISeconds()) {
            ClearSwipe();
        } else if (mSwipeAmt >= 1.0f) {
            if (print) {
                mArcDetector.PrintJointPath();
            }
            ClearSwipe();
            mHasDirection = true;
            sLastSwipeTime[skeleton.SkeletonIndex()] = TheTaskMgr.UISeconds();
        }
        mPercentPulled = Clamp(0.0f, 1.0f, (mSwipeAmt - sVal) / (1.0f - sVal));
    }
}

void DirectionGestureFilterSingleUser::Draw(const Skeleton &skeleton, SkeletonViz &viz) {
    mArcDetector.Draw(skeleton, viz);
    bool valid = IsValidSwipePosition(skeleton);
    viz.DrawPoint3D(
        skeleton.HandJoint(mHandSide).mJointPos[0],
        0.1f,
        valid ? Hmx::Color(0, 1, 0) : Hmx::Color(1, 0, 0),
        0.2f
    );
    if (sLastSwipeTime[skeleton.SkeletonIndex()] + mSwipeCooldown > TheTaskMgr.UISeconds()
        && sLastSwipeTime[skeleton.SkeletonIndex()] < TheTaskMgr.UISeconds()) {
        const Vector3 &pos = skeleton.HandJoint(mHandSide).mJointPos[0];
        viz.DrawPoint3D(pos, 0.1f, Hmx::Color(0, 1, 0), 0.2f);
    }
}

static float sValidHandFloats[4] = { 0.2f, 2.0f, 0.3f, 0.3f };

bool DirectionGestureFilterSingleUser::HandAtSide(
    const Skeleton &skeleton, float radius, float xScale, float elbowBlend
) const {
    float heightDiff =
        skeleton.TrackedJoints()[kJointHead].mJointPos[0].y
        - skeleton.TrackedJoints()[kJointFootRight].mJointPos[0].y;

    const TrackedJoint &hand = skeleton.HandJoint(mHandSide);
    const TrackedJoint &hip = skeleton.HipJoint(mHandSide);
    const TrackedJoint &knee = skeleton.KneeJoint(mHandSide);
    const TrackedJoint &elbow = skeleton.ElbowJoint(mHandSide);

    float sumY = knee.mJointPos[0].y + hip.mJointPos[0].y;
    float sumZ = knee.mJointPos[0].z + hip.mJointPos[0].z;
    float elbowX = elbow.mJointPos[0].x;
    float handY = hand.mJointPos[0].y;
    float elbowOffset = radius * elbowBlend + elbowX;
    float handX = hand.mJointPos[0].x;
    float handZ = hand.mJointPos[0].z;
    float threshold = heightDiff * 0.591715931892395f;
    sumY = sumY * 0.5f;
    sumZ = sumZ * 0.5f;
    float dx = handX - elbowOffset;
    threshold = threshold * radius;
    float dy = handY - sumY;
    float dz = handZ - sumZ;
    dx = dx * xScale;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    return dist <= threshold;
}

bool DirectionGestureFilterSingleUser::IsHandValid(const Skeleton &skeleton) const {
    return IsValidSwipePosition(skeleton)
        || (mArcDetector.NumJointsInPath() > 1U
            && !HandAtSide(skeleton, sValidHandFloats[0], sValidHandFloats[1], 0.0f));
}

bool DirectionGestureFilterSingleUser::IsValidScrollPos(const Skeleton &skeleton) const {
    if (IsValidSwipePosition(skeleton)) {
        return true;
    } else if (HandAtSide(skeleton, sValidHandFloats[2], 1.0f, 0.5f)) {
        return !HandAtSide(skeleton, sValidHandFloats[3], 1.0f, 0.0f);
    } else {
        return false;
    }
}

void DirectionGestureFilterSingleUser::ClearSwipe() {
    mSwipeAmt = 0;
    mHasDirection = false;
    mPercentPulled = 0;
    mArcDetector.Clear();
}

bool DirectionGestureFilterSingleUser::IsLockedIn() const {
    return mArcDetector.IsLockedIn();
}

void DirectionGestureFilterSingleUser::ResetHoverTimer() {
    mArcDetector.ResetHoverTimer();
}

float DirectionGestureFilterSingleUser::UpdateOverlay(RndOverlay *overlay, float f1) {
    return mArcDetector.UpdateOverlay(overlay, f1);
}

bool DirectionGestureFilterSingleUser::IsValidSwipePosition(const Skeleton &skeleton) const {
    const TrackedJoint *joints = skeleton.TrackedJoints();

    float shoulderRightY = joints[kJointShoulderRight].mJointPos[0].y;
    float shoulderLeftY = joints[kJointShoulderLeft].mJointPos[0].y;

    float shoulderRightZ = joints[kJointShoulderRight].mJointPos[0].z;
    float shoulderLeftZ = joints[kJointShoulderLeft].mJointPos[0].z;

    float hipX = joints[kJointHipCenter].mJointPos[0].x;
    float shoulderX = joints[kJointShoulderCenter].mJointPos[0].x;
    float deltaX = hipX - shoulderX;

    float hipY = joints[kJointHipCenter].mJointPos[0].y;
    float shoulderY = joints[kJointShoulderCenter].mJointPos[0].y;
    float deltaY = hipY - shoulderY;

    float hipZ = joints[kJointHipCenter].mJointPos[0].z;
    float shoulderZ = joints[kJointShoulderCenter].mJointPos[0].z;
    float deltaZ = hipZ - shoulderZ;

    float shoulderLeftX = joints[kJointShoulderLeft].mJointPos[0].x;
    float shoulderRightX = joints[kJointShoulderRight].mJointPos[0].x;
    float dx = shoulderLeftX - shoulderRightX;
    float dy = shoulderLeftY - shoulderRightY;
    float dz = shoulderLeftZ - shoulderRightZ;

    float shoulderDist = sqrtf(dx * dx + dz * dz + dy * dy);

    // Build corners with reassignments (operand order matters for codegen)
    Vector3 corner1, corner2;
    corner1.x = shoulderX - deltaX;
    deltaX = deltaX + hipX;  // reassign (swap operands)
    corner2.x = deltaX;

    corner1.y = shoulderY - deltaY;
    deltaY = deltaY + hipY;  // reassign (swap operands)
    corner2.y = deltaY;

    corner1.z = shoulderZ - deltaZ;
    deltaZ = deltaZ + hipZ;  // reassign (swap operands)
    corner2.z = deltaZ;

    // NOW call HandJoint for ClosestPoint
    const TrackedJoint &handJoint = skeleton.HandJoint(mHandSide);
    Vector3 closest;
    ClosestPoint(corner1, corner2, handJoint.mJointPos[0], &closest);

    // Call HandJoint AGAIN for delta calculation
    const TrackedJoint &handJoint2 = skeleton.HandJoint(mHandSide);
    float closestDeltaX = closest.x - handJoint2.mJointPos[0].x;
    float closestDeltaZ = closest.z - handJoint2.mJointPos[0].z;

    float hipLeftX = joints[kJointHipLeft].mJointPos[0].x;
    float hipRightX = joints[kJointHipRight].mJointPos[0].x;
    float hipLeftY = joints[kJointHipLeft].mJointPos[0].y;
    float hipRightY = joints[kJointHipRight].mJointPos[0].y;
    float hipLeftZ = joints[kJointHipLeft].mJointPos[0].z;
    float hipRightZ = joints[kJointHipRight].mJointPos[0].z;

    Vector3 direction;
    direction.x = hipRightX - hipLeftX;
    direction.y = hipRightY - hipLeftY;
    direction.z = hipRightZ - hipLeftZ;
    Normalize(direction, direction);

    float angle = atan2(direction.z, direction.x);
    float s1 = Sine(1.5707963705062866f - angle);  // Use exact constant from binary
    float s2 = Sine(-angle);

    float rotX = closestDeltaX * s2 + closestDeltaZ * s1;
    float rotY = closestDeltaX * s1 - closestDeltaZ * s2;

    float width, height;
    if (mEngaged) {
        height = 0.4f;
        width = 0.35f;
    } else {
        height = 0.3f;
        width = 0.25f;
    }

    height *= shoulderDist;
    width *= shoulderDist;

    float ellipseTest = (rotX * rotX) / (width * width) + (rotY * rotY) / (height * height);

    if (ellipseTest < 1.0f) {
        return 0;
    }

    if (!mAllowAboveShoulder || mHighButtonMode) {
        // Call HandJoint AGAIN for Y-test
        const TrackedJoint &handJoint3 = skeleton.HandJoint(mHandSide);
        float shoulderYFresh = joints[kJointShoulderCenter].mJointPos[0].y;
        float yTest = handJoint3.mJointPos[0].y - shoulderYFresh;
        if (mHighButtonMode) {
            if (yTest < 0.0f) {
                return 0;
            }
        } else {
            if (yTest > 0.0f) {
                return 0;
            }
        }
    }
    return 1;
}

#pragma endregion
#pragma region DirectionGestureFilterDoubleUser

DirectionGestureFilterDoubleUser::DirectionGestureFilterDoubleUser(
    SkeletonSide s1, SkeletonSide s2, float f3, float f4
)
    : mFilter1(new DirectionGestureFilterSingleUser(s1, s2, f3, f4)),
      mFilter2(new DirectionGestureFilterSingleUser(s1, s2, f3, f4)) {
    for (int i = 0; i < 2; i++) {
        mStillFilters[i] = new StandingStillGestureFilter();
        mStillFilters[i]->SetRequiredMs(750);
        mStillFilters[i]->SetUnk48(true);
    }
}

DirectionGestureFilterDoubleUser::~DirectionGestureFilterDoubleUser() {
    delete mFilter1;
    delete mFilter2;
    for (int i = 0; i < 2; i++) {
        delete mStillFilters[i];
    }
}

void DirectionGestureFilterDoubleUser::Clear() {
    mFilter1->Clear();
    mFilter2->Clear();
}

JointConfidence DirectionGestureFilterDoubleUser::Confidence() const {
    return Max(mFilter1->Confidence(), mFilter2->Confidence());
}

void DirectionGestureFilterDoubleUser::Update(const Skeleton &skeleton, int ms) {
    for (int i = 0; i < 2; i++) {
        mStillFilters[i]->Update(TheGestureMgr->GetPlayerSkeletonID(i), ms);
    }
    int i1, i2;
    GetValidSkeletons(i1, i2);
    mFilter1->Update(TheGestureMgr->GetSkeleton(i1), ms);
    mFilter2->Update(TheGestureMgr->GetSkeleton(i2), ms);
}

void DirectionGestureFilterDoubleUser::Draw(const Skeleton &skeleton, SkeletonViz &viz) {
    mFilter1->Draw(skeleton, viz);
}

bool DirectionGestureFilterDoubleUser::HasDirection() const {
    return mFilter1->HasDirection() || mFilter2->HasDirection();
}

float DirectionGestureFilterDoubleUser::GetPercentPulled() const {
    return Max(mFilter1->GetPercentPulled(), mFilter2->GetPercentPulled());
}

void DirectionGestureFilterDoubleUser::ClearSwipe() {
    mFilter1->ClearSwipe();
    mFilter2->ClearSwipe();
}

bool DirectionGestureFilterDoubleUser::IsLockedIn() const {
    return mFilter1->IsLockedIn() || mFilter2->IsLockedIn();
}

void DirectionGestureFilterDoubleUser::SetEngaged(bool engaged) {
    mFilter1->SetEngaged(engaged);
    mFilter2->SetEngaged(engaged);
}

void DirectionGestureFilterDoubleUser::ResetHoverTimer() {
    mFilter1->ResetHoverTimer();
    mFilter2->ResetHoverTimer();
}

void DirectionGestureFilterDoubleUser::SetAllowAboveShoulder(bool allow) {
    mFilter1->SetAllowAboveShoulder(allow);
    mFilter2->SetAllowAboveShoulder(allow);
}

void DirectionGestureFilterDoubleUser::SetHighButtonMode(bool set) {
    mFilter1->SetHighButtonMode(set);
    mFilter2->SetHighButtonMode(set);
}

void DirectionGestureFilterDoubleUser::GetValidSkeletons(int &out1, int &out2) const {
    int id1 = TheGestureMgr->GetPlayerSkeletonID(0);
    int id2 = TheGestureMgr->GetPlayerSkeletonID(1);
    int idx;
    if (id1 != -1) {
        idx = TheGestureMgr->GetSkeletonIndexByTrackingID(id1);
    } else {
        idx = -1;
    }
    out1 = idx;
    if (id2 != -1) {
        idx = TheGestureMgr->GetSkeletonIndexByTrackingID(id2);
    } else {
        idx = -1;
    }
    out2 = idx;
    if (out1 != -1) {
        if (!TheGestureMgr->IsSkeletonValid(out1)) {
            out1 = -1;
        }
    }
    if (out2 != -1) {
        if (!TheGestureMgr->IsSkeletonValid(out2)) {
            out2 = -1;
        }
    }
}

bool DirectionGestureFilterDoubleUser::IsHandValid(const Skeleton &skeleton) const {
    int i1, i2;
    GetValidSkeletons(i1, i2);
    bool result = false;
    if (i1 >= 0 && mFilter1->IsHandValid(TheGestureMgr->GetSkeleton(i1))
        && mStillFilters[0]->StandingStill()) {
        result = true;
    } else if (i2 >= 0 && mFilter2->IsHandValid(TheGestureMgr->GetSkeleton(i2))
               && mStillFilters[1]->StandingStill()) {
        result = true;
    }
    return result;
}

bool DirectionGestureFilterDoubleUser::IsValidScrollPos(const Skeleton &skeleton) const {
    int i1, i2;
    GetValidSkeletons(i1, i2);
    bool result = false;
    if (i1 >= 0 && mFilter1->IsValidScrollPos(TheGestureMgr->GetSkeleton(i1))) {
        result = true;
    } else if (i2 >= 0 && mFilter2->IsValidScrollPos(TheGestureMgr->GetSkeleton(i2))) {
        result = true;
    }
    return result;
}
