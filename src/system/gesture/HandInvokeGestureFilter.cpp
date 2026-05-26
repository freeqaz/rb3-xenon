#include "gesture/HandInvokeGestureFilter.h"
#include "math/Rot.h"
#include <cmath>

HandInvokeGestureFilter::HandInvokeGestureFilter()
    : unk4(Vector3(0, 0, -1), 3, 0), unk50(Vector3::GetZero(), 3, 0),
      unk8c(Vector3::GetZero(), 6, 0), unkc8(Vector3::GetZero(), 3, 0),
      unk104(Vector3::GetZero(), 6, 0), mInvokeDetected(0), unk144(0) {}

HandInvokeGestureFilter::~HandInvokeGestureFilter() {}

float HandInvokeGestureFilter::GetBend(
    const Vector3 &a, const Vector3 &b, const Vector3 &c
) const {
    Vector3 d1(b.x - c.x, b.y - c.y, b.z - c.z);
    Normalize(d1, d1);
    Vector3 d2(a.x - c.x, a.y - c.y, a.z - c.z);
    Normalize(d2, d2);
    return std::acos(d2.x * d1.x + d2.y * d1.y + d2.z * d1.z);
}

bool HandInvokeGestureFilter::UpdateBodyPlane(const Skeleton &skel, float dt) {
    const TrackedJoint &rightShoulder = skel.ShoulderJoint(kSkeletonRight);
    const TrackedJoint &leftShoulder = skel.ShoulderJoint(kSkeletonLeft);

    bool valid = false;
    float dx = leftShoulder.mJointPos[0].x - rightShoulder.mJointPos[0].x;
    float dy = leftShoulder.mJointPos[0].y - rightShoulder.mJointPos[0].y;
    float dz = leftShoulder.mJointPos[0].z - rightShoulder.mJointPos[0].z;

    if (skel.ShoulderJoint(kSkeletonLeft).mJointConf != kConfidenceNotTracked
        && skel.ShoulderJoint(kSkeletonRight).mJointConf != kConfidenceNotTracked
        && dx * dx + dy * dy + dz * dz > 0.0f) {
        valid = true;
    }

    if (valid) {
        // Cross(yAxis, shoulderVec) = body forward normal in XZ plane
        Vector3 bodyNormal(dz - dy * 0.0f, dx * 0.0f - dz * 0.0f, dy * 0.0f - dx);
        Normalize(bodyNormal, bodyNormal);
        unk4.Smooth(bodyNormal, dt, true);

        // Compute body "side" vector as Cross(yAxis, smoothedBodyNormal)
        Vector3 smoothed = unk4.Value();
        float sx = smoothed.x;
        float sy = smoothed.y;
        float sz = smoothed.z;
        unk40.y = sx * 0.0f - sz * 0.0f;
        unk40.z = sy * 0.0f - sx;
        unk40.x = sz - sy * 0.0f;
        Normalize(unk40, unk40);

        // Check body orientation angle: project bodyNormal to XZ plane and get angle
        Vector3 projected = bodyNormal;
        projected.y = 0.0f;
        Normalize(projected, projected);
        float angle = std::acos((projected.x + projected.y) * 0.0f + projected.z);
        if (angle < 32.0f * DEG2RAD) {
            return true;
        }
    }
    return false;
}

bool HandInvokeGestureFilter::CalcInPose(const Skeleton &skel, float dt) {
    // Smooth hand and shoulder positions
    const TrackedJoint &rightHand = skel.HandJoint(kSkeletonRight);
    unk50.Smooth(rightHand.mJointPos[0], dt, false);
    const TrackedJoint &rightShoulder = skel.ShoulderJoint(kSkeletonRight);
    unk8c.Smooth(rightShoulder.mJointPos[0], dt, false);
    const TrackedJoint &leftHand = skel.HandJoint(kSkeletonLeft);
    unkc8.Smooth(leftHand.mJointPos[0], dt, false);
    const TrackedJoint &leftShoulder = skel.ShoulderJoint(kSkeletonLeft);
    unk104.Smooth(leftShoulder.mJointPos[0], dt, false);

    // Right arm direction: shoulder - hand, normalized
    Vector3 rightHandVal = unk50.Value();
    Vector3 rightShoulderVal = unk8c.Value();
    Vector3 rightArmDir(
        rightShoulderVal.x - rightHandVal.x,
        rightShoulderVal.y - rightHandVal.y,
        rightShoulderVal.z - rightHandVal.z
    );
    Normalize(rightArmDir, rightArmDir);

    // Left arm direction: shoulder - hand, normalized
    Vector3 leftHandVal = unkc8.Value();
    Vector3 leftShoulderVal = unk104.Value();
    Vector3 leftArmDir(
        leftShoulderVal.x - leftHandVal.x,
        leftShoulderVal.y - leftHandVal.y,
        leftShoulderVal.z - leftHandVal.z
    );
    Normalize(leftArmDir, leftArmDir);

    // Spine vector: shoulderCenter - hipCenter
    float spineX = skel.TrackedJoints()[kJointShoulderCenter].mJointPos[0].x
        - skel.TrackedJoints()[kJointHipCenter].mJointPos[0].x;
    float spineY = skel.TrackedJoints()[kJointShoulderCenter].mJointPos[0].y
        - skel.TrackedJoints()[kJointHipCenter].mJointPos[0].y;
    float spineZ = skel.TrackedJoints()[kJointShoulderCenter].mJointPos[0].z
        - skel.TrackedJoints()[kJointHipCenter].mJointPos[0].z;

    // Project spine onto body normal direction, remove that component to get lateral
    Vector3 bodyNormalVal = unk4.Value();
    float spineDot = bodyNormalVal.x * spineX + bodyNormalVal.y * spineY + bodyNormalVal.z * spineZ;

    Vector3 bodyNormalVal2 = unk4.Value();
    float projX = bodyNormalVal2.x * spineDot;
    float projY = bodyNormalVal2.y * spineDot;
    float projZ = bodyNormalVal2.z * spineDot;
    Vector3 lateral(spineX - projX, spineY - projY, spineZ - projZ);
    Normalize(lateral, lateral);

    // Project arm directions onto body vectors
    float rightElevation = -(unk40.x * rightArmDir.x + unk40.y * rightArmDir.y + unk40.z * rightArmDir.z);
    Vector3 bodyNormalVal3 = unk4.Value();
    float leftElevation = -(unk40.x * leftArmDir.x + unk40.y * leftArmDir.y + unk40.z * leftArmDir.z);
    float rightForward = rightArmDir.x * bodyNormalVal3.x + rightArmDir.y * bodyNormalVal3.y + rightArmDir.z * bodyNormalVal3.z;
    Vector3 bodyNormalVal4 = unk4.Value();
    float negZero = -0.0f;
    float leftForward = bodyNormalVal4.x * leftArmDir.x + bodyNormalVal4.y * leftArmDir.y + bodyNormalVal4.z * leftArmDir.z;

    // Compute angles for right arm
    float rightAngle1 = std::atan2(rightElevation, (rightArmDir.z + rightArmDir.x) * negZero - rightArmDir.y);
    float rightAngle2 = std::atan2(rightElevation, rightForward);
    const TrackedJoint &rHand = skel.HandJoint(kSkeletonRight);
    const TrackedJoint &rElbow = skel.ElbowJoint(kSkeletonRight);
    const TrackedJoint &rShoulder = skel.ShoulderJoint(kSkeletonRight);
    float rightBend = GetBend(rShoulder.mJointPos[0], rElbow.mJointPos[0], rHand.mJointPos[0]);

    // Compute angles for left arm
    float leftAngle1 = std::atan2(leftElevation, (leftArmDir.z + leftArmDir.x) * negZero - leftArmDir.y);
    float leftAngle2 = std::atan2(leftElevation, leftForward);
    const TrackedJoint &lHand = skel.HandJoint(kSkeletonLeft);
    const TrackedJoint &lElbow = skel.ElbowJoint(kSkeletonLeft);
    const TrackedJoint &lShoulder = skel.ShoulderJoint(kSkeletonLeft);
    float leftBend = GetBend(lShoulder.mJointPos[0], lElbow.mJointPos[0], lHand.mJointPos[0]);

    // Lateral body tilt angle
    float tiltAngle = std::acos((lateral.x + lateral.y) * 0.0f + lateral.z);

    // Wrap negative angles to [0, 2*PI]
    if (rightAngle1 < 0.0f) {
        rightAngle1 += 2.0f * PI;
    }
    if (rightAngle2 < 0.0f) {
        rightAngle2 += 2.0f * PI;
    }
    if (leftAngle1 < 0.0f) {
        leftAngle1 += 2.0f * PI;
    }
    if (leftAngle2 < 0.0f) {
        leftAngle2 += 2.0f * PI;
    }

    // Check right arm pose
    bool rightInPose = true;
    if (rightAngle1 >= 155.0f * DEG2RAD || rightAngle1 <= 115.0f * DEG2RAD
        || rightAngle2 >= 110.0f * DEG2RAD || rightAngle2 <= 70.0f * DEG2RAD
        || rightBend >= 35.0f * DEG2RAD) {
        rightInPose = false;
    }

    // Check left arm pose
    bool leftInPose = true;
    if (leftAngle1 >= 200.0f * DEG2RAD || leftAngle1 <= 160.0f * DEG2RAD
        || leftAngle2 >= 2.0f * PI || leftAngle2 <= 0.0f
        || leftBend >= 110.0f * DEG2RAD) {
        leftInPose = false;
    }

    // If left arm not in pose, check joint tracking confidence
    if (!leftInPose) {
        if (skel.HandJoint(kSkeletonLeft).mJointConf != kConfidenceNotTracked
            && skel.ShoulderJoint(kSkeletonLeft).mJointConf != kConfidenceNotTracked) {
            leftInPose = false;
        } else {
            leftInPose = true;
        }
    }

    if (leftInPose && rightInPose && tiltAngle < 9.0f * DEG2RAD) {
        return true;
    }
    return false;
}

void HandInvokeGestureFilter::Update(const Skeleton &skel, int ms) {
    bool wasInvokeDetected = mInvokeDetected;
    float dt = (float)(double)(long long)ms * 0.001f;
    if (skel.IsTracked()) {
        bool bodyPlaneValid = UpdateBodyPlane(skel, dt);
        mInvokeDetected = false;
        if (bodyPlaneValid) {
            mInvokeDetected = CalcInPose(skel, dt);
            if (mInvokeDetected) {
                return;
            }
        }
        if (wasInvokeDetected) {
            return;
        }
    }
    mInvokeDetected = false;
    unk144 = 0;
}
