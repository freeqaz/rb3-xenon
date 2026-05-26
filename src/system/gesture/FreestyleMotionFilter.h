#pragma once
#include "gesture/Skeleton.h"
#include "obj/Object.h"

class FreestyleMotionFilter : public Hmx::Object {
public:
    // Hmx::Object
    virtual ~FreestyleMotionFilter();
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    FreestyleMotionFilter();
    void Clear();
    void Activate();
    void Deactivate();
    bool IsActive() const;
    bool Detected();
    void UpdateFilters(SkeletonUpdateData const &);

    float mVelocityThreshold;
    float mMoveTime;
    float mMovementAmount;
    bool mIsActive; // 0x38
};
