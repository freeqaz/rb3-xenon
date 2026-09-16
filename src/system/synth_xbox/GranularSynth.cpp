#include "types.h"
#include "GranularSynth.h"
#ifndef HX_NATIVE
#include "../stlport/stl/_uninitialized.h"
#include "IPP_basicmath_xbox.h"
#include <math.h>
#include <string.h>

// NOT PORTED FROM DC3 (deliberate): dc3's GranularSynth.cpp additionally emits
//   template ObjectDir::Viewport *__uninitialized_fill_n(...)
// because the COMDAT the linker kept inside DC3's GranularSynth range is spelled
// with ObjectDir::Viewport, and DC3's row carries that name so it must be
// emitted there to pair.  RB3's corresponding row is ANONYMOUS (fn_82B744E8,
// 80 B) -- objdiff pairs by name and a placeholder target name cannot be paired
// by emitting any spelling, so the instantiation would be inert here.  Skipped
// rather than copied, which also avoids pulling obj/Dir.h into this TU.

// ZS-MISSING-INSTANTIATION: retail out-of-lined Util::Log<float> in this TU
// (declared in PitchCorrectedVoice.cpp, defined + instantiated here). dc3 idiom.
// Synapse natural-log helper with a small-magnitude clamp (avoids log(0)).
namespace Util {
template <class T>
T Log(const T &x) {
    if (x < 1.0000000359391298e-36f && x > -1.0000000359391298e-36f) {
        return -1.0000000409184788e+35f;
    }
    return (T)log(x);
}
template float Log<float>(const float &);
} // namespace Util

namespace DSP {
namespace Synapse {

GranularSynth::~GranularSynth() {
}

GranularSynth::GranularSynth(const FloatVec &input, unsigned int numVoices, unsigned int hop,
                             unsigned int maxLength)
    : mInput(&input), mHopF((float)hop), mOffset(0.0f), mLengthMix(0.0f), mHop(hop),
      mMaxLength(maxLength), mFrame(0), mBlock(0),
      mGranules((maxLength * 2 / hop + 16) * numVoices, Granule()),
      mVoices(numVoices, Voice()) {
    // Four analysis windows, geometrically spaced between mHop and mMaxLength
    // samples; each is a raised cosine falling from 1 to 0 across its length.
    mWindows.resize(4);

    for (unsigned int i = 0; i < mWindows.size(); i++) {
        FloatVec &window = mWindows[i];
        float shortest = (float)mHop;
        float longest = (float)mMaxLength;
        float step = (Util::Log(longest) - Util::Log(shortest)) * (float)i / (float)mWindows.size();
        float length = (float)exp(Util::Log(shortest) + step);
        unsigned int n = (unsigned int)(length + (length >= 0.0f ? 0.5f : -0.5f));
        window.resize(n);

        for (unsigned int j = 0; j < window.size(); j++) {
            static const float sPi = 3.1415927410125732f;
            float angle = ((float)j + 0.5f) * sPi;
            window[j] = ((float)cos(angle / (float)window.size()) + 1.0f) * 0.5f;
        }
    }

    for (unsigned int v = 0; v < mVoices.size(); v++) {
        mVoices[v].mNextTime = (double)mMaxLength;
    }
}

// Advance one frame and hand every due voice a free granule, sized from the
// voice's rate and the current mLengthMix, and scheduled against mBlock.
void GranularSynth::ExtractGranules() {
    mFrame = mFrame + 1;
    unsigned int span = mMaxLength * 2;

    for (unsigned int v = 0; v < mVoices.size(); v++) {
        Voice &vo = mVoices[v];
        if (!vo.mActive) {
            continue;
        }
        if ((double)mFrame <= vo.mNextTime) {
            continue;
        }

        for (unsigned int s = 0; s < mGranules.size(); s++) {
            Granule &gr = mGranules[s];
            if (gr.mActive) {
                continue;
            }

            gr.mStartTime = vo.mNextTime;
            vo.mNextTime = mHopF / vo.mRate + vo.mNextTime;
            gr.mNextTime = vo.mNextTime;
            gr.mOffset = mOffset;

            float reach = mHopF * 4.0f;
            float tight = reach / (vo.mRate * 3.0f + 1.0f);
            float loose = reach / (vo.mRate + 3.0f);
            gr.mLength = (loose - tight) * mLengthMix + tight;

            float longest = (float)mMaxLength * 1.5f;
            float shortest = (float)mWindows[0].size();
            if (gr.mLength < shortest) {
                gr.mLength = shortest;
            } else if (gr.mLength > longest) {
                gr.mLength = longest;
            }

            gr.mGain = vo.mGain;
            gr.mActive = true;
            gr.mVoice = v;
            gr.mFadeIn =
                (unsigned int)(gr.mLength + (gr.mLength >= 0.0f ? 0.5f : -0.5f));
            gr.mStartOffset = mBlock;
            gr.mPhase = (float)((double)mFrame - gr.mStartTime + gr.mOffset);

            // RESIDUAL provenance (DC3's measurements, DC3's addresses).  The
            // placement of this `length` local is load-bearing there and was
            // tuned: HERE = 92.2 canonical; before the gr.mGain store = 91.5;
            // after the window loop = 87.3; removed entirely = 87.3.  DC3 also
            // measured six further spellings, all of which scored the same or
            // worse (hoisting `length` above the gr.mGain store = 89.1; a
            // `startOffset` temp = inert; a `windowLen` local = 87.6;
            // `rounded` before the window loop = 86.5).  Carried as provenance
            // only -- RB3 is a different image and those numbers are not
            // predictions about this build.
            float length = gr.mLength;
            // Pick the largest window that still fits inside 0.7 of the grain.
            gr.mWindow = mWindows.size() - 1;
            while (gr.mWindow != 0) {
                if ((float)mWindows[gr.mWindow].size() <= length * 0.7f) {
                    break;
                }
                gr.mWindow = gr.mWindow - 1;
            }

            unsigned int rounded = (unsigned int)(length + (length >= 0.0f ? 0.5f : -0.5f));
            gr.mWindowLen = mWindows[gr.mWindow].size();

            float delay = (float)(rounded + gr.mWindowLen) - gr.mPhase + gr.mOffset - 1.0f +
                          (float)(unsigned int)gr.mStartOffset;
            if (delay < 0.0f) {
                delay = 0.0f;
            }
            gr.mDelay = (unsigned int)delay;

            float life = (float)(span + gr.mWindowLen) - gr.mPhase + gr.mOffset - 1.0f +
                         (float)(unsigned int)gr.mStartOffset;
            if (life < 0.0f) {
                life = 0.0f;
            }
            // Rounded through unsigned, not int: retail's `fctidz` is the
            // 64-bit convert MSVC emits for a float->unsigned cast, where an
            // (int) cast would be `fctiwz`.
            gr.mLifetime = (unsigned int)(life + (life >= 0.0f ? 0.5f : -0.5f));
            break;
        }
    }

    mBlock = mBlock + 1;
}

// Mix every active grain into its voice's output channel, then apply the
// per-voice gain.  `out` is one float* per voice; `count` samples per channel.
void GranularSynth::Synthesize(unsigned int count, float *const *out) {
    for (unsigned int v = 0; v < mVoices.size(); v++) {
        if (count != 0) {
            memset(out[v], 0, count * sizeof(float));
        }
    }

    for (unsigned int g = 0; g < mGranules.size(); g++) {
        Granule &gr = mGranules[g];
        if (!gr.mActive) {
            continue;
        }

        unsigned int window = gr.mWindow;
        unsigned int windowLen = gr.mWindowLen;
        unsigned int avail = (unsigned int)(gr.mDelay > 0 ? gr.mDelay : 0);
        unsigned int limit = stlpmtx_std::min(avail, count);

        for (unsigned int k = gr.mStartOffset; k < limit; k++) {
            gr.mPhase = gr.mPhase + 1.0f;
            // Read once, at the TOP of the body.  In DC3's image this single
            // line is the whole 88.5 -> 100 crossing: keeping mFadeIn live
            // across the body pushes &mGranules out of the volatile range and
            // changes the frame size, whereas reading gr.mFadeIn inline in the
            // compare below lets MSVC sink the load.  Keep it hoisted.
            unsigned int fadeIn = gr.mFadeIn;

            float t = gr.mPhase - gr.mOffset;
            unsigned int pos = (unsigned int)(t + (t >= 0.0f ? 0.5f : -0.5f));

            float phase = gr.mPhase;
            if (phase >= (float)mInput->size()) {
                phase = phase - (float)mInput->size();
            }

            unsigned int i0 = (unsigned int)phase;
            unsigned int i1 = i0 + 1;
            if (i1 >= mInput->size()) {
                i1 = 0;
            }
            float frac = phase - (float)i0;
            float a = mInput->begin()[i0];
            float s = (mInput->begin()[i1] - a) * frac + a;

            if (pos >= fadeIn) {
                s = mWindows[window][pos - fadeIn] * s;
            } else if (pos < windowLen) {
                s = (1.0f - mWindows[window][pos]) * s;
            }

            // RESIDUAL (raw only in DC3; DC3's canonical is 100.0).  DC3's
            // image accumulates `fadds f0, f10, f0` (loaded sample first) where
            // it emits `fadds f0, f0, f10`; DC3 measured both `+=` and
            // `s + out[...][k]` as inert, i.e. the operand order is chosen
            // after scheduling, not from source order.  This is the one site
            // that keeps DC3's row at fuzzy 99.9487 rather than 100.
            float acc = out[gr.mVoice][k];
            out[gr.mVoice][k] = acc + s;
        }

        gr.mLifetime -= count;
        gr.mDelay -= count;
        if (gr.mLifetime <= 0) {
            gr.mActive = false;
        }
        gr.mStartOffset = 0;
    }

    for (unsigned int v = 0; v < mVoices.size(); v++) {
        IPP::MulConstant_InPlace(count, out[v], mVoices[v].mGain);
    }

    mBlock = 0;
}

// Compact the granule pool: keep the active grains at the front (in order) and
// clear the active flag on every slot they no longer occupy.
void GranularSynth::Flush() {
    unsigned int kept = 0;
    for (unsigned int i = 0; i < mGranules.size(); i++) {
        if (mGranules[i].mActive) {
            mGranules[kept] = mGranules[i];
            kept++;
        }
    }

    for (unsigned int j = kept; j < mGranules.size(); j++) {
        mGranules[j].mActive = false;
    }
}

} // namespace Synapse
} // namespace DSP

namespace stlpmtx_std {

// Explicit specialization of _Vector_base constructor for UVoice
template <>
_Vector_base<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>::_Vector_base(
    size_t __n,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>& __a
) : _M_start(0), _M_finish(0), _M_end_of_storage(__a, 0)
{
    _M_start = _M_end_of_storage.allocate(__n);
    _M_finish = _M_start;
    _M_end_of_storage._M_data = _M_start + __n;
}

// Explicit specialization of vector fill constructor for UVoice
template <>
vector<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>::vector(
    size_type __n,
    const DSP::Synapse::GranularSynth::Voice& __val,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>& __a
) : _Vector_base<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>(__n, __a)
{
    _M_finish = __uninitialized_fill_n(_M_start, __n, __val, __false_type());
}

// Explicit specialization of vector constructor for Granule
template <>
vector<DSP::Synapse::GranularSynth::Granule, StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>>::vector(
    unsigned int __n,
    const DSP::Synapse::GranularSynth::Granule& __val,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>& __a
) : _Vector_base<DSP::Synapse::GranularSynth::Granule, StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>>(__n, __a)
{
    this->_M_finish = __uninitialized_fill_n(this->_M_start, __n, __val, __type_traits<DSP::Synapse::GranularSynth::Granule>::is_POD_type());
}

} // namespace stlpmtx_std
#endif // HX_NATIVE

// sw2 scatter-include (default/GranularSynth <- os/Archive.cpp)
#define gRev gRev_Archive
#define gAltRev gAltRev_Archive
#include "os/Archive.cpp"
#undef gRev
#undef gAltRev
