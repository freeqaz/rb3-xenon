#pragma once
#include "CharCameraInput.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonHistory.h"
#include "hamobj/HamCharacter.h"
#include "math/Mtx.h"

class MocapSkeletonIterator : public SkeletonHistoryArchive, public SkeletonHistory {
public:
    MocapSkeletonIterator(int, int);
    ~MocapSkeletonIterator();
    virtual bool PrevSkeleton(const Skeleton &, int, ArchiveSkeleton &, int &) const;

    operator bool();
    void operator++();

    int CurrentFrame() const { return mCurrentFrame; }

private:
    void Update();

    HamCharacter *mDancer; // 0x4c
    CharCameraInput mInput; // 0x50
    int mStartFrame; // 0x24b0
    int mEndFrame; // 0x24b4
    int mCurrentFrame;
    float mPrevFrame;
    int unk24c0;
    Skeleton mSkeleton; // 0x24c4
    float mSavedSeconds; // 0x2f98
    Transform mSavedXfm; // 0x2f9c
};
