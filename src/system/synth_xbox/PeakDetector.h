#pragma once
#ifndef HX_NATIVE

#include "../stlport/stl/_vector.h"
#include "utl/StlAlloc.h"

namespace DSP {
namespace Synapse {

// Tracks the running envelope of a circular float buffer and emits one peak per
// "event": while a candidate is being tracked the loudest sample wins, and once
// the candidate is far enough behind the write head the peak position is refined
// by parabolic interpolation and published.
//
// Ported from ../dc3-decomp (same Milo engine, same /O1 /Oi /GR /EHsc flags).
// The layout below is DC3's; RB3's retail row sizes agree exactly (948 B Detect,
// 268 B gaussianWindow), which is the evidence the bodies are the same code.
class PeakDetector {
public:
    PeakDetector(const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > &input,
                 unsigned int windowSize, unsigned int hop);

    void Detect(unsigned int pos);

    const stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > *mInput; // 0x00
    float mWidth;               // 0x04 configured envelope width (samples)
    unsigned int mWindowSize;   // 0x08 minimum spacing between two peaks
    unsigned int mHop;          // 0x0c
    unsigned int mOrigin;       // 0x10 index the circular buffer is measured from
    unsigned int mPeakPos;      // 0x14 best candidate seen so far
    unsigned int mStartPos;     // 0x18 where the current candidate started
    unsigned int mLength;       // 0x1c length of the last published peak
    bool mTracking;             // 0x20 a candidate is currently being tracked
    float mCurWidth;            // 0x24 width in use for the current candidate
    float mCenter;              // 0x28 refined (fractional) peak position
    float mPeakValue;           // 0x2c windowed magnitude at mPeakPos
    float mNextCenter;          // 0x30 mCenter biased back by 0.4 * mCurWidth

private:
    unsigned int Size() const { return (unsigned int)(mInput->end() - mInput->begin()); }
    float gaussianWindow(unsigned int i) const;
};

} // namespace Synapse
} // namespace DSP

#endif // HX_NATIVE
