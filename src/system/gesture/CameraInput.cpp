#include "gesture/CameraInput.h"
#include "gesture/Skeleton.h"

void CameraInput::PollTracking() {
    mNewFrame = PollNewFrame();
    if (mNewFrame) {
        memcpy(&mCachedFrame, mNewFrame, sizeof(SkeletonFrame));
    }
}

CameraInput::CameraInput() : mNewFrame(nullptr) {
    memset(&mCachedFrame, 0, sizeof(SkeletonFrame));
}

CameraInput::~CameraInput() {}

const SkeletonFrame *CameraInput::NewFrame() const { return mNewFrame; }
const SkeletonFrame &CameraInput::CachedFrame() const { return mCachedFrame; }
