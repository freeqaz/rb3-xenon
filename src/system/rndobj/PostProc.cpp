#include "rndobj/PostProc.h"
#include "PostProc.h"
#include "Rnd.h"
#include "Utl.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "math/Key.h"
#include "rndobj/DOFProc.h"
#include "rndobj/Draw.h"
#include "rndobj/HiResScreen.h"
#include "utl/BinStream.h"
#include "synth_xbox/PitchCorrectedVoice.h"

#ifdef HX_NATIVE
RndPostProc *RndPostProc::sCurrent;
float RndPostProc::sBloomLocFactor;
DOFOverrideParams RndPostProc::sDOFOverride;
#endif

float TrueColor::ExposureRecipe::GetLux() { return mLux; }

void RndPostProc::ResetDofProc() { TheDOFProc->UnSet(); }
RndPostProc *RndPostProc::Current() { return sCurrent; }

ProcCounter::ProcCounter()
    : mProcAndLock(0), mCount(0), mSwitch(0), mOdd(0), mFPS(0), mEvenOddDisabled(0),
      mTriFrameRendering(0) {}

void ProcCounter::SetProcAndLock(bool pandl) {
    mProcAndLock = pandl;
    mCount = -1;
}

void ProcCounter::SetEvenOddDisabled(bool eod) {
    if (mEvenOddDisabled == eod)
        return;
    mEvenOddDisabled = eod;
    if (mEvenOddDisabled)
        mCount = -1;
}

// Configure frame rate emulation by calculating frame skip pattern
// For 24fps on 60Hz: 120/24=5 -> switch every 2-3 frames (alternating via mOdd)
// For 30fps on 60Hz: 120/30=4 -> switch every 2 frames (mOdd=0)
unsigned int ProcCounter::SetEmulateFPS(int fps) {
    // Disable emulation
    if (fps <= 0) {
        if (mFPS != fps) {
            mFPS = 0;
            mSwitch = 0;
            mOdd = 0;
            mCount = 0;
        }
        return mFPS;
    }

    // Clamp to valid range and skip if unchanged
    auto _tmp0 = Clamp(1, 60, fps);
    fps = _tmp0;
    if (fps == mFPS)
        return mFPS;

    // Calculate frame timing: 120 / fps gives us the frame interval
    // Example: 24fps -> 120/24=5 -> alternate between 2 and 3 frame intervals
    mFPS = fps;
    int round = Round(120.0 / (f32)fps);
    mOdd = round & 1;        // Remainder: 0 for even intervals, 1 for odd
    mSwitch = round >> 1;    // Base interval (divide by 2)

    // Reset counter if it would overshoot the new interval
    if (mCount < mSwitch)
        return fps;
    mCount = 0;
    return fps;
}

// Frame processing state machine: determines which render passes to execute this frame
// when emulating lower frame rates (e.g., 24fps film look on 60Hz display)
ProcessCmd ProcCounter::ProcCommands() {
    // Proc-and-lock mode: process only once, then lock until reset
    if (mProcAndLock) {
        if (mCount >= 0) {
            return kProcessNone;
        }
        mCount = 0;
        return kProcessAll;
    }

    // Even-odd disabled: always process everything
    if (mEvenOddDisabled) {
        return kProcessAll;
    }

    // Update FPS emulation settings from current post-processor
    SetEmulateFPS(
        RndPostProc::Current() ? Round(RndPostProc::Current()->EmulateFPS()) : 0
    );

    // No emulation needed: process everything
    if (mSwitch < 2) {
        return kProcessAll;
    }

    // State machine: alternate between world and post-processing passes
    // mCount cycles through: -1 (init) -> 0 (world) -> 1 (post) -> 2+ (none) -> 0 (reset)
    ProcessCmd cmd = kProcessNone;
    switch (mCount) {
    case -1: // Initial state: process everything and skip to count=1
        mCount = 1;
        cmd = kProcessAll;
        break;
    case 0: // Process world geometry only
        cmd = kProcessWorld;
        break;
    case 1: // Process post-effects only
        cmd = kProcessPost;
        break;
    default: // Count >= 2: skip rendering (waiting for reset)
        break;
    }

    // Advance counter and handle variable frame timing
    // mOdd flips sign to alternate between floor(rate) and ceil(rate) frames
    mCount++;
    if (mCount >= mSwitch) {
        mCount = 0;
        mSwitch += mOdd;
        mOdd = -mOdd;
    }
    return cmd;
}

RndPostProc::RndPostProc()
    : mPriority(1), mBloomColor(1, 1, 1, 0), mBloomThreshold(4), mBloomIntensity(0),
      mBloomGlare(0), mBloomStreak(0), mBloomStreakAttenuation(0.9f),
      mBloomStreakAngle(0), mForceCurrentInterp(0), mColorXfm(), mPosterLevels(0),
      mPosterMin(1), mKaleidoscopeComplexity(0), mKaleidoscopeSize(0.5f),
      mKaleidoscopeAngle(0), mKaleidoscopeRadius(0), mKaleidoscopeFlipUVs(1),
      mFlickerModBounds(0, 0), mFlickerTimeBounds(0.001f, 0.007f), mFlickerSeconds(0, 0),
      mColorModulation(1), mNoiseBaseScale(32, 24), mNoiseTopScale(1.35914f),
      mNoiseIntensity(0), mNoiseStationary(0), mNoiseMidtone(1), mNoiseMap(this, 0),
      mTrailThreshold(1), mTrailDuration(0), mBlendVec(1, 1, 0), mEmulateFPS(30),
      mLastRender(0), mHallOfTimeType(0), mHallOfTimeRate(0),
      mHallOfTimeColor(1, 1, 1, 0), mHallOfTimeMix(0), mMotionBlurWeight(1, 1, 1, 0),
      mMotionBlurBlend(0), mMotionBlurVelocity(1), mGradientMap(this, 0),
      mGradientMapOpacity(0), mGradientMapIndex(0), mGradientMapStart(0),
      mGradientMapEnd(1), mRefractMap(this, 0), mRefractDist(0.05f), mRefractScale(1, 1),
      mRefractPanning(0, 0), mRefractVelocity(0, 0), mRefractAngle(0),
      mChromaticAberrationOffset(0), mChromaticSharpen(0), mVignetteColor(0, 0, 0, 0),
      mVignetteIntensity(0), mHueTarget(-75), mHueFocus(0.958), mBlendAmount(0),
      mBrightnessPower(1) {
    mColorXfm.Reset();
}

RndPostProc::~RndPostProc() {
    Unselect();
    if (TheRnd.GetPostProcOverride() == this) {
        TheRnd.SetPostProcOverride(nullptr);
    }
}

BEGIN_HANDLERS(RndPostProc)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_ACTION(select, Select())
    HANDLE_ACTION(unselect, Unselect())
    HANDLE_ACTION(multi_select, OnSelect())
    HANDLE_ACTION(multi_unselect, OnUnselect())
    HANDLE_ACTION(
        interp,
        Interp(_msg->Obj<RndPostProc>(2), _msg->Obj<RndPostProc>(3), _msg->Float(4))
    )
    HANDLE(allowed_normal_map, OnAllowedNormalMap)
END_HANDLERS

BEGIN_PROPSYNCS(RndPostProc)
    SYNC_PROP(priority, mPriority)
    SYNC_PROP(bloom_color, mBloomColor)
    SYNC_PROP(bloom_threshold, mBloomThreshold)
    SYNC_PROP(bloom_intensity, mBloomIntensity)
    SYNC_PROP(bloom_glare, mBloomGlare)
    SYNC_PROP(bloom_streak, mBloomStreak)
    SYNC_PROP(bloom_streak_attenuation, mBloomStreakAttenuation)
    SYNC_PROP(bloom_streak_angle, mBloomStreakAngle)
    SYNC_PROP_MODIFY(hue, mColorXfm.mHue, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(saturation, mColorXfm.mSaturation, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(lightness, mColorXfm.mLightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(brightness, mColorXfm.mBrightness, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(contrast, mColorXfm.mContrast, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(in_lo, mColorXfm.mLevelInLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(in_hi, mColorXfm.mLevelInHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(out_lo, mColorXfm.mLevelOutLo, mColorXfm.AdjustColorXfm())
    SYNC_PROP_MODIFY(out_hi, mColorXfm.mLevelOutHi, mColorXfm.AdjustColorXfm())
    SYNC_PROP(num_levels, mPosterLevels)
    SYNC_PROP(min_intensity, mPosterMin)
    SYNC_PROP(kaleidoscope_complexity, mKaleidoscopeComplexity)
    SYNC_PROP(kaleidoscope_size, mKaleidoscopeSize)
    SYNC_PROP(kaleidoscope_angle, mKaleidoscopeAngle)
    SYNC_PROP(kaleidoscope_radius, mKaleidoscopeRadius)
    SYNC_PROP(kaleidoscope_flipUVs, mKaleidoscopeFlipUVs)
    SYNC_PROP(flicker_intensity, mFlickerModBounds)
    SYNC_PROP(flicker_secs_range, mFlickerTimeBounds)
    SYNC_PROP(noise_base_scale, mNoiseBaseScale)
    SYNC_PROP(noise_intensity, mNoiseIntensity)
    SYNC_PROP(noise_stationary, mNoiseStationary)
    SYNC_PROP(noise_midtone, mNoiseMidtone)
    SYNC_PROP(noise_map, mNoiseMap)
    SYNC_PROP(threshold, mTrailThreshold)
    SYNC_PROP(duration, mTrailDuration)
    SYNC_PROP(emulate_fps, mEmulateFPS)
    SYNC_PROP(hall_of_time_type, mHallOfTimeType)
    SYNC_PROP(hall_of_time_rate, mHallOfTimeRate)
    SYNC_PROP(hall_of_time_color, mHallOfTimeColor)
    SYNC_PROP(hall_of_time_mix, mHallOfTimeMix)
    SYNC_PROP(motion_blur_blend, mMotionBlurBlend)
    SYNC_PROP(motion_blur_weight, mMotionBlurWeight)
    SYNC_PROP(motion_blur_exposure, mMotionBlurWeight.alpha)
    SYNC_PROP(motion_blur_velocity, mMotionBlurVelocity)
    SYNC_PROP(gradient_map, mGradientMap)
    SYNC_PROP(gradient_map_opacity, mGradientMapOpacity)
    SYNC_PROP(gradient_map_index, mGradientMapIndex)
    SYNC_PROP(gradient_map_start, mGradientMapStart)
    SYNC_PROP(gradient_map_end, mGradientMapEnd)
    SYNC_PROP(refract_map, mRefractMap)
    SYNC_PROP(refract_dist, mRefractDist)
    SYNC_PROP(refract_scale, mRefractScale)
    SYNC_PROP(refract_panning, mRefractPanning)
    SYNC_PROP(refract_velocity, mRefractVelocity)
    SYNC_PROP(refract_angle, mRefractAngle)
    SYNC_PROP(chromatic_aberration_offset, mChromaticAberrationOffset)
    SYNC_PROP(chromatic_sharpen, mChromaticSharpen)
    SYNC_PROP(vignette_color, mVignetteColor)
    SYNC_PROP(vignette_intensity, mVignetteIntensity)
    SYNC_PROP(hue_target, mHueTarget)
    SYNC_PROP(hue_focus, mHueFocus)
    SYNC_PROP(blend_amount, mBlendAmount)
    SYNC_PROP(brightness_power, mBrightnessPower)
    SYNC_PROP(force_current_interp, mForceCurrentInterp)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndPostProc)
    SAVE_REVS(0x25, 2)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mBloomColor;
    bs << mBloomIntensity;
    bs << mBloomThreshold;
    mColorXfm.Save(bs);
    bs << mFlickerModBounds << mFlickerTimeBounds;
    bs << mNoiseBaseScale << mNoiseTopScale << mNoiseIntensity << mNoiseStationary;
    bs << mNoiseMap;
    bs << mNoiseMidtone;
    bs << mTrailThreshold;
    bs << mTrailDuration;
    bs << mEmulateFPS;
    bs << mPosterLevels;
    bs << mPosterMin;
    bs << mKaleidoscopeComplexity;
    bs << mKaleidoscopeSize;
    bs << mKaleidoscopeAngle;
    bs << mKaleidoscopeRadius;
    bs << mKaleidoscopeFlipUVs;
    bs << mHallOfTimeRate;
    bs << mHallOfTimeColor << mHallOfTimeMix;
    bs << mHallOfTimeType;
    bs << mMotionBlurBlend;
    bs << mMotionBlurWeight;
    bs << mMotionBlurVelocity;
    bs << mGradientMap;
    bs << mGradientMapOpacity;
    bs << mGradientMapIndex;
    bs << mGradientMapStart;
    bs << mGradientMapEnd;
    bs << mRefractMap;
    bs << mRefractDist;
    bs << mRefractScale;
    bs << mRefractPanning;
    bs << mRefractAngle;
    bs << mRefractVelocity;
    bs << mChromaticAberrationOffset;
    bs << mChromaticSharpen;
    bs << mVignetteColor;
    bs << mVignetteIntensity;
    bs << mBloomGlare;
    bs << mBloomStreak;
    bs << mBloomStreakAttenuation;
    bs << mBloomStreakAngle;
    bs << mHueTarget;
    bs << mHueFocus;
    bs << mBlendAmount;
    bs << mBrightnessPower;
END_SAVES

BEGIN_COPYS(RndPostProc)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndPostProc)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPriority)
        COPY_MEMBER(mBloomIntensity)
        COPY_MEMBER(mBloomColor)
        COPY_MEMBER(mBloomThreshold)
        COPY_MEMBER(mBloomGlare)
        COPY_MEMBER(mBloomStreak)
        COPY_MEMBER(mBloomStreakAttenuation)
        COPY_MEMBER(mBloomStreakAngle)
        COPY_MEMBER(mColorXfm)
        COPY_MEMBER(mFlickerModBounds)
        COPY_MEMBER(mFlickerTimeBounds)
        COPY_MEMBER(mNoiseBaseScale)
        COPY_MEMBER(mNoiseTopScale)
        COPY_MEMBER(mNoiseIntensity)
        COPY_MEMBER(mNoiseStationary)
        COPY_MEMBER(mNoiseMap)
        COPY_MEMBER(mNoiseMidtone)
        COPY_MEMBER(mTrailDuration)
        COPY_MEMBER(mTrailThreshold)
        COPY_MEMBER(mEmulateFPS)
        COPY_MEMBER(mPosterLevels)
        COPY_MEMBER(mPosterMin)
        COPY_MEMBER(mKaleidoscopeComplexity)
        COPY_MEMBER(mKaleidoscopeSize)
        COPY_MEMBER(mKaleidoscopeAngle)
        COPY_MEMBER(mKaleidoscopeRadius)
        COPY_MEMBER(mKaleidoscopeFlipUVs)
        COPY_MEMBER(mHallOfTimeRate)
        COPY_MEMBER(mHallOfTimeColor)
        COPY_MEMBER(mHallOfTimeMix)
        COPY_MEMBER(mHallOfTimeType)
        COPY_MEMBER(mMotionBlurBlend)
        COPY_MEMBER(mMotionBlurWeight)
        COPY_MEMBER(mMotionBlurVelocity)
        COPY_MEMBER(mGradientMap)
        COPY_MEMBER(mGradientMapIndex)
        COPY_MEMBER(mGradientMapOpacity)
        COPY_MEMBER(mGradientMapStart)
        COPY_MEMBER(mGradientMapEnd)
        COPY_MEMBER(mRefractMap)
        COPY_MEMBER(mRefractDist)
        COPY_MEMBER(mRefractScale)
        COPY_MEMBER(mRefractPanning)
        COPY_MEMBER(mRefractVelocity)
        COPY_MEMBER(mRefractAngle)
        COPY_MEMBER(mChromaticAberrationOffset)
        COPY_MEMBER(mChromaticSharpen)
        COPY_MEMBER(mVignetteColor)
        COPY_MEMBER(mVignetteIntensity)
        COPY_MEMBER(mHueTarget)
        COPY_MEMBER(mHueFocus)
        COPY_MEMBER(mBlendAmount)
        COPY_MEMBER(mBrightnessPower)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0x25, 2)

BEGIN_LOADS(RndPostProc)
    LOAD_REVS(bs)
    ASSERT_REVS(0x25, 2)
    if (d.rev == 0x10) {
        int dRev;
        d >> dRev;
        MILO_ASSERT(dRev == 3, 0x2A8);
        float f30 = 0;
        bool b70;
        Vector3 v40;
        int i5c;
        d >> b70 >> v40 >> f30 >> i5c;
    } else {
        LOAD_SUPERCLASS(Hmx::Object)
    }
    LoadRev(d);
END_LOADS

void RndPostProc::LoadRev(BinStreamRev &d) {
    if (d.rev > 4) {
        if (d.rev > 0xA) {
            d >> mBloomColor;
            if (d.rev < 0x18) {
                int dummy;
                d >> dummy;
            }
            d >> mBloomIntensity;
            d >> mBloomThreshold;
        } else {
            Hmx::Color c;
            d >> c;
            float minVal = c.red;
            if (minVal > c.green)
                minVal = c.green;
            if (minVal > c.blue)
                minVal = c.blue;
            if (minVal < 4.0f) {
                float range = 4.0f - minVal;
                mBloomThreshold = c.alpha;
                c.red = (4.0f - c.red) / range;
                c.green = (4.0f - c.green) / range;
                c.blue = (4.0f - c.blue) / range;
                c.alpha = 0.0f;
                mBloomColor = c;
            } else {
                mBloomColor.red = 1.0f;
                mBloomColor.green = 1.0f;
                mBloomColor.blue = 1.0f;
                mBloomColor.alpha = 0.0f;
                mBloomThreshold = c.alpha;
            }
            int dummy;
            d >> dummy;
            d >> mBloomIntensity;
            mBloomIntensity = sqrtf(mBloomIntensity);
            d >> dummy;
        }
    }
    if (d.rev > 5 && d.altRev < 1) {
        ObjPtr<RndTex> luminanceMap(this, 0);
        d >> luminanceMap;
    }
    if (d.rev > 6) {
        if (d.rev < 0x12) {
            d >> mColorXfm.mColorXfm.m.x >> mColorXfm.mColorXfm.m.y
                >> mColorXfm.mColorXfm.m.z;
            d >> mColorXfm.mColorXfm.v;
        } else {
            if (!mColorXfm.Load(d.stream)) {
                MILO_FAIL(
                    "%s can't load new %s version", PathName(this), ClassName()
                );
            }
        }
        d >> (Key<float>&)mFlickerModBounds >> (Key<float>&)mFlickerTimeBounds;
        if (d.rev < 9) {
            mFlickerModBounds.x = 1.0f - mFlickerModBounds.x;
            mFlickerModBounds.y = 1.0f - mFlickerModBounds.y;
        }
        if (d.rev < 0x1D) {
            mFlickerModBounds.x = 0.0f;
        }
        d >> (Key<float>&)mNoiseBaseScale >> mNoiseTopScale >> mNoiseIntensity;
        if (d.rev > 0xC) {
            d >> mNoiseStationary;
        }
        if (d.rev > 8) {
            d >> mNoiseMap;
        }
        if (d.rev > 0x24) {
            d >> mNoiseMidtone;
        } else {
            mNoiseMidtone = false;
        }
        if (d.rev < 0x12) {
            d >> mColorXfm.mHue >> mColorXfm.mSaturation >> mColorXfm.mLightness
                >> mColorXfm.mContrast >> mColorXfm.mBrightness;
        }
    }
    if (d.rev > 7) {
        d >> mTrailThreshold >> mTrailDuration >> mEmulateFPS;
    }
    if (d.rev > 9) {
        if (d.rev < 0x12) {
            d >> mColorXfm.mLevelInLo >> mColorXfm.mLevelInHi;
            d >> mColorXfm.mLevelOutLo >> mColorXfm.mLevelOutHi;
        }
        d >> mPosterLevels;
    }
    if (d.rev > 0xD) {
        d >> mPosterMin;
    }
    if (d.rev > 0xB) {
        if (d.rev < 0x16) {
            float complexity;
            d >> complexity;
            if (complexity != 0.0f) {
                mKaleidoscopeComplexity = 2.0f;
            }
        } else {
            d >> mKaleidoscopeComplexity >> mKaleidoscopeSize >> mKaleidoscopeAngle
                >> mKaleidoscopeRadius >> mKaleidoscopeFlipUVs;
        }
    }
    if (d.rev > 0xE && d.rev < 0x1F) {
        int dummy;
        d >> dummy;
        if (d.rev < 0x11) {
            int dummy2;
            d >> dummy2;
            ObjPtr<RndDrawable> dummyDraw(this, 0);
            d >> dummyDraw;
        }
    }
    if (d.rev > 0x12) {
        d >> mHallOfTimeRate;
        d >> mHallOfTimeColor >> mHallOfTimeMix;
        if (d.rev > 0x13 && d.rev < 0x20) {
            bool hotType;
            d >> hotType;
            mHallOfTimeType = hotType ? 1 : 0;
        } else if (d.rev > 0x1F) {
            d >> mHallOfTimeType;
        }
    }
    if (d.rev > 0x14) {
        d >> mMotionBlurBlend;
        if (d.rev > 0x1A) {
            d >> mMotionBlurWeight;
            if (d.rev > 0x21) {
                d >> mMotionBlurVelocity;
            }
        }
    }
    if (d.rev > 0x16) {
        d >> mGradientMap >> mGradientMapOpacity >> mGradientMapIndex
            >> mGradientMapStart >> mGradientMapEnd;
    }
    if (d.rev < 0x18) {
        mBloomThreshold *= 4.0f;
    }
    if (d.rev > 0x18) {
        d >> mRefractMap >> mRefractDist >> (Key<float>&)mRefractScale
            >> (Key<float>&)mRefractPanning >> mRefractAngle;
        if (d.rev > 0x1B) {
            d >> (Key<float>&)mRefractVelocity;
        }
    }
    if (d.rev > 0x19) {
        d >> mChromaticAberrationOffset;
        if (d.rev > 0x22) {
            d >> mChromaticSharpen;
        }
    }
    if (d.rev > 0x1D) {
        d >> mVignetteColor >> mVignetteIntensity;
    }
    if (d.rev > 0x20) {
        d >> mBloomGlare;
    }
    if (d.rev > 0x23) {
        d >> mBloomStreak >> mBloomStreakAttenuation >> mBloomStreakAngle;
    }
    if (d.altRev > 1) {
        d >> mHueTarget >> mHueFocus >> mBlendAmount >> mBrightnessPower;
    }
}

void RndPostProc::Select() {
    if (sCurrent != this) {
        if (sCurrent) {
            sCurrent->OnUnselect();
        }
        sCurrent = this;
        sCurrent->OnSelect();
    }
}

void RndPostProc::Unselect() {
    if (sCurrent == this) {
        sCurrent->OnUnselect();
        sCurrent = nullptr;
    }
}

void RndPostProc::OnSelect() {
    TheRnd.RegisterPostProcessor(this);
    static Message msg("selected");
    Handle(msg, false);
}

void RndPostProc::OnUnselect() {
    TheRnd.UnregisterPostProcessor(this);
    static Message msg("unselected");
    Handle(msg, false);
}

void RndPostProc::DoPost() {
    UpdateTimeDelta();
    UpdateColorModulation();
    UpdateBlendPrevious();
}

void RndPostProc::Init() {
    sBloomLocFactor = SystemConfig("rnd", "bloom_loc")->FindFloat(SystemLanguage());
}

void RndPostProc::Reset() {
    if (sCurrent) {
        sCurrent->OnUnselect();
        sCurrent = nullptr;
    }
    TheDOFProc->UnSet();
}

DataNode RndPostProc::OnAllowedNormalMap(const DataArray *) {
    return GetNormalMapTextures(Dir());
}

bool RndPostProc::BlendPrevious() const {
    return mTrailThreshold < 1 && mTrailDuration > 0 && !TheHiResScreen.IsActive();
}

float RndPostProc::BloomIntensity() const {
    if (mBloomGlare && TheHiResScreen.IsActive()) {
        return mBloomIntensity / 3.0f;
    } else
        return mBloomIntensity;
}

bool RndPostProc::HallOfTime() const { return mHallOfTimeRate != 0; }
bool RndPostProc::DoChromaticAberration() const {
    return mChromaticAberrationOffset != 0;
}
bool RndPostProc::DoVignette() const { return mVignetteIntensity != 0; }

bool RndPostProc::DoMotionBlur() const {
    return mMotionBlurBlend > 0 && mMotionBlurWeight.Pack() > 0
        && !TheHiResScreen.IsActive();
}

bool RndPostProc::DoGradientMap() const {
    return mGradientMapOpacity > 0 && mGradientMap;
}

bool RndPostProc::DoRefraction() const { return mRefractMap && mRefractDist; }

// Checks if any color transformation effects are active
bool RndPostProc::ColorXfmEnabled() const {
    // Color modulation or basic color adjustments
    return mColorModulation != 1 || mColorXfm.mHue != 0 || mColorXfm.mSaturation != 0
        || mColorXfm.mLightness != 0 || mColorXfm.mContrast != 0
        || mColorXfm.mBrightness != 0
        // Color level adjustments (Pack() returns 24-bit RGB: 0=black, 0xffffff=white)
        || mColorXfm.mLevelInLo.Pack() != 0 || mColorXfm.mLevelOutLo.Pack() != 0
        || mColorXfm.mLevelInHi.Pack() != 0xffffff
        || mColorXfm.mLevelOutHi.Pack() != 0xffffff;
}

void RndPostProc::UpdateColorModulation() {
    if (mFlickerTimeBounds.x > 0 && mFlickerTimeBounds.y > 0 && mFlickerModBounds.y > 0) {
        if (mFlickerSeconds.x >= mFlickerSeconds.y) {
            float diff = mFlickerSeconds.x - mFlickerSeconds.y;
            mFlickerSeconds.x = Max(diff, 0.0f);
            mColorModulation =
                1.0f - RandomFloat(mFlickerModBounds.x, mFlickerModBounds.y);
            mFlickerSeconds.y = RandomFloat(mFlickerTimeBounds.x, mFlickerTimeBounds.y);
            mFlickerSeconds.y = Max(mFlickerSeconds.x, mFlickerSeconds.y);
        }
        mFlickerSeconds.x += mDeltaSecs;
    } else {
        mColorModulation = 1.0f;
    }
}

void RndPostProc::UpdateTimeDelta() {
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float delta = secs - mLastRender;
    mLastRender = secs;
    mDeltaSecs = Clamp(0.0f, 1.0f, delta);
}

void RndPostProc::UpdateBlendPrevious() {
    if (BlendPrevious()) {
        MILO_ASSERT(mTrailDuration > 0.f, 0x100);
        mBlendVec.Set(mTrailThreshold, mDeltaSecs / mTrailDuration, 1.0f / 3.0f);
    }
}

void RndPostProc::Interp(const RndPostProc *from, const RndPostProc *to, float pct) {
    if ((!from && !to) || mForceCurrentInterp)
        return;

    // If one is null, use the other for both
    if (!to) {
        to = from;
    } else if (!from) {
        from = to;
    }

    // For non-interpolatable properties, pick based on blend direction
    const RndPostProc *pick = pct > 0.0f ? to : from;

    // Copy non-interpolatable bool/obj properties from pick
    mNoiseMidtone = pick->mNoiseMidtone;
    mNoiseStationary = pick->mNoiseStationary;
    mNoiseMap = pick->mNoiseMap.Ptr();
    mGradientMap = pick->mGradientMap.Ptr();
    mRefractMap = pick->mRefractMap.Ptr();
    mBloomGlare = pick->mBloomGlare;
    mMotionBlurVelocity = pick->mMotionBlurVelocity;
    mChromaticSharpen = pick->mChromaticSharpen;

    // Bloom intensity uses BloomIntensity() which accounts for glare/hires
    float toBloom = to->BloomIntensity();
    float fromBloom = from->BloomIntensity();
    ::Interp(fromBloom, toBloom, pct, mBloomIntensity);

    // Bloom color
    ::Interp(from->mBloomColor, to->mBloomColor, pct, mBloomColor);

    // Blend vec (Vector3)
    ::Interp(from->mBlendVec, to->mBlendVec, pct, mBlendVec);

    // Trail threshold and duration
    ::Interp(from->mTrailDuration, to->mTrailDuration, pct, mTrailDuration);
    ::Interp(from->mTrailThreshold, to->mTrailThreshold, pct, mTrailThreshold);

    // Noise — if both have noise but different stationarity, snap noise interp to 1.0
    float noisePct = pct;
    if (from != to && from->mNoiseMidtone != to->mNoiseMidtone
        && from->mNoiseIntensity != 0.0f && to->mNoiseIntensity != 0.0f) {
        noisePct = 1.0f;
    }
    ::Interp(from->mNoiseBaseScale, to->mNoiseBaseScale, noisePct, mNoiseBaseScale);
    ::Interp(from->mNoiseTopScale, to->mNoiseTopScale, noisePct, mNoiseTopScale);
    ::Interp(from->mNoiseIntensity, to->mNoiseIntensity, noisePct, mNoiseIntensity);

    // Kaleidoscope
    ::Interp(from->mKaleidoscopeComplexity, to->mKaleidoscopeComplexity, pct, mKaleidoscopeComplexity);
    ::Interp(from->mKaleidoscopeSize, to->mKaleidoscopeSize, pct, mKaleidoscopeSize);
    ::Interp(from->mKaleidoscopeAngle, to->mKaleidoscopeAngle, pct, mKaleidoscopeAngle);
    ::Interp(from->mKaleidoscopeRadius, to->mKaleidoscopeRadius, pct, mKaleidoscopeRadius);
    mKaleidoscopeFlipUVs = pct < 1.0f ? from->mKaleidoscopeFlipUVs : to->mKaleidoscopeFlipUVs;

    // Emulate FPS
    ::Interp(from->mEmulateFPS, to->mEmulateFPS, pct, mEmulateFPS);

    // Poster
    ::Interp(from->mPosterLevels, to->mPosterLevels, pct, mPosterLevels);
    ::Interp(from->mPosterMin, to->mPosterMin, pct, mPosterMin);

    // Color modulation
    ::Interp(from->mColorModulation, to->mColorModulation, pct, mColorModulation);

    // Color transform
    ::Interp(from->mColorXfm.mBrightness, to->mColorXfm.mBrightness, pct, mColorXfm.mBrightness);
    ::Interp(from->mColorXfm.mHue, to->mColorXfm.mHue, pct, mColorXfm.mHue);
    ::Interp(from->mColorXfm.mSaturation, to->mColorXfm.mSaturation, pct, mColorXfm.mSaturation);
    ::Interp(from->mColorXfm.mLightness, to->mColorXfm.mLightness, pct, mColorXfm.mLightness);
    ::Interp(from->mColorXfm.mContrast, to->mColorXfm.mContrast, pct, mColorXfm.mContrast);
    ::Interp(from->mColorXfm.mLevelInLo, to->mColorXfm.mLevelInLo, pct, mColorXfm.mLevelInLo);
    ::Interp(from->mColorXfm.mLevelInHi, to->mColorXfm.mLevelInHi, pct, mColorXfm.mLevelInHi);
    ::Interp(from->mColorXfm.mLevelOutLo, to->mColorXfm.mLevelOutLo, pct, mColorXfm.mLevelOutLo);
    ::Interp(from->mColorXfm.mLevelOutHi, to->mColorXfm.mLevelOutHi, pct, mColorXfm.mLevelOutHi);
    mColorXfm.AdjustColorXfm();

    // Gradient map
    ::Interp(from->mGradientMapOpacity, to->mGradientMapOpacity, pct, mGradientMapOpacity);
    ::Interp(from->mGradientMapIndex, to->mGradientMapIndex, pct, mGradientMapIndex);
    ::Interp(from->mGradientMapStart, to->mGradientMapStart, pct, mGradientMapStart);
    ::Interp(from->mGradientMapEnd, to->mGradientMapEnd, pct, mGradientMapEnd);

    // Refraction
    ::Interp(from->mRefractDist, to->mRefractDist, pct, mRefractDist);
    ::Interp(from->mRefractScale, to->mRefractScale, pct, mRefractScale);
    ::Interp(from->mRefractPanning, to->mRefractPanning, pct, mRefractPanning);
    ::Interp(from->mRefractVelocity, to->mRefractVelocity, pct, mRefractVelocity);
    ::Interp(from->mRefractAngle, to->mRefractAngle, pct, mRefractAngle);

    // Motion blur
    ::Interp(from->mMotionBlurBlend, to->mMotionBlurBlend, pct, mMotionBlurBlend);
    ::Interp(from->mMotionBlurWeight, to->mMotionBlurWeight, pct, mMotionBlurWeight);

    // Chromatic aberration
    ::Interp(from->mChromaticAberrationOffset, to->mChromaticAberrationOffset, pct, mChromaticAberrationOffset);

    // Vignette
    ::Interp(from->mVignetteColor, to->mVignetteColor, pct, mVignetteColor);
    ::Interp(from->mVignetteIntensity, to->mVignetteIntensity, pct, mVignetteIntensity);

    // DC3-specific members
    ::Interp(from->mHueTarget, to->mHueTarget, pct, mHueTarget);
    ::Interp(from->mHueFocus, to->mHueFocus, pct, mHueFocus);
    ::Interp(from->mBlendAmount, to->mBlendAmount, pct, mBlendAmount);
    ::Interp(from->mBrightnessPower, to->mBrightnessPower, pct, mBrightnessPower);

    // Flicker bounds
    ::Interp(from->mFlickerTimeBounds, to->mFlickerTimeBounds, pct, mFlickerTimeBounds);
    ::Interp(from->mFlickerModBounds, to->mFlickerModBounds, pct, mFlickerModBounds);

    // Hall of time — copy from 'from' if it has a non-zero rate, otherwise zero
    if (from->HallOfTime()) {
        mHallOfTimeType = from->mHallOfTimeType;
        mHallOfTimeRate = from->mHallOfTimeRate;
        mHallOfTimeColor = from->mHallOfTimeColor;
        mHallOfTimeMix = from->mHallOfTimeMix;
    } else {
        mHallOfTimeRate = 0.0f;
    }
}
