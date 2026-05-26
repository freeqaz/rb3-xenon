#pragma once
#include "math/Mtx.h"
#include "obj/Object.h"
#include "gesture/Skeleton.h"

class CameraInput : public virtual Hmx::Object {
public:
    CameraInput();
    // Hmx::Object
    virtual ~CameraInput();
    // CameraInput
    virtual float DrawScale() const { return 1; }
    virtual bool NatalToWorld(Transform &) const { return false; }
    virtual bool IsConnected() const { return true; }
    virtual bool IsOverride() const { return true; }
    virtual void PollTracking();
    virtual const SkeletonFrame *PollNewFrame() = 0;

    const SkeletonFrame &CachedFrame() const; // returns 0xc
    const SkeletonFrame *NewFrame() const;

protected:
    const SkeletonFrame *mNewFrame; // 0x8
    SkeletonFrame mCachedFrame; // 0xc - size 0x11c8
};
