#pragma once

class NgStats {
public:
    int mFaces; // 0x0
    int mParts; // 0x4
    int mPartSys; // 0x8
    int mRegMeshes; // 0xc
    int mMutMeshes; // 0x10
    int mBones; // 0x14
    int mMats; // 0x18
    int mCams; // 0x1c
    int mLightsReal; // 0x20
    int mLightsApprox; // 0x24
    int mMultiMeshInsts; // 0x28
    int mMultiMeshBatches; // 0x2c
    int mFlares; // 0x30
    int mMotionBlurs; // 0x34
    int mSpotlights; // 0x38
};

extern NgStats *TheNgStats;
