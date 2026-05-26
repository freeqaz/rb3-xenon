#pragma once

#include "xdk/xaudio2/xaudio2.h"

// Maximum number of audio channels supported
#define EQ_MAX_CHANS 2

// Number of crossover filter stages (low, band, high)
#define EQ_NUM_XOVER_STAGES 3

// Number of passes per crossover stage (doubled for 4th order)
#define EQ_NUM_XOVER_PASSES 2

// Number of delay taps per crossover sub-stage
#define EQ_NUM_XOVER_TAPS 5

// Total delay floats per channel per delay type
#define EQ_XOVER_DELAY_PER_CHAN \
    (EQ_NUM_XOVER_STAGES * EQ_NUM_XOVER_PASSES * EQ_NUM_XOVER_TAPS) // 30

// Maximum crossover filter coefficients per stage
#define EQ_XOVER_MAX_COEFFS 4

class EQEffect {
public:
    struct Params {
        u32 mActiveBands;
        float mBand1Freq;
        float mBand1Gain;
        float mBand1Q;
        float mBand2Freq;
        float mBand2Gain;
        float mBand2Q;
        float mBand3Freq;
        float mBand3Gain;
        float mBand3Q;
        float mBand4Freq;
        float mBand4Gain;
        float mBand4Q;
        float mBand5Freq;
        float mBand5Q;
    };

    EQEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameter(int, float);
    void SetParameters(EQEffect::Params const &);

    // Band parameters (fed from SetParameter)
    float mBand1Freq;       // 0x00
    float mBand1Gain;       // 0x04
    float mBand1Q;          // 0x08
    float mBand2Freq;       // 0x0C
    float mBand2Gain;       // 0x10
    float mBand2Q;          // 0x14
    float mBand3Freq;       // 0x18  (note: semantically band3 gain in SetParameter case 6)
    float mBand3Gain;       // 0x1C  (note: semantically band3 freq in SetParameter case 7)
    float mBand3Q;          // 0x20
    float mBand4Freq;       // 0x24
    float mBand4Gain;       // 0x28
    float mBand4Q;          // 0x2C
    float mBand5Freq;       // 0x30

    float mSmoothCoeff;     // 0x34  fade/smoothing coefficient

    // Band 0: Low shelf filter
    bool mBand0Enabled;     // 0x38
    float mBand0B0;         // 0x3C  tan(freq)
    float mBand0B1;         // 0x40  gain target
    float mBand0B2;         // 0x44  gain current (smoothed)
    float mBand0A1;         // 0x48  shelf target
    float mBand0A2;         // 0x4C  shelf current (smoothed)
    float mBand0Z1;         // 0x50  allpass coefficient

    // Band 1: Bell/peaking filter
    bool mBand1Enabled;     // 0x54
    float mBand1B0;         // 0x58  allpass coefficient
    float mBand1B1;         // 0x5C  tan(freq)
    float mBand1B2;         // 0x60  gain target
    float mBand1A1;         // 0x64  gain current (smoothed)
    float mBand1A2;         // 0x68  shelf target
    float mBand1Z1;         // 0x6C  shelf current (smoothed)
    float mBand1Z2;         // 0x70  cos coeff * (1 - allpass)

    // Band 2: High shelf filter
    bool mBand2Enabled;     // 0x74
    float mBand2B0;         // 0x78  tan(freq)
    float mBand2B1;         // 0x7C  gain target
    float mBand2B2;         // 0x80  gain current (smoothed)
    float mBand2A1;         // 0x84  shelf target
    float mBand2A2;         // 0x88  shelf current (smoothed)
    float mBand2Z1;         // 0x8C  allpass coefficient

    // Band 3: Bandpass filter 1
    bool mBand3Enabled;     // 0x90
    float mBand3B0;         // 0x94  b0
    float mBand3B1;         // 0x98  b1
    float mBand3B2;         // 0x9C  b2
    float mBand3A1;         // 0xA0  a1
    float mBand3A2;         // 0xA4  a2

    // Band 4: Bandpass filter 2
    bool mBand4Enabled;     // 0xA8
    float mBand4B0;         // 0xAC  b0
    float mBand4B1;         // 0xB0  b1
    float mBand4B2;         // 0xB4  b2
    float mBand4A1;         // 0xB8  a1
    float mBand4A2;         // 0xBC  a2

    // Per-channel biquad filter delay state (max 2 channels)
    // Band 1 bell filter delay lines (stride 4 per channel)
    float mBand1DelayXn[EQ_MAX_CHANS];     // 0xC0  x[n-2]
    float mBand1DelayXn1[EQ_MAX_CHANS];    // 0xC8  x[n-1]
    float mBand1DelayZ[EQ_MAX_CHANS];      // 0xD0  z[n]
    float mBand1DelayZ1[EQ_MAX_CHANS];     // 0xD8  z[n-1]
    // Band 0/2 shelf allpass delay (stride 4 per channel)
    float mBand0DelayZ1[EQ_MAX_CHANS];     // 0xE0  band0 z1
    float mBand2DelayZ1[EQ_MAX_CHANS];     // 0xE8  band2 z1
    // Band 3/4 bandpass delay (stride 8 per channel: [chan][tap])
    float mBand3DelayX[EQ_MAX_CHANS][2];   // 0xF0  x[n], x[n-1]
    float mBand3DelayZ[EQ_MAX_CHANS][2];   // 0x100 z[n], z[n-1]
    float mBand4DelayX[EQ_MAX_CHANS][2];   // 0x110 x[n], x[n-1]
    float mBand4DelayZ[EQ_MAX_CHANS][2];   // 0x120 z[n], z[n-1]

    // Crossover filter coefficients
    float mXoverGain[EQ_NUM_XOVER_STAGES];                         // 0x130 gain reciprocals
    float mXoverCoeffs[EQ_NUM_XOVER_STAGES][EQ_XOVER_MAX_COEFFS];  // 0x13C filter coefficients

    // Per-channel crossover delay lines
    float mXoverInputDelay[EQ_MAX_CHANS][EQ_XOVER_DELAY_PER_CHAN];  // 0x16C
    float mXoverOutputDelay[EQ_MAX_CHANS][EQ_XOVER_DELAY_PER_CHAN]; // 0x25C
    // End: 0x34C
};
