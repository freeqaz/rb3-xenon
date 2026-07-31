#pragma once
#include "obj/Data.h"
#include "rndobj/Overlay.h"
#include "os/Debug.h"
#include "math/Utl.h"
#include <algorithm> // std::min (MSVC stlport: not transitively provided)

namespace {
    RndOverlay *gGuitarOverlay;
}

class MercurySwitchFilter {
public:
    MercurySwitchFilter() {}
    virtual ~MercurySwitchFilter() {}
    virtual bool Poll(float, float) = 0;
    virtual void Reset() = 0;
};

class LowPassMercurySwitchFilter : public MercurySwitchFilter {
public:
    LowPassMercurySwitchFilter(DataArray *arr)
        : mSensitivity(arr->FindFloat("sensitivity")),
          mOnThreshold(arr->FindFloat("on_threshold")),
          mOffThreshold(arr->FindFloat("off_threshold")), mState(false), mLastPoll(0.0f) {

    }

    virtual ~LowPassMercurySwitchFilter() {}
    virtual bool Poll(float f1, float f2) {
        float fvar1 = std::min(100.0f, f1 - mLastPoll);
        ClampEq<float>(f2, 0, 1);
        for (; fvar1 > 0; fvar1 -= 17.0f) {
            mAccum = (1.0f - mSensitivity) * mAccum + mSensitivity * f2;
        }
        if (!mState) {
            if (mAccum > mOnThreshold)
                mState = true;
        } else if (mAccum < mOffThreshold)
            mState = false;
// rb3-Wii guards this with #ifdef MILO_DEBUG; retail compiled it out.  Retail's
// LowPassMercurySwitchFilter::Poll is target fn_8279E720 (0x8279E720, size 0xD8),
// identified by exact field offsets: 0x14=mLastPoll, 0x4=mSensitivity,
// 0x18=mAccum, 0x10=mState (byte), 0x8/0xc=mOn/mOffThreshold, with the
// `for (; fvar1 > 0; fvar1 -= 17.0f)` accumulator loop at .L_8279E794 and the
// hysteresis block ending at .L_8279E7E8 -> `lbz r3,0x10(r11)` /
// `stfs f1,0x14(r11)` / `blr`.  That function contains ZERO `bl` instructions and
// has no stack frame -- it is a pure leaf.  An overlay emission needs a virtual
// Showing() call plus a varargs MakeString plus operator<<, which is structurally
// impossible in a frameless leaf.  Retail excludes it.
// CORRECTNESS-ONLY: fn_8279E720 is anonymous in target_symbol_map.json so it
// cannot pair; measured metric-inert.  Keep the overlay for the native build.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (gGuitarOverlay->Showing()) {
            *gGuitarOverlay
                << MakeString("    raw %4.2f avg %4.2f out %d\n", f2, mAccum, mState);
        }
#endif
        mLastPoll = f1;
        return mState;
    }

    virtual void Reset() { mLastPoll = 0.0f; }

    float mSensitivity;
    float mOnThreshold;
    float mOffThreshold;
    bool mState;
    float mLastPoll;
    float mAccum;
};

class AnySignMercurySwitchFilter : public MercurySwitchFilter {
public:
    AnySignMercurySwitchFilter(DataArray *arr)
        : mWindow(arr->FindFloat("window") * 1000.0f),
          mNumFramesThreshold(arr->FindInt("num_frames")),
          mThreshold(arr->FindFloat("threshold")) {
        Reset();
    }
    virtual ~AnySignMercurySwitchFilter() {}
    virtual bool Poll(float f1, float f2) {
        if (f2 >= mThreshold)
            mNumFrames++;
        else
            mNumFrames = 0;

        if (mNumFrames >= mNumFramesThreshold)
            mLastOn = f1;
        bool asdf = f1 - mLastOn < mWindow;
// This sibling overlay was left UNGUARDED, but retail excludes it just as it
// excludes the LowPass one above.  Retail's AnySignMercurySwitchFilter::Poll is
// target fn_8279E930 (falling through into fn_8279E948; dtk splits them at a
// branch target), identified by exact field offsets: 0x14=mThreshold,
// 0xc=mNumFrames, 0x10=mNumFramesThreshold, 0x8=mLastOn, 0x4=mWindow.  Like its
// sibling it contains ZERO `bl` instructions and no stack frame -- a pure leaf,
// which cannot host a virtual Showing() call plus varargs MakeString plus
// operator<<.  Guarded to match, kept live for the native build.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (gGuitarOverlay && gGuitarOverlay->Showing()) {
            *gGuitarOverlay << MakeString(
                " val %4.2f    frames %4d   ->   %d\n", f2, mNumFrames, asdf
            );
        }
#endif
        return asdf;
    }

    virtual void Reset() {
        mNumFrames = 0;
        mLastOn = mWindow * -2.0f;
    }

    float mWindow;
    float mLastOn;
    int mNumFrames;
    int mNumFramesThreshold;
    float mThreshold;
};

MercurySwitchFilter *NewMercurySwitchFilter(DataArray *);
