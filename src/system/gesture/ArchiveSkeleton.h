#pragma once
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "os/Debug.h"

class ArchiveSkeleton : public BaseSkeleton {
public:
    ArchiveSkeleton();
    virtual ~ArchiveSkeleton() {}
    virtual void JointPos(SkeletonCoordSys, SkeletonJoint, Vector3 &) const; // 0x4
    virtual bool
    Displacement(const SkeletonHistory *, SkeletonCoordSys, SkeletonJoint, int, Vector3 &, int &)
        const {
        MILO_FAIL("ArchiveSkeleton::Displacement called");
        return false;
    }
    virtual bool
    Displacements(const SkeletonHistory *, SkeletonCoordSys, int, Vector3 *, int &) const {
        MILO_FAIL("ArchiveSkeleton::Displacements called");
        return false;
    }
    virtual JointConfidence JointConf(SkeletonJoint) const; // 0x10
    virtual bool IsTracked() const { return mTracked; }
    virtual int QualityFlags() const { return mQualityFlags; }
    virtual int ElapsedMs() const { return mElapsedMs; }
    virtual void CameraToPlayerXfm(SkeletonCoordSys, Transform &) const;

    void Set(const Skeleton &);

protected:
    PaddedJointPos mJointPoses[kNumJoints]; // 0x4
    JointConfidence mJointConfs[kNumJoints]; // 0x144
    int mElapsedMs; // 0x194
    bool mTracked; // 0x198
    int mQualityFlags; // 0x19c
};
