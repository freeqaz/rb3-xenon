#include "utl/DeJitter.h"
#include "obj/Data.h"

float DeJitter::sTimeScale = 1;

DeJitter::DeJitter() {
    Reset();
    // RB3 stores the history ring in a heap-backed std::vector,
    // not an inline array; size it once at construction.
    mHistoryBuffer.resize(32);
}

void DeJitter::Reset() {
    mCurrentIndex = 0;
    mHistoryCount = -2;
    mFilteredDelta = 0;
    mPreviousOutput = 0;
}

float DeJitter::NewMs(float f1, float &fref) {
    // Retail TU5 shape: no time-scale path and no "dejitter_disable" check,
    // and the output is clamped to +/-16 ms of the raw sample.
    float filteredValue = 1.0000000150474662e+30;
    float sample = f1;

    if (mHistoryCount > 8) { // Need more than 8 samples in the history
        // Ring buffer indices (0-31 wrapping): prevPos is the previous write
        // position, historyPos is mHistoryCount steps back from it.
        int prevPos = (mCurrentIndex - 1) & 0x1F;
        int historyPos = (prevPos - mHistoryCount) & 0x1F;
        // Average delta since mHistoryCount steps ago
        float f0 = (mHistoryBuffer[prevPos] - mHistoryBuffer[historyPos]) / (float)mHistoryCount;
        // Smooth the average with an exponential moving average (alpha=0.1)
        if (mFilteredDelta == 0.0f) {
            mFilteredDelta = f0;
        }
        f0 = (f0 - mFilteredDelta) * 0.1f + mFilteredDelta;
        mFilteredDelta = f0;
        // Clamp the predicted output to +/-16ms of the sample
        float f12 = mPreviousOutput + f0;
        float f11 = sample - 16.0f;
        float f13 = sample + 16.0f;
        float f10 = ((f11 - f12) >= 0.0f) ? f11 : f12;
        filteredValue = ((f10 - f13) >= 0.0f) ? f13 : f10;
        // Don't let result go below previous output value
        if (filteredValue < mPreviousOutput) {
            filteredValue = mPreviousOutput;
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
