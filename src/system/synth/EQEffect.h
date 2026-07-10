#pragma once

#include "xdk/xaudio2/xaudio2.h"

// RB3's EQEffect is the older, smaller EQ engine (sizeof 0x10C) — it predates
// DC3's crossover rework (DC3-era sizeof 0x34C: crossover gain/coeff/delay
// arrays, parameter smoothing members, SetParameter cases 11/12). Layout
// verified against retail asm: ctor fn_82B82B58 (defaults + five stb enable
// flags at 0x2C/0x40/0x58/0x6C/0x84), Reset fn_82B828E8 (zeros 0x9C..0x108),
// and the StandardEffect<EQEffect> ctor size immediate (was +0x240 with the
// DC3 layout; 0x34C - 0x10C = 0x240 exactly).
class EQEffect {
public:
    // size 0x30 (RB3: two band params fewer than DC3's 0x38)
    struct Params {
        bool unk0;
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
    };

    EQEffect(IXAudioBatchAllocator *);
    void Reset();
    void Process(float *, int, int);
    void SetParameter(int, float);
    void SetParameters(EQEffect::Params const &);

    // Band parameters (fed from SetParameter cases 0-10)
    float mBand1Freq; // 0x00 (default 12000)
    float mBand1Gain; // 0x04
    float mBand1Q;    // 0x08 (default 8000)
    float mBand2Freq; // 0x0C (default 1000)
    float mBand2Gain; // 0x10
    float mBand2Q;    // 0x14 (default 2000)
    float mBand3Freq; // 0x18
    float mBand3Gain; // 0x1C (default 20000)
    float mBand3Q;    // 0x20
    float mBand4Freq; // 0x24 (default 20)
    float mBand4Gain; // 0x28

    // Band 0: low shelf filter
    bool mBand0Enabled; // 0x2C
    float mBand0B0;     // 0x30 tan(freq)
    float mBand0Gain;   // 0x34 gain
    float mBand0Shelf;  // 0x38 shelf coefficient
    float mBand0Coeff;  // 0x3C allpass coefficient

    // Band 1: bell/peaking filter
    bool mBand1Enabled; // 0x40
    float mBand1B0;     // 0x44
    float mBand1B1;     // 0x48
    float mBand1B2;     // 0x4C
    float mBand1A1;     // 0x50
    float mBand1A2;     // 0x54

    // Band 2: high shelf filter
    bool mBand2Enabled; // 0x58
    float mBand2B0;     // 0x5C tan(freq)
    float mBand2Gain2;  // 0x60 gain
    float mBand2Shelf;  // 0x64 shelf coefficient
    float mBand2Coeff;  // 0x68 allpass coefficient

    // Band 3: bandpass filter 1
    bool mBand3Enabled; // 0x6C
    float mBand3B0;     // 0x70
    float mBand3B1;     // 0x74
    float mBand3B2;     // 0x78
    float mBand3A1;     // 0x7C
    float mBand3A2;     // 0x80

    // Band 4: bandpass filter 2
    bool mBand4Enabled; // 0x84
    float mBand4B0;     // 0x88
    float mBand4B1;     // 0x8C
    float mBand4B2;     // 0x90
    float mBand4A1;     // 0x94
    float mBand4A2;     // 0x98

    // Per-channel filter delay state (2 channels)
    float mDelayA[2];         // 0x9C
    float mDelayB[2];         // 0xA4
    float mDelayC[2];         // 0xAC
    float mDelayD[2];         // 0xB4
    float mDelayE[2];         // 0xBC
    float mDelayF[2];         // 0xC4
    float mBand3DelayX[2][2]; // 0xCC x[n], x[n-1]
    float mBand3DelayZ[2][2]; // 0xDC z[n], z[n-1]
    float mBand4DelayX[2][2]; // 0xEC x[n], x[n-1]
    float mBand4DelayZ[2][2]; // 0xFC z[n], z[n-1]
    // sizeof: 0x10C
};
