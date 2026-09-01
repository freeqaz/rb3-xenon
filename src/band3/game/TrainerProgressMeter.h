#pragma once
#include "math/Mtx.h"
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include <vector>

class TrainerProgressMeter {
public:
    TrainerProgressMeter();
    ~TrainerProgressMeter();
    void Init(RndDir *, int);
    void SetCompleted(int, bool);
    void SetCurrent(int);
    void Draw();
    void Hide();

    std::vector<bool> mCompleted; // 0x0
    RndDir *mProgressMeter; // 0x14
    RndMesh *mBar; // 0x18
    Transform mBarTrans; // 0x1c
    RndMesh *mGlow; // 0x5c
    Transform mGlowTrans; // 0x60
    RndMesh *mBoxes; // 0xa0
    int mCurrent; // 0xa4
};
