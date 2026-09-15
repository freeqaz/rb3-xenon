// Ported from the rb3-Wii oracle (../rb3/src/system/dsp/PitchDetector.cpp).
// dc3-decomp has no dsp/ equivalent, so Wii is the correct provenance here
// rather than the usual engine rule.
//
// Two deliberate divergences from the oracle, both forced by retail bytes:
//
//  1. ALLOCATOR SPELLING.  The Wii file calls `_MemAlloc` / `_MemFree`.  Those
//     are MWCC phantoms on X360 -- see the census in utl/MemMgr.h: across all
//     396 pinned target objs `?_MemAlloc@@` and `?_MemFree@@` appear ZERO
//     times.  Retail's own bodies here agree: 0x82B807C0 calls
//     `?MemFree@@YAXPAX@Z` and 0x82B80FC8 calls `?MemAlloc@@YAPAXHH@Z`.  So
//     the match build uses the 2-arg MemAlloc(size, align) / 1-arg MemFree(p).
//
//  2. ShiftedDotProduct's RETURN TYPE.  The Wii file forward-declares it
//     `float`; our own dsp/SndAnalysis.cpp (in-tree, compiled, and the
//     authority) defines it `void`.  The in-tree record outranks the oracle.
#include "dsp/PitchDetector.h"
#include "dsp/IIRFilter.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <math.h>
#include <string.h>

// Defined in dsp/SndAnalysis.cpp; there is no SndAnalysis.h, so these are
// forward-declared here exactly as the oracle does -- but with OUR signatures.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool fast);
int FindCCPeak(const float *dp_data, const float *ss_data, int vlen, int startPeriod);
float RefinePeriod2(
    const float *buf, const float *autocorr, const float *dp, int vlen, int period
);

// Retail calls an anonymous-namespace helper here where the Wii source inlines
// `1.0f - exp(-1.0f / (t * rate))`. AnalyzeBlock's call site is 0x82B80D54 ->
// 0x82B6EA08 = ?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z, and AnalyzeBlock contains NO
// exp call at all (its only libm call is log10 at 0x82B80E90). 0x82B6EA08 lies
// far outside this TU's .text (0x82B807C0-0x82B81228), so it is an ICF fold
// survivor: the same anon-namespace helper is defined in several TUs and the
// surviving copy's name is whichever TU owns that address.
//
// STATUS: this spelling is currently INERT -- measured Delta 0, with
// AnalyzeBlock byte-unchanged at fuzzy 85.723595 before and after, because
// /O1 /Ob2 inlines the helper straight back where retail emits a real `bl`.
// It is kept because it records WHERE the call belongs; it does not yet
// reproduce it. The next experiment is to establish why retail did not inline
// it -- the anon-namespace hash ?A0xa7b3dd7d is NOT this TU's, so the surviving
// definition lives in another TU, and the retail source may reach it through a
// shared declaration rather than defining it here.
namespace {
    float Time2IirA(float time, float rate) { return exp(-1.0f / (time * rate)); }
}

void dump(float *data, int len) {
    const char *space = " ";
    for (int i = 0.0f; len > i; i++) {
        int n = (int)(data[i] / 700.0f) + 30;
        int j = 0;
        if (n > 0) {
            do {
                FormatString fs(space);
                TheDebug << fs.Str();
                j++;
            } while (j < n);
        }
        TheDebug << MakeString("* %d\n", i);
    }
}

PitchDetector::PitchDetector(int sampleRate) {
    mSamplesPerSec = 0;
    unk14 = 0;
    unk18 = 0;
    mDecimBuf = 0;
    mCorrBuf = 0;
    mPeakBuf = 0;
    mAveEnergy = 0.0f;
    unk38 = 5.0f;
    mEnablePitchDetection = true;
    unk40 = 0;
    unk44 = 1.0f;
    unk48 = 0.0f;
    unk4C = -1.0f;
    SetSampleRate(sampleRate);
    float b[5] = { 0.046581834f, 0.186327335f, 0.279491007f, 0.186327335f, 0.046581834f };
    float a[5] = { 1.0f, -0.781814635f, 0.680165708f, -0.182484567f, 0.030120272f };
    mFilter = new IIR4PoleFilter(b, a);
}

PitchDetector::~PitchDetector() {
    Deallocate();
    delete mFilter;
}

void PitchDetector::Deallocate() {
    MemFree(mDecimBuf);
    MemFree(mCorrBuf);
    MemFree(mPeakBuf);
}

void PitchDetector::AnalyzeBlock(
    const char *label,
    short *samples,
    int numSamples,
    float gain,
    float pitchHint,
    float &pitchOut,
    float &confidenceOut,
    float &gateOut
) {
    static DataNode &PD_FLOOR_BOTTOM = DataVariable("PD_FLOOR_BOTTOM");
    static DataNode &PD_FLOOR_RATIO = DataVariable("PD_FLOOR_RATIO");
    static DataNode &PD_FLOOR_TOP = DataVariable("PD_FLOOR_TOP");
    static DataNode &PD_GATE_RATIO = DataVariable("PD_GATE_RATIO");
    static DataNode &PD_FLOOR_SECONDS = DataVariable("PD_FLOOR_SECONDS");
    static DataNode &PD_FIXED_GAIN = DataVariable("PD_FIXED_GAIN");
    static float kPropFilter = 0.3f;
    static int sDump = 0;

    int offset = (mDecimRate - mIdx) - (mDecimRate - mIdx) / mDecimRate * mDecimRate;
    int dec_size = (numSamples - offset - 1) / mDecimRate + 1;
    if (numSamples == 0 || numSamples == offset) {
        dec_size = 0;
    }

    float lastVal = 0.0f;
    int ixDecim = 0;
    if (dec_size != 0 && dec_size < mFrameSize) {
        int overlap = mFrameSize - dec_size;
        memcpy(mDecimBuf, mDecimBuf + dec_size, overlap * 4);
        lastVal = mCorrBuf[dec_size - 1];
        if (overlap > 0) {
            for (int i = 0; i < overlap; i++) {
                mCorrBuf[i] = mCorrBuf[i + dec_size] - lastVal;
            }
        }
        MILO_ASSERT(overlap > 0, 0xBA);
        lastVal = mCorrBuf[overlap - 1];
    }

    if (dec_size > mFrameSize) {
        int extra = (dec_size - mFrameSize) * mDecimRate;
        dec_size = mFrameSize;
        numSamples -= extra;
        samples += extra;
    }
    int begIxDecim = ixDecim;

    mFilter->Begin();
    float decimAccum = gain * mFilter->FilterSlow((float)samples[0]);
    int writeOff = ixDecim * 4;
    int sampleIdx = 0;
    while (sampleIdx < numSamples) {
        decimAccum =
            kPropFilter * (mFilter->FilterSlow((float)samples[0]) * gain - decimAccum)
            + decimAccum;
        if (ixDecim < mFrameSize && ((sampleIdx + mIdx) % mDecimRate) == 0) {
            ixDecim++;
            *((float *)((char *)mDecimBuf + writeOff)) = decimAccum;
            float sq = decimAccum * decimAccum + lastVal;
            *((float *)((char *)mCorrBuf + writeOff)) = sq;
            lastVal = *((float *)((char *)mCorrBuf + writeOff));
            writeOff += 4;
        }
        samples += 1;
        sampleIdx += 1;
    }
    mFilter->End();
    if (sDump) {
        dump(mDecimBuf, 192);
    }

    MILO_ASSERT(ixDecim - begIxDecim == dec_size, 0x102);

    int newIdx = (numSamples + mIdx);
    mIdx = newIdx - (newIdx / mDecimRate) * mDecimRate;
    float level = sqrt(mCorrBuf[mFrameSize - 1]);
    mAveEnergy = (float)level / ((float)(mFrameSize) - 0.0f);
    if (mEnablePitchDetection) {
        ShiftedDotProduct(mDecimBuf, mFrameSize, mPeakBuf, true);
        int period = FindCCPeak(mPeakBuf, mCorrBuf, mFrameSize, mMaxPeriod);
        mPeriod = RefinePeriod2(mDecimBuf, mCorrBuf, mPeakBuf, mFrameSize, period);
    }

    float floorRatio = 1.1f;
    if (PD_FLOOR_RATIO.Float(NULL) > 0.0f) {
        floorRatio = PD_FLOOR_RATIO.Float(NULL);
    }
    float floorBottom = 0.06f;
    if (PD_FLOOR_BOTTOM.Float(NULL) > 0.0f) {
        floorBottom = PD_FLOOR_BOTTOM.Float(NULL);
    }
    float floorTop = 5.0f;
    if (PD_FLOOR_TOP.Float(NULL) > 0.0f) {
        floorTop = PD_FLOOR_TOP.Float(NULL);
    }
    float gateRatio = 2.0f;
    if (PD_GATE_RATIO.Float(NULL) > 0.0f) {
        gateRatio = PD_GATE_RATIO.Float(NULL);
    }
    float fixedGain = 12.0f;
    if (PD_FIXED_GAIN.Float(NULL) > 0.0f) {
        fixedGain = PD_FIXED_GAIN.Float(NULL);
    }
    float floorSeconds = 10.0f;
    if (PD_FLOOR_SECONDS.Float(NULL) > 0.0f) {
        floorSeconds = PD_FLOOR_SECONDS.Float(NULL);
    }

    if (floorSeconds != unk4C) {
        float alpha;
        if (floorSeconds > 0.0f) {
            alpha = 1.0f - Time2IirA(floorSeconds, 60.0f);
        } else {
            alpha = 1.0f;
        }
        unk48 = alpha;
        unk4C = floorSeconds;
    }

    if ((unsigned)unk14 > 60) {
        float level = mAveEnergy;
        float candidate;
        if ((level > 0.0f) && (candidate = level * floorRatio, candidate < unk38)) {
            unk38 = candidate;
        } else if (mPeriod == 0.0f || level < unk38 * gateRatio) {
            float floorVal = unk38;
            unk38 = unk48 * (floorTop - floorVal) + floorVal;
        }
    }

    float *floorBottomPtr;
    if (floorBottom < unk38) {
        floorBottomPtr = &unk38;
    } else {
        floorBottomPtr = &floorBottom;
    }
    unk38 = *floorBottomPtr;
    float *floorTopPtr;
    if (unk38 < floorTop) {
        floorTopPtr = &unk38;
    } else {
        floorTopPtr = &floorTop;
    }
    unk38 = *floorTopPtr;

    if (mAveEnergy < unk38 * gateRatio) {
        mPeriod = 0.0f;
        mAveEnergy = 0.0f;
    } else if (mAveEnergy > 30.0f) {
        mAveEnergy = 30.0f;
    }

    if (mPeriod == 0.0f) {
        mPitch = 0.0f;
    } else {
        float pitchHz = (float)(mSamplesPerSec / mDecimRate) / mPeriod;
        if (pitchHz <= 0.0f) {
            confidenceOut = 0.0f;
            pitchOut = 0.0f;
            mPitch = 0.0f;
            mPeriod = 0.0f;
            return;
        }
        mPitch = 39.863136f + -36.376316f * (float)log10(pitchHz);
    }
    unk14++;
    unk18 += numSamples;
    pitchOut = mPitch;
    confidenceOut = fixedGain * (pitchHint * mAveEnergy) / unk38;
    gateOut = mAveEnergy;
}

void PitchDetector::SetSampleRate(int sampleRate) {
    if (mSamplesPerSec != sampleRate) {
        mSamplesPerSec = sampleRate;
        MILO_ASSERT(mSamplesPerSec, 0x1B2);
        mDecimRate = sampleRate / 6000;
        int decimated = mSamplesPerSec / mDecimRate;
        mMaxPeriod = decimated / 1320;
        mFrameSize = (((decimated / 65) * 2) + 15) & ~15;
        Deallocate();
        // Parenthesized to BYPASS utl/MemMgr.h's macro
        //   #define MemAlloc(size, file, line, name, ...) (MemAlloc)((size), 0)
        // which MSVC's permissive preprocessor expands even for this 2-arg
        // call and FORCES align to 0. Retail passes 16: the three call sites
        // below are `li r4,0x10` at 0x82B81050/0x82B8106C/0x82B81088, and the
        // unparenthesized form emitted `li r4,0` at all three -- the only
        // non-relocation difference in this body.
        mDecimBuf = (float *)(MemAlloc)(mFrameSize * 4, 0x10);
        mCorrBuf = (float *)(MemAlloc)(mFrameSize * 4, 0x10);
        mPeakBuf = (float *)(MemAlloc)(mFrameSize * 4, 0x10);
        memset(mDecimBuf, 0, mFrameSize * 4);
        memset(mCorrBuf, 0, mFrameSize * 4);
        memset(mPeakBuf, 0, mFrameSize * 4);
        mIdx = 0;
    }
}
