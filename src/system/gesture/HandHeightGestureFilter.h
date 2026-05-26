#pragma once
#include "gesture/BaseSkeleton.h"
#include "gesture/Skeleton.h"

class HandHeightGestureFilter {
    friend class HamNavList;
public:
    HandHeightGestureFilter(SkeletonSide);
    virtual ~HandHeightGestureFilter() {}

    void Update(const Skeleton &, int);
    void Clear();
    float GetHandHeight() const { return mHandHeight; }

protected:
    SkeletonSide mSide; // 0x4
    float mHeightOffset; // 0x8
    int mTrackingState; // 0xc
    float mHandHeight; // 0x10
};
