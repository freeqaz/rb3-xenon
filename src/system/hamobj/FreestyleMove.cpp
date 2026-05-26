#include "hamobj/FreestyleMove.h"
#include "gesture/BaseSkeleton.h"
#include "hamobj/DancerSkeleton.h"
#include <string.h>

FreestyleMove::FreestyleMove() : mDepthFrames(0), mNumFrames(0), unk10(0), unk14(0), mFrames(0) {}

FreestyleMove::~FreestyleMove() {
    delete[] mDepthFrames;
    delete[] mFrames;
}

void FreestyleMove::Clear() { mNumFrames = 0; }

void FreestyleMove::Free() {
    mNumFrames = 0;
    delete[] mDepthFrames;
    delete[] mFrames;
    mDepthFrames = nullptr;
    mFrames = nullptr;
}

void FreestyleMove::Init(int i1) {
    mNumFrames = 0;
    if (!mDepthFrames) {
        mDepthFrames = new DepthFrame[i1];
    }
    if (!mFrames) {
        mFrames = new FreestyleMoveFrame[i1];
    }
}

void FreestyleMove::CalcCentering(int frameIdx) {
    unsigned char *depth = (unsigned char *)&mDepthFrames[frameIdx];
    float totalVal = 0.0f;
    int totalCount = 0;
    int histogram[80];
    memset(histogram, 0, sizeof(histogram));
    for (int col = 0; col < 80; col++) {
        unsigned char *ptr = depth + col;
        for (int row = 0; row < 60; row++) {
            unsigned char val = *ptr;
            if (val != 0) {
                totalCount++;
                histogram[col]++;
                totalVal += (float)val;
            }
            ptr += 80;
        }
    }
    unk14 = (int)(totalVal / (float)totalCount);
    int weightedSum = 0;
    int pixelCount = 0;
    for (int i = 0; i < 80; i++) {
        pixelCount += histogram[i];
        weightedSum += histogram[i] * i;
    }
    int center = 0;
    if (weightedSum != 0) {
        center = weightedSum / pixelCount;
    }
    unk10 = center - 40;
}

void FreestyleMove::RecordSkeletonFrame(BaseSkeleton *skeleton, int i2, float f3) {
    FreestyleMoveFrame frame;
    frame.skeleton.Init();
    frame.mBeat = f3;
    if (skeleton && skeleton->IsTracked()) {
        frame.skeleton.Set(*skeleton);
    }
    mFrames[i2] = frame;
}
