#include "hamobj/Pose.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "utl/Std.h"

Pose::Pose(int x, ScoreMode s) : unk18(x), mScoreMode(s) {}
Pose::~Pose() { DeleteAll(mElements); }

void Pose::AddElement(PoseElement *e) { mElements.push_back(e); }

void Pose::Update(const Skeleton &skeleton) {
    MILO_ASSERT(!mElements.empty(), 0x8a);
    float weightedSum = 0.0f;
    float totalWeight = 0.0f;
    for (std::vector<PoseElement *>::iterator it = mElements.begin(); it != mElements.end();
         ++it) {
        PoseElement *elem = *it;
        float score = elem->Score(skeleton);
        weightedSum += score * elem->unk4;
        totalWeight += elem->unk4;
    }
    MILO_ASSERT(totalWeight != 0.0f, 0x95);
    float score = weightedSum / totalWeight;
    unk10.push_back(score);
    unsigned int count = 0;
    for (std::list<float>::iterator it = unk10.begin(); it != unk10.end(); ++it) {
        count++;
    }
    if ((unsigned int)unk18 < count) {
        unk10.erase(unk10.begin());
    }
}

float Pose::CurrentScore() const {
    float minVal = 1.0f;
    float sum = 0.0f;
    std::list<float>::const_iterator it = unk10.begin();
    while (it != unk10.end()) {
        float val = *it;
        ++it;
        sum += val;
        minVal = minVal - val < 0.0 ? minVal : val;
    }
    switch (mScoreMode) {
    case (ScoreMode)0:
        return sum / (float)unk18;
    case (ScoreMode)1: {
        unsigned int count = 0;
        for (std::list<float>::const_iterator it = unk10.begin(); it != unk10.end(); ++it) {
            count++;
        }
        if (count < (unsigned int)unk18) {
            return 0.0f;
        }
        return minVal;
    }
    default:
        MILO_FAIL("Bad Pose ScoreMode!");
        return 0.0f;
    }
}

JointDistPoseElement::JointDistPoseElement(
    SkeletonJoint j1, SkeletonJoint j2, float minDist, float maxDist
)
    : PoseElement(1.0f), mJoint1(j1), mJoint2(j2), mMinDist(minDist), mMaxDist(maxDist), mCoordSys(0) {
    MILO_ASSERT(minDist <= maxDist, 0x2f);
}

float JointDistPoseElement::Score(const Skeleton &skeleton) const {
    Vector3 pos1, pos2;
    skeleton.JointPos((SkeletonCoordSys)mCoordSys, mJoint1, pos1);
    skeleton.JointPos((SkeletonCoordSys)mCoordSys, mJoint2, pos2);

    float dy = pos1.y - pos2.y;
    float dx = pos1.x - pos2.x;
    float dz = pos1.z - pos2.z;

    float dist = sqrtf(dx * dx + dy * dy + dz * dz);

    if (!(dist < mMinDist) && !(dist > mMaxDist)) {
        return 1.0f;
    }
    return 0.0f;
}

float CamDistancePoseElement::Score(const Skeleton &skeleton) const {
    Vector3 pos;
    skeleton.JointPos(kCoordCamera, kJointSpine, pos);
    if (pos.z > unk8)
        return 1.0f;
    return 0.0f;
}

float BoneAngleRangePoseElement::Score(const Skeleton &skeleton) const {
    Vector3 boneDir;
    skeleton.BoneVec(mBone, kCoordCamera, boneDir);
    Normalize(boneDir, boneDir);
    const Vector3& angle = mAngle;
    MILO_ASSERT(1.0f - 0.001f <= Length(angle) && Length(angle) <= 1.0f + 0.001f, 0x21);
    float dot = boneDir.x * angle.x + boneDir.y * angle.y + boneDir.z * angle.z;
    float acosAngle = acosf(dot);
    if (acosAngle <= unk1c)
        return 1.0f;
    return 0.0f;
}

BoneAngleRangePoseElement::BoneAngleRangePoseElement(
    SkeletonBone bone, const Vector3 &v, float f1, float f2
)
    : PoseElement(f2), mBone(bone), unk1c(f1) {
    Normalize(v, mAngle);
}
