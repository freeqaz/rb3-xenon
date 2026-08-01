#pragma once
#include "obj/Object.h"
#include "rndobj/Trans.h"

class WorldDir;

class FreeCamera : public Hmx::Object {
public:
    FreeCamera(WorldDir *, float, float, int);
    virtual ~FreeCamera() {}
    virtual DataNode Handle(DataArray *, bool);

    void Poll();
    void SetParentDof(bool b1, bool b2, bool b3);
    void SetPadNum(int p) { mPadNum = p; }

protected:
    void UpdateFromCamera();

    // Offsets below are compiler-verified (scripts/harvest/class_layout_report.py
    // FreeCamera). The previous comments were uniformly +4 stale.
    RndTransformable *mParent; // 0x28
    Vector3 mRot; // 0x2c
    Transform mXfm; // 0x3c
    float mFov; // 0x7c
    bool mFrozen; // 0x80
    int mPadNum; // 0x84
    float mRotateRate; // 0x88
    float mSlewRate; // 0x8c
    float mFocalPlane; // 0x90
    bool mUseParentRotateX; // 0x94
    bool mUseParentRotateY; // 0x95
    bool mUseParentRotateZ; // 0x96
    WorldDir *mWorld; // 0x98
    // RB3-360 retail only (absent from DC3 and from the rb3-Wii decomp): the
    // `enable_depth_of_field` handler writes a bool here, and Poll() gates its
    // TheDOFProc block on it (retail `lbz r11,0x9c(r31)` with r31 == this,
    // the same reg used for `lwz r10,0x98(r31)` == mWorld).
    bool mEnableDOF; // 0x9c
};
