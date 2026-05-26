#pragma once

class DeJitter {
public:
    DeJitter();
    void Reset();
    float NewMs(float, float &);

    static float sTimeScale;

private:
    float mHistoryBuffer[0x20]; // 0x0
    int mCurrentIndex; // 0x80
    int mHistoryCount; // 0x84
    float mFilteredDelta; // 0x88
    float mPreviousOutput; // 0x8c
};
