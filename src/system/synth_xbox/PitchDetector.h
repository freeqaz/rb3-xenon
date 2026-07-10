#pragma once

#include "FftIpp.h"
#include "stlport/stl/_vector.h"
#include "utl/StlAlloc.h"
#include <vector>

namespace DSP {

class SpectralAnalysis {
public:
    ~SpectralAnalysis();

    void SetMode(unsigned int windowSize, unsigned int hop);
    void Analyze(const float *in, float *out);

    int mWindowSize;        // 0x00
    int unk4;               // 0x04
    int unk8;               // 0x08
    FftIpp mFft1;           // 0x0C
    FftIpp mFft2;           // 0x50
    std::vector<float, XboxAllocator<float> > mData0; // 0x94
    std::vector<float, XboxAllocator<float> > mData1; // 0xA0
    std::vector<float, XboxAllocator<float> > mData2; // 0xAC
    std::vector<float, XboxAllocator<float> > mData3; // 0xB8
    std::vector<float, XboxAllocator<float> > mData4; // 0xC4
    std::vector<float, XboxAllocator<float> > mData5; // 0xD0
};

namespace Synapse {

class PitchDetector {
public:
    PitchDetector(const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > &input,
                  unsigned int windowSize, unsigned int hop);
    ~PitchDetector();

    void Detect(unsigned int pos);

    const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > *mInput; // 0x00
    unsigned int mWindowSize;   // 0x04
    unsigned int mHop;          // 0x08
    float mFrequency;           // 0x0C
    float mConfidence;          // 0x10
    float mClarity;             // 0x14
    SpectralAnalysis mSpectral; // 0x18
    int mUnusedF4;              // 0xF4
    int mUnusedF8;              // 0xF8
    int mUnusedFC;              // 0xFC
    stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > mSpectrum; // 0x100
    stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > mWindow;   // 0x10C
    stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > mWeight;   // 0x118
};

} // namespace Synapse

} // namespace DSP
