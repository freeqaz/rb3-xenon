// PitchDetector (system/dsp/PitchDetector.cpp).
// dc3-decomp has no dsp/ equivalent, so this is RB3's own code
// rather than the usual DC3 engine copy.
//
// Two deliberate details, both forced by retail bytes:
//
//  1. ALLOCATOR SPELLING.  Not `_MemAlloc` / `_MemFree`.  Those
//     are MWCC phantoms on X360 -- see the census in utl/MemMgr.h: across all
//     396 pinned target objs `?_MemAlloc@@` and `?_MemFree@@` appear ZERO
//     times.  Retail's own bodies here agree: 0x82B807C0 calls
//     `?MemFree@@YAXPAX@Z` and 0x82B80FC8 calls `?MemAlloc@@YAPAXHH@Z`.  So
//     the match build uses the 2-arg MemAlloc(size, align) / 1-arg MemFree(p).
//
//  2. ShiftedDotProduct's RETURN TYPE is `void`, not
//     `float`; our own dsp/SndAnalysis.cpp (in-tree, compiled, and the
//     authority) defines it `void`.
#include "dsp/PitchDetector.h"
#include "dsp/IIRFilter.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <math.h>
#include <string.h>

// The match build spells MemAlloc as retail's 2-arg (size, align); that
// overload exists only for X360 (utl/MemMgr.h), so native passes the 5-arg
// debug form. Token-identical to the old call sites on X360.
#ifdef HX_NATIVE
#define PD_MEM_ALLOC(size, align) MemAlloc((size), __FILE__, __LINE__, "PitchDetector", (align))
#else
#define PD_MEM_ALLOC(size, align) (MemAlloc)(size, align)
#endif

// Defined in dsp/SndAnalysis.cpp; there is no SndAnalysis.h, so these are
// forward-declared here -- with OUR signatures.
void ShiftedDotProduct(const float *buf, int len, float *ss, bool fast);
int FindCCPeak(const float *dp_data, const float *ss_data, int vlen, int startPeriod);
float RefinePeriod2(
    const float *buf, const float *autocorr, const float *dp, int vlen, int period
);

// Retail calls an anonymous-namespace helper here rather than inlining
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
    // The real retail body, read off 0x82B6EA08 (92 B) rather than guessed:
    //   fcmpu f1,0.0 / ble -> return 1.0
    //   fmuls f13,f1,f2      time * rate
    //   fdivs f1,-1.0,f13    -1.0 / (time * rate)
    //   bl exp (0x8282ED70)
    //   frsp f13,f1          narrow to float FIRST...
    //   fsubs f1,1.0,f13     ...then subtract in float -- hence the cast
    // The previous one-liner `return exp(-1.0f/(time*rate));` was missing both
    // the guard and the subtraction (the caller carried the `1.0f -`), and it
    // divided by zero when time == 0. It was also small and branch-free, which
    // is why /O1 /Ob2 inlined it straight back where retail emits a real `bl`.
    float Time2IirA(float time, float rate) {
        if (time > 0.0f) {
            return 1.0f - (float)exp(-1.0f / (time * rate));
        }
        return 1.0f;
    }
}

// DEV-BUILD ONLY -- retail does not have this. Measured, not assumed: the
// retail PitchDetector TU (0x82B807C0-0x82B81228) contains no `dump` body at
// all, and AnalyzeBlock's complete `bl` census over its own extent has no
// call to one. Guarded with the house pattern (see CLAUDE.md) so native
// keeps the tracer and the match build does not emit the call.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
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
#endif

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
    // Retail's coefficients (.rdata 0x8219B058..0x8219B070) are the 5-digit values.
    float b[5] = { 0.046583f, 0.18633f, 0.2795f, 0.18633f, 0.046583f };
    float a[5] = { 1.0f, -0.7821f, 0.67998f, -0.18268f, 0.030119f };
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

    // Spell this as a real `%`. Written out as `x - x/y*y` with the
    // subexpression repeated, MSVC re-associates it using n == d - i and
    // emits `(1 - q)*d - i` -- an extra `subfic r9,r9,0x1` and a shifted
    // operand chain. Retail computes the numerator ONCE and subtracts
    // (`subf r11,r11,r8` / `divw` / `mullw` / `subf r11,r9,r11`), which is
    // exactly what `%` lowers to. Same value either way for signed
    // truncated division.
    int offset = (mDecimRate - mIdx) % mDecimRate;
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
            ixDecim = overlap;
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
    int sampleIdx = 0;
    while (sampleIdx < numSamples) {
        float filtered = mFilter->FilterSlow((float)samples[sampleIdx]) * gain;
        decimAccum = kPropFilter * (filtered - decimAccum) + decimAccum;
        if (ixDecim < mFrameSize && ((sampleIdx + mIdx) % mDecimRate) == 0) {
            mDecimBuf[ixDecim] = decimAccum;
            float sq = decimAccum * decimAccum + lastVal;
            mCorrBuf[ixDecim] = sq;
            lastVal = mCorrBuf[ixDecim];
            ixDecim++;
        }
        sampleIdx += 1;
    }
    mFilter->End();
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (sDump) {
        dump(mDecimBuf, 192);
    }
#endif

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
        unk48 = Time2IirA(floorSeconds, 60.0f);
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
        // ⚠ RETAIL'S FORM, NOT THE INTUITIVE ONE. The spelling
        // `39.863136f + -36.376316f * log10(pitchHz)` is wrong: retail refutes it on
        // bytes: retail emits `fmsubs f0, f12, f0, f13` with f0=39.863136 and
        // f13=36.376316 (read out of .rdata at 0x8219AF54/0x8219AF50), i.e.
        // `log10 * 39.863136 - 36.376316`, where the other spelling compiles
        // to `fnmsubs` with the two constants in the opposite roles.
        // The retail form is also the only one that is MEANINGFUL: it is the
        // standard Hz->MIDI-note conversion 69 + 12*log2(f/440), since
        // 12/log10(2) = 39.863136 and 39.863136*log10(440) - 69 = 36.376316.
        // So this is a behavioural fix, not only a codegen one -- the inherited
        // spelling returned a quantity that is not a note number at all.
        mPitch = 39.863136f * (float)log10(pitchHz) - 36.376316f;
    }
    unk14++;
    unk18 += numSamples;
    pitchOut = mPitch;
    // Retail divides fixedGain by unk38 first:
    //   fdivs f0,f25,f0; fmuls f0,f0,f24; fmuls f0,f0,f13
    // (f25=fixedGain, f24=pitchHint, f13=mAveEnergy). The explicit grouping
    // reproduces it; the flat spelling reassociates under /fp:fast.
    confidenceOut = ((fixedGain / unk38) * pitchHint) * mAveEnergy;
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
        mDecimBuf = (float *)PD_MEM_ALLOC(mFrameSize * 4, 0x10);
        mCorrBuf = (float *)PD_MEM_ALLOC(mFrameSize * 4, 0x10);
        mPeakBuf = (float *)PD_MEM_ALLOC(mFrameSize * 4, 0x10);
        memset(mDecimBuf, 0, mFrameSize * 4);
        memset(mCorrBuf, 0, mFrameSize * 4);
        memset(mPeakBuf, 0, mFrameSize * 4);
        mIdx = 0;
    }
}
