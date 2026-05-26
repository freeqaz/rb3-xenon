#pragma once
#include "math/Geo.h"
#include "math/Color.h"
#include <float.h>

class DebugGraph {
public:
    struct Sample {
        float data;
        bool b;
    };
    DebugGraph(
        float f1,
        float f2,
        float f3,
        float f4,
        Hmx::Color c1,
        Hmx::Color c2,
        int i1,
        float f5,
        float f6,
        String s
    )
        : mRect(f1, f2, f3, f4), mColorA(c1), mColorB(c2), mMaxSamples(i1), mMinValue(f5),
          mMaxValue(f6), mThresholdValue(FLT_MAX), mGraphName(s), mIsVisible(1) {}
    ~DebugGraph() {}
    void AddData(float, bool);
    void Draw();
    void SetIsVisible(bool b) { mIsVisible = b; }
    bool GetIsVisible() { return mIsVisible; }
    void SetThresholdValue(float f) { mThresholdValue = f; }
    int GetMaxSamples() { return mMaxSamples; }

protected:
    Hmx::Rect mRect; // 0x0
    Hmx::Color mColorA; // 0x10
    Hmx::Color mColorB; // 0x20
    std::list<Sample> mSamples; // 0x30
    int mMaxSamples; // 0x38
    float mMinValue; // 0x3c
    float mMaxValue; // 0x40
    float mThresholdValue; // 0x44
    String mGraphName; // 0x48
    bool mIsVisible; // 0x50
};
