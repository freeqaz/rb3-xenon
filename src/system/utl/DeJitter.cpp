#include "utl/DeJitter.h"
#include "obj/Data.h"

float DeJitter::sTimeScale = 1;

DeJitter::DeJitter() { Reset(); }

void DeJitter::Reset() {
    mCurrentIndex = 0;
    mHistoryCount = -2;
    mFilteredDelta = 0;
    mPreviousOutput = 0;
    for (int i = 0; i < 32; i++) {
        mHistoryBuffer[i] = 0;
    }
}

float DeJitter::NewMs(float f1, float &fref) {
    float filteredValue = 1.0000000150474662e+30;
    // Ring buffer indices (0-31 wrapping): prevPos is previous write position, historyPos is the
    // position mHistoryCount steps back (for averaging interval)
    float sample = f1;
    static DataNode &n = DataVariable("dejitter_disable"); // FLT_MAX-like sentinel for uninitialized result
    int prevPos = (mCurrentIndex - 1) & 0x1F;
    int historyPos = (prevPos - mHistoryCount) & 0x1F;

    // Only apply jitter correction if enabled and have accumulated enough samples
    if (!n.Int()) {
        if (mHistoryCount > 8) { // Need more than 8 samples in the history
            // Calculate average delta since mHistoryCount steps ago
            float f0 = (mHistoryBuffer[prevPos] - mHistoryBuffer[historyPos]) / (float)mHistoryCount;
            // Smooth the average with exponential moving average (alpha=0.1)
            if (mFilteredDelta == 0.0f) {
                mFilteredDelta = f0;
            }
            f0 = (f0 - mFilteredDelta) * 0.1f + mFilteredDelta;
            filteredValue = f0;
            mFilteredDelta = f0;
            if (sTimeScale != 1.0f) {
                // With time scale, output is scaled delta
                f0 = f0 * sTimeScale;
                mFilteredDelta = f0;
                filteredValue = f0 + mPreviousOutput;
            } else {
                // Without time scale, clamp output to ±33ms from previous value
                float f12 = mPreviousOutput + f0;
                float f11 = sample - 33.0f;
                float f13 = sample + 33.0f;
                float f10 = ((f11 - f12) >= 0.0f) ? f11 : f12;
                filteredValue = ((f10 - f13) >= 0.0f) ? f13 : f10;
            }
            // Don't let result go below previous output value
            if (filteredValue < mPreviousOutput) {
                filteredValue = mPreviousOutput;
            }
        }
    }

    // Store new sample in ring buffer
    mHistoryBuffer[mCurrentIndex] = sample;
    // Use computed jittered value if it was calculated, otherwise use raw input
    if (filteredValue != 1.0000000150474662e+30) {
        sample = filteredValue;
    }
    mCurrentIndex = (mCurrentIndex + 1) & 0x1F;

    // Output delta: on initialization (-2), use default frame time; otherwise use difference
    if (mHistoryCount == -2) {
        fref = 16.666f; // Default 60 FPS frame time
    } else {
        fref = sample - mPreviousOutput;
    }

    // Count up to stabilization threshold
    if (mHistoryCount < 30) {
        mHistoryCount = mHistoryCount + 1;
    }

    // Remember output for next iteration
    mPreviousOutput = sample;
    return sample;
}
