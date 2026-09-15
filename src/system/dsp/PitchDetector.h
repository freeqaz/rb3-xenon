#pragma once

class IIR4PoleFilter;

class PitchDetector {
public:
    PitchDetector(int sampleRate);
    ~PitchDetector();
    void AnalyzeBlock(
        const char *label,
        short *samples,
        int numSamples,
        float gain,
        float pitchHint,
        float &pitchOut,
        float &confidenceOut,
        float &gateOut
    );
    void SetSampleRate(int sampleRate);
    void Deallocate();

    IIR4PoleFilter *mFilter; // 0x00
    int mSamplesPerSec; // 0x04
    int mFrameSize; // 0x08
    int mDecimRate; // 0x0c
    int mMaxPeriod; // 0x10
    int unk14; // 0x14
    int unk18; // 0x18
    int mIdx; // 0x1c
    float *mDecimBuf; // 0x20
    float *mCorrBuf; // 0x24
    float *mPeakBuf; // 0x28
    float mPitch; // 0x2c  - Bank 5 DWARF: smoothed pitch (semitones)
    float mPeriod; // 0x30  - refined autocorrelation period (samples)
    float mAveEnergy; // 0x34  - Bank 5 DWARF: smoothed signal level (avg energy)
    float unk38; // 0x38  - 5.0f
    bool mEnablePitchDetection; // 0x3c
    int unk40; // 0x40
    float unk44; // 0x44  - 1.0f
    float unk48; // 0x48  - 0.0f (one-pole alpha)
    float unk4C; // 0x4c  - -1.0f (last floor seconds)
};
