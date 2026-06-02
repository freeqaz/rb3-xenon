#pragma once
#include <vector>

// RB3-360 retail layout (verified vs the target binary, NOT dc3's). dc3 is
// *newer* and replaced RB3's heap-backed `std::vector<float>` history ring with
// an inline `float mHistoryBuffer[0x20]` (making DeJitter 0x90 bytes). RB3
// predates that change: rb3-Wii's DeJitter (utl/DeJitter.h) uses a
// `std::vector<float>` resized to 32 plus two ints and three floats, so the
// retail struct is small (vector 0xc + 2 ints + 3 floats = 0x20 on X360). The
// inline-array version pushed every GamePanel field past mDeJitter +0x8c
// (mDirectInstrument read 0x1dc vs the target's 0x150). Keeping the vector
// matches the target object layout.
class DeJitter {
public:
    DeJitter();
    void Reset();
    float NewMs(float, float &);

    static float sTimeScale;

private:
    std::vector<float> mHistoryBuffer; // 0x0 (resized to 32 in ctor; STLport vector = 0xc)
    int mCurrentIndex;  // 0xc (X360) — ring write position
    int mHistoryCount;  // 0x10 — accumulated-sample counter (starts -2)
    float mFilteredDelta; // 0x14 — EMA-smoothed delta
    float mPreviousOutput; // 0x18 — last emitted value
}; // total 0x1c on X360
