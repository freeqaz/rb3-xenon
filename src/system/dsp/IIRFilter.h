#pragma once

#include "xdk/LIBCMT/vectorintrinsics.h"

// A plain 16-byte float quad. Deliberately NOT __vector4: retail copies the
// two members declared with this type using 8-byte `ld`/`std` pairs, while it
// copies the five __vector4 members with 16-byte `stvx128`. That is a TYPE
// distinction in the original source, not a scheduling accident -- see the
// width sweep in docs/decomp/W16BW_*.
struct IIRQuad { /* Size=0x10 */
    float f[4];
};

class IIR4PoleFilter {
public:
    IIR4PoleFilter(float *b, float *a);
    void Begin();
    void End();
    float FilterSlow(float x);

    // 0x00..0x5F -- PINNED by ?FilterSlow@IIR4PoleFilter@@QAAMM@Z, which is
    // byte-identical to retail (0x82B81538, 124 B, zero differing words) and
    // reads mB0/mGain/mNegA/mAccum at these offsets. Do not disturb.
    float mB0[4];        // 0x00 - first b coefficient per pole
    float mState1[4];    // 0x10 - 1.0 per pole (initial)
    float mGain[4];      // 0x20 - second b coefficient per pole
    float mB0NegA[4];    // 0x30 - -b0 * a per pole
    float mNegA[4];      // 0x40 - -a per pole
    float mAccum[4];     // 0x50 - running accumulator

    // 0x60..0xDF -- the SIMD mirror. Identified by lane W16-BW via a
    // whole-binary xref sweep over orig/45410914/band.exe:
    //
    //   * IIR4PoleFilter has exactly TWO bodies in retail (this ctor at
    //     0x82B815B8 and FilterSlow at 0x82B81538); Begin/End are ICF-folded
    //     empties. `bl 0x82B815B8` has ONE caller (??0PitchDetector) and
    //     `bl 0x82B81538` has TWO (both inside AnalyzeBlock).
    //   * The object is reachable only through PitchDetector::mFilter, and
    //     every dereference of that pointer in retail is `lwz r3,0(rX)`
    //     immediately followed by `bl` to FilterSlow or to the folded empty.
    //
    //   => NOTHING in the retail binary ever READS 0x60..0xDF. The ctor
    //      writes it and it is dead thereafter. It is a mirror of the scalar
    //      block in VMX register layout, evidently for a fast path that the
    //      shipped build no longer contains (only FilterSlow survives).
    //
    // mV60 is never written by the ctor either -- the 80-byte shortfall this
    // header used to carry is exactly the seven assignments below.
    __vector4 mV60;      // 0x60 - never written, never read
    __vector4 mVState1;  // 0x70 = mState1
    IIRQuad   mQ80;      // 0x80 = zero
    __vector4 mVB0;      // 0x90 = mB0
    __vector4 mVGain;    // 0xa0 = mGain
    __vector4 mVB0NegA;  // 0xb0 = mB0NegA
    __vector4 mVNegA;    // 0xc0 = mNegA
    IIRQuad   mQD0;      // 0xd0 = zero
};
