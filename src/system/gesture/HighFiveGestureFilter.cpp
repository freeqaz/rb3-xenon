#include "gesture/HighFiveGestureFilter.h"
#include "gesture/BaseSkeleton.h"
#include "gesture/JointUtl.h"
#include "gesture/Skeleton.h"
#include "math/Vec.h"
#include "obj/Object.h"

HighFiveGestureFilter::HighFiveGestureFilter() : mHighFived(false) {}

HighFiveGestureFilter::~HighFiveGestureFilter() {}

bool HighFiveGestureFilter::CheckHighFive() {
    // One-shot check: returns true once after a high-five is detected,
    // then resets for the next detection
    if (mHighFived) {
        mHighFived = false;
        return true;
    }
    return false;
}

extern float kShoulderOffset;
extern float kCloseThreshold;
extern float kFarThreshold;
float kShoulderOffset = 0.2f;
float kCloseThreshold = 0.25f;
float kFarThreshold = 0.3f;

void HighFiveGestureFilter::Update(Skeleton const *skeleton1, Skeleton const *skeleton2) {
    if (skeleton1 && skeleton2) {
        Vector3 shoulderCenter1, shoulderCenter2;
        skeleton1->JointPos(kCoordCamera, kJointShoulderCenter, shoulderCenter1);
        skeleton2->JointPos(kCoordCamera, kJointShoulderCenter, shoulderCenter2);

        for (int i = 0; i < 4; i++) {
            const TrackedJoint &joint1 = skeleton1->HandJoint((SkeletonSide)(i & 1));
            const TrackedJoint &joint2 = skeleton2->HandJoint((SkeletonSide)(i >> 1));

            Vector3 pos1 = joint1.mJointPos[0];
            Vector3 pos2 = joint2.mJointPos[0];

            if ((pos1.y - kShoulderOffset > shoulderCenter1.y) ||
                (pos2.y - kShoulderOffset > shoulderCenter2.y)) {
                float dz = pos1.z - pos2.z;
                float dx = pos1.x - pos2.x;
                float dy = pos1.y - pos2.y;

                if ((dy * dy + (dx * dx + dz * dz)) < kCloseThreshold * kCloseThreshold) {
                    mHighFived = true;
                    return;
                }
            }

            Vector2 screenPos1, screenPos2;
            JointScreenPos(joint1, screenPos1);
            JointScreenPos(joint2, screenPos2);

            if ((screenPos1.y < 0.0f) && (screenPos2.y < 0.0f)) {
                float dy = pos1.y - pos2.y;
                float dz = pos1.z - pos2.z;
                float dx = pos1.x - pos2.x;

                if ((dx * dx + (dz * dz + dy * dy)) < kFarThreshold * kFarThreshold) {
                    mHighFived = true;
                    return;
                }
            }
        }
    }
}
