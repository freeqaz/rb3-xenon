#pragma once

#ifndef HX_NATIVE
#include "stlport/stl/_vector.h"
#else
#include <vector>
#endif
#include "PitchCorrectedVoice.h"
#include "scoped_ptr.h"

namespace DSP {

class Biquad;

namespace Synapse {

class PitchDetector;
class PeakDetector;
class GranularSynth;

class Synapse {
public:
    Synapse(float);
    ~Synapse();

    void ProcessInPlace(unsigned int numSamples, float *buffer);

    void SetVoiceTargetNote(unsigned int idx, float val);
    void SetVoiceGain(unsigned int idx, float val);
    void SetVoiceEnabled(unsigned int idx, bool enabled);
    void SetVoiceTransposition(unsigned int idx, float val);
    void SetVoiceAmount(unsigned int idx, float val);
    void SetVoiceProximityEffect(unsigned int idx, float val);
    void SetVoiceProximityFocus(unsigned int idx, float val);
    void SetAttackSmoothing(float val);
    void SetReleaseSmoothing(float val);

#ifdef HX_NATIVE
    std::vector<float> mInputBuffer;                     // 0x00
    std::vector<float> mDownsampledBuffer;               // 0x0C
#else
    stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > mInputBuffer;           // 0x00
    stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > mDownsampledBuffer;      // 0x0C
#endif
    unsigned int mBufferIndex;                                                               // 0x18
    unsigned int mDefaultPitch;                                                                // 0x1C
    unsigned int mField_0x20;                                                                  // 0x20
    unsigned int mDetectionInterval;                                                           // 0x24
    scoped_ptr<PitchDetector> mPitchDetector;                                                // 0x28
    float mDetectedPitch;                                                                    // 0x2C
    float mPitchConfidence;                                                                  // 0x30
    float mPitchClarity;                                                                     // 0x34
    float mPitchThreshold;                                                                   // 0x38
    float mGain;                                                                             // 0x3C
    scoped_ptr<PeakDetector> mPeakDetector;                                                  // 0x40
#ifdef HX_NATIVE
    std::vector<std::vector<float>> mChannelBuffers;     // 0x44
    std::vector<float *> mOutputBuffers;                 // 0x50
    std::vector<PitchCorrectedVoice> mVoices;            // 0x5C
#else
    stlpmtx_std::vector<stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> >, stlpmtx_std::StlNodeAlloc<stlpmtx_std::vector<float, stlpmtx_std::StlNodeAlloc<float> > > > mChannelBuffers; // 0x44
    stlpmtx_std::vector<float *, stlpmtx_std::StlNodeAlloc<float *> > mOutputBuffers;       // 0x50
    stlpmtx_std::vector<PitchCorrectedVoice, stlpmtx_std::StlNodeAlloc<PitchCorrectedVoice> > mVoices; // 0x5C
#endif
    scoped_ptr<GranularSynth> mGranularSynth;                                                // 0x68
    float mTargetPitch;                                                                      // 0x6C
    scoped_ptr<Biquad> mScratchBuffer1;                                                      // 0x70
    scoped_ptr<Biquad> mScratchBuffer2;                                                      // 0x74
    float mIirSmooth;                                                                        // 0x78
    float mIirCoeff;                                                                         // 0x7C
};

} // namespace Synapse
} // namespace DSP
