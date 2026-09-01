#pragma once
#include "rndobj/Dir.h"
#include "rndobj/Anim.h"
#include "rndobj/Text.h"
#include "beatmatch/RGState.h"

// size 0x34
class FingerShape {
public:
    FingerShape(RndDir *);
    ~FingerShape();
    void Update(const RGState &, bool, bool);
    void UpdateLeftyFlip(bool);
    void Reset(bool);
    void UpdateAnim(RndAnimatable *, float, bool);
    void UpdateFretNumber(const RGState &, bool);

    std::vector<RndAnimatable *> mFretHeightAnims; // 0x0
    std::vector<RndAnimatable *> mContourHeightAnims; // 0xc
    std::vector<RndAnimatable *> mContourAngleAnims; // 0x18
    RGState *mLastState; // 0x24
    RndAnimatable *mFretNumberShowAnim; // 0x28
    RndAnimatable *mFretNumberPositionAnim; // 0x2c
    RndText *mFretNumberText; // 0x30
    int mLastFretNumber; // 0x34
    float mAnimPeriod; // 0x38
    bool mLefty; // 0x3c
};