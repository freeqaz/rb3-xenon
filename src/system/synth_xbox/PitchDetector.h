#pragma once

#include "FftIpp.h"
#include <vector>

namespace DSP {

class SpectralAnalysis {
public:
    ~SpectralAnalysis();
    SpectralAnalysis();

    int unk0;               // 0x00
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

} // namespace DSP
