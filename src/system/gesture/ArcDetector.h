#pragma once
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"
#include "math/Vec.h"
#include "math/Color.h"
#include "rndobj/Overlay.h"

class ArcDetector {
public:
    ArcDetector();
    virtual ~ArcDetector();

    void Clear();
    void Initialize(SkeletonSide, SkeletonJoint, SkeletonJoint, float);
    void PrintJointPath() const;
    float GetSwipeAmount() const;
    void ResetHoverTimer();
    bool IsLockedIn() const;
    float UpdateOverlay(RndOverlay *, float);
    void Update(const Skeleton &, int);
    void Draw(const Skeleton &, SkeletonViz &);
    int NumJointsInPath() const { return mJointPath.size(); }

private:
    Vector3 GetCurveStart() const;
    void TryToStartSwipe(const Vector3 &, const Skeleton &);
    float GetPathLength() const;
    float GetPathError() const;
    bool IsPathAcceptable() const;
    void SwipeFailed(const Skeleton &);
    void CullPath();
    void DrawPath(const std::list<Vector3> &, class SkeletonViz &, class Hmx::Color, const Vector3 &) const;

    static float _swipeRetentionFactor;
    static float _acceptablePathErrorRatio;

    SkeletonSide mSide; // 0x4
    SkeletonJoint mPrimaryJoint; // 0x8
    SkeletonJoint mSecondaryJoint; // 0xc
    std::list<Vector3> mJointPath; // 0x10
    Vector3 mArcOffset; // 0x18
    float mSwipeExtentX; // 0x28
    float mSwipeExtentY; // 0x2c
    float mSwipeThreshold; // 0x30
    bool mInitialized; // 0x34
    bool mHadProgress; // 0x35
    float mCurrentSwipeAmt; // 0x38
    int mHoverTimer; // 0x3c
    Vector3 unk40;
};
