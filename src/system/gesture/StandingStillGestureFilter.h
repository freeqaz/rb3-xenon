#pragma once
#include "gesture/Skeleton.h"
#include "obj/Data.h"
#include "obj/Object.h"

class StandingStillGestureFilter : public Hmx::Object {
public:
    StandingStillGestureFilter();
    virtual ~StandingStillGestureFilter();
    OBJ_CLASSNAME(StandingStillGestureFilter)
    OBJ_SET_TYPE(StandingStillGestureFilter)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    friend class SkeletonChooser;

    void SetForwardFacingCutoff(float);
    void RestoreDefaultForwardFacingCutoff();
    void Update(const Skeleton &, int);
    void Update(int, int);
    void Clear();
    void SetRequiredMs(int ms) { mRequiredMs = ms; }
    void SetRaisedMs(int ms) { mRaisedMs = ms; }
    void SetUnk48(bool b) { unk48 = b; }

    void SetStandingStill(bool b) { mStandingStill = b; }
    bool StandingStill() const { return mStandingStill; }
    int RaisedMs() const { return mRaisedMs; }

private:
    bool mStandingStill; // 0x2c
    int mRaisedMs; // 0x30
    int mRequiredMs; // 0x34
    Vector3 unk38; // 0x38
    float mForwardFacingCutoff; // 0x44
    bool unk48;
};
