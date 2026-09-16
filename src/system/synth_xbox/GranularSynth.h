#pragma once
#ifndef HX_NATIVE

#include "../stlport/stl/_vector.h"
#include "utl/StlAlloc.h"

namespace DSP {
namespace Synapse {

// Granular resynthesiser: every Voice asks for a grain at its own rate, each
// grain reads a windowed span out of the shared circular input buffer and is
// mixed into that voice's output channel.
//
// Ported from ../dc3-decomp.  The layout is INDEPENDENTLY CONFIRMED ON RB3
// RETAIL BYTES: lane W12-D read 0x82b74600 (~GranularSynth) and recorded that
// it destroys a vector<vector<float>> at +0x38, then a buffer at +0x2c with
// element stride 0x18, then a buffer at +0x20 with element stride 0x40
// (srawi/slwi 6).  That is exactly mGranules@0x20 (Granule = 0x40),
// mVoices@0x2c (Voice = 0x18), mWindows@0x38 below, so this is not an
// inherited-from-DC3 assumption.  Our own Synapse_dsp.cpp models the same
// class at sizeof 0x44 with mVoices at 0x2C, which agrees a third time.
class GranularSynth {
public:
    typedef stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > FloatVec;

    // A pending/active grain.  Note the target's default ctor deliberately
    // leaves mFadeIn/mDelay/mLifetime uninitialised -- ExtractGranules writes
    // all three before the grain is ever marked active.
    struct Granule {
        Granule()
            : mActive(false), mOffset(0.0f), mLength(0.0f), mGain(0.0f),
              mStartTime(0.0), mNextTime(0.0), mVoice(0), mWindow(0),
              mWindowLen(0), mStartOffset(0), mPhase(0.0f) {}

        bool mActive;             // 0x00
        float mOffset;            // 0x04
        float mLength;            // 0x08 grain length in samples
        float mGain;              // 0x0c
        double mStartTime;        // 0x10
        double mNextTime;         // 0x18
        int mVoice;               // 0x20 index into mVoices / the output channel
        unsigned int mWindow;     // 0x24 index into mWindows
        unsigned int mWindowLen;  // 0x28
        int mStartOffset;         // 0x2c first sample of the block this grain writes
        unsigned int mFadeIn;     // 0x30 (uninitialised by the ctor, see above)
        int mDelay;               // 0x34
        int mLifetime;            // 0x38
        float mPhase;             // 0x3c read cursor into mInput
    };

    struct Voice {
        Voice() : mPan(0.0f), mGain(0.0f), mRate(1.0f), mActive(false), mNextTime(0.0) {}

        float mPan;          // 0x00
        float mGain;         // 0x04
        float mRate;         // 0x08
        bool mActive;        // 0x0c
        double mNextTime;    // 0x10
    };

    typedef stlpmtx_std::vector<Granule, stlpmtx_std::StlNodeAlloc<Granule> > GranuleVec;
    typedef stlpmtx_std::vector<Voice, stlpmtx_std::StlNodeAlloc<Voice> > VoiceVec;
    typedef stlpmtx_std::vector<FloatVec, stlpmtx_std::StlNodeAlloc<FloatVec> > WindowVec;

    GranularSynth(const FloatVec &input, unsigned int numVoices, unsigned int hop,
                  unsigned int maxLength);
    ~GranularSynth();

    void ExtractGranules();
    void Synthesize(unsigned int count, float *const *out);
    void Flush();

private:
    unsigned int Size() const { return (unsigned int)(mInput->end() - mInput->begin()); }

public:

    const FloatVec *mInput;  // 0x00
    float mHopF;             // 0x04 (float)mHop
    float mOffset;           // 0x08
    float mLengthMix;        // 0x0c blend between the two candidate grain lengths
    unsigned int mHop;       // 0x10
    unsigned int mMaxLength; // 0x14
    unsigned int mFrame;     // 0x18
    unsigned int mBlock;     // 0x1c
    GranuleVec mGranules;    // 0x20
    VoiceVec mVoices;        // 0x2c
    WindowVec mWindows;      // 0x38
};

} // namespace Synapse
} // namespace DSP

namespace stlpmtx_std {

// Forward declaration of _Vector_base constructor
template <>
_Vector_base<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>::_Vector_base(
    size_t __n,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>& __a
);

// Explicit specialization of vector constructor for Voice
template <>
vector<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>::vector(
    unsigned int __n,
    const DSP::Synapse::GranularSynth::Voice& __val,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>& __a
);

// Explicit specialization of vector constructor for Granule
template <>
vector<DSP::Synapse::GranularSynth::Granule, StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>>::vector(
    unsigned int __n,
    const DSP::Synapse::GranularSynth::Granule& __val,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>& __a
);

} // namespace stlpmtx_std

#endif // HX_NATIVE
