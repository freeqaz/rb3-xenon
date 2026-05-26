#include "synth/ThreeDSound.h"
#include "math/Decibels.h"
#include "math/Easing.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "synth/Utl.h"
#include "utl/BinStream.h"

const float sSpeedCaps[2] = { 0.00390625f, 4.0f };

ThreeDSound::ThreeDSound()
    : mIsLooping(0), unk195(0), mDelayedVolume(0), mDelayedPan(0), mDelayedTranspose(0), mDelayedOwner(0), mDelayMs(0),
      mFalloffType(kEaseLinear), mFalloffParameter(2), mMinFalloffDistance(10),
      mSilenceDistance(100), mDopplerEnabled(1), mPanEnabled(1), mShape(0), mRadius(10),
      unk20c(100), unk210(0), mDopplerPower(1), mStartedPlaying(0) {
    mDistanceFader = static_cast<Fader *>(Fader::NewObject());
    mFaders.Add(mDistanceFader);
    CalculateFaderVolume();
    Vector3 v(mMinFalloffDistance, mSilenceDistance, 1);
    SetLocalScale(this, v);
}

BEGIN_HANDLERS(ThreeDSound)
    HANDLE_SUPERCLASS(Sound)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(ThreeDSound)
    SYNC_PROP_SET(enable_doppler, mDopplerEnabled, EnableDoppler(_val.Int()))
    SYNC_PROP_SET(enable_pan, mPanEnabled, EnablePan(_val.Int()))
    SYNC_PROP_SET(falloff_type, (int &)mFalloffType, SetFalloffType((EaseType)_val.Int()))
    SYNC_PROP_SET(falloff_parameter, mFalloffParameter, SetFalloffParameter(_val.Float()))
    SYNC_PROP_SET(
        min_falloff_distance, mMinFalloffDistance, SetMinFalloffDistance(_val.Float())
    )
    SYNC_PROP_SET(silence_distance, mSilenceDistance, SetSilenceDistance(_val.Float()))
    SYNC_PROP_SET(shape, mShape, SetShape(_val.Int()))
    SYNC_PROP_SET(radius, mRadius, SetRadius(_val.Float()))
    SYNC_PROP(doppler_power, mDopplerPower)
    SYNC_SUPERCLASS(Sound)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(ThreeDSound)
    SAVE_REVS(6, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    mFaders.Remove(mDistanceFader);
    SAVE_SUPERCLASS(Sound)
    mFaders.Add(mDistanceFader);
    bs << mFalloffType;
    bs << mFalloffParameter;
    bs << mMinFalloffDistance;
    bs << mSilenceDistance;
    bs << mDopplerEnabled;
    bs << mPanEnabled;
    bs << mShape;
    bs << mRadius;
    bs << unk195;
    bs << mDopplerPower;
END_SAVES

BEGIN_COPYS(ThreeDSound)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    mFaders.Remove(mDistanceFader);
    COPY_SUPERCLASS(Sound)
    mFaders.Add(mDistanceFader);
    CREATE_COPY(ThreeDSound)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mFalloffType)
            COPY_MEMBER(mFalloffParameter)
            COPY_MEMBER(mMinFalloffDistance)
            COPY_MEMBER(mSilenceDistance)
            COPY_MEMBER(mDopplerEnabled)
            COPY_MEMBER(mPanEnabled)
            COPY_MEMBER(mShape)
            COPY_MEMBER(mRadius)
            COPY_MEMBER(unk195)
            COPY_MEMBER(mDopplerEnabled)
            COPY_MEMBER(mDopplerPower)
        }
    END_COPYING_MEMBERS
    Vector3 v(mMinFalloffDistance, mSilenceDistance, mRadius);
    SetLocalScale(this, v);
    CalculateFaderVolume();
END_COPYS

INIT_REVS(6, 0)

BEGIN_LOADS(ThreeDSound)
    LOAD_REVS(bs)
    ASSERT_REVS(6, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    if (d.rev >= 2) {
        LOAD_SUPERCLASS(RndTransformable)
    }
    LOAD_SUPERCLASS(Sound)
    d >> (int &)mFalloffType;
    d >> mFalloffParameter;
    d >> mMinFalloffDistance;
    d >> mSilenceDistance;
    unk20c = mSilenceDistance;
    if (d.rev < 2) {
        ObjPtr<RndTransformable> t(this);
        d >> t;
        SetTransParent(t, false);
    }
    if (d.rev >= 1) {
        d >> mDopplerEnabled;
    }
    if (d.rev >= 3) {
        d >> mPanEnabled;
    }
    if (d.rev >= 4) {
        d >> mShape;
        d >> mRadius;
    }
    if (d.rev >= 5) {
        d >> unk195;
    }
    if (d.rev >= 6) {
        d >> mDopplerPower;
    }
    SetLocalScale(this, Vector3(mMinFalloffDistance, mSilenceDistance, mRadius));
    CalculateFaderVolume();
END_LOADS

void ThreeDSound::Highlight() {
    if (mShape >= 1U) {
        if (mShape != 1U) {
            MILO_FAIL("Trying to drawn unknown sound shape %d\n", mShape);
        } else {
            Transform xfm = WorldXfm();
            Vector3 vscale;
            MakeScale(xfm.m, vscale);
            vscale.x = 1.0f / vscale.x;
            vscale.y = 1.0f / vscale.y;
            vscale.z = 1.0f / vscale.z;
            Scale(vscale, xfm.m, xfm.m);
            if (mMinFalloffDistance <= mRadius) {
                UtilDrawSphere(
                    WorldXfm().v, mMinFalloffDistance, Hmx::Color(1, 0, 0), nullptr
                );
            } else {
                UtilDrawCylinder(
                    xfm, mRadius, mMinFalloffDistance, Hmx::Color(1, 0, 0), 8
                );
            }
            if (mSilenceDistance <= mRadius) {
                UtilDrawSphere(
                    WorldXfm().v, mSilenceDistance, Hmx::Color(0, 1, 0), nullptr
                );
            } else {
                UtilDrawCylinder(xfm, mRadius, mSilenceDistance, Hmx::Color(0, 1, 0), 8);
            }
        }
    } else {
        UtilDrawSphere(WorldXfm().v, mMinFalloffDistance, Hmx::Color(1, 0, 0), nullptr);
        UtilDrawSphere(WorldXfm().v, mSilenceDistance, Hmx::Color(0, 1, 0), nullptr);
    }
}

void ThreeDSound::Play(
    float volume, float pan, float transpose, Hmx::Object *o4, float delayMs
) {
    mStartedPlaying = true;
    if (mLoop && !unk195) {
        mDelayedVolume = volume;
        mDelayedOwner = o4;
        mDelayedPan = pan;
        mIsLooping = true;
        mDelayedTranspose = transpose;
        mDelayMs = delayMs;
    } else {
        Sound::Play(volume, pan, transpose, o4, delayMs);
    }
}

void ThreeDSound::Stop(Hmx::Object *obj, bool b2) {
    mIsLooping = false;
    Sound::Stop(obj, b2);
}

bool ThreeDSound::IsPlaying() const {
    return !mIsLooping && (!mSamples.empty() || !mDelayArgs.empty());
}

void ThreeDSound::SaveWorldXfm() { mSavedWorldTransform = WorldXfm(); }

bool ThreeDSound::HasMoved() { return WorldXfm() != mSavedWorldTransform; }

void ThreeDSound::EnablePan(bool enable) {
    mDistanceFader->SetPan(0);
    mPanEnabled = enable;
    BroadcastPropertyChange("fader_pan");
}

void ThreeDSound::GetVelocity(Vector3 &vel) { Subtract(WorldXfm().v, mSavedWorldTransform.v, vel); }

void ThreeDSound::SetAngle(float radians) {
    MILO_ASSERT(radians >= -PI && radians <= PI, 0x191);
    if (mPanEnabled && !Sound::DisablePan(nullptr)) {
        mDistanceFader->SetPan(std::sin(radians));
        BroadcastPropertyChange("fader_pan");
    }
}

void ThreeDSound::SetDoppler(float doppler) {
    float powed = std::pow(doppler, mDopplerPower);
    if (powed > sSpeedCaps[1] || powed < sSpeedCaps[0]) {
        powed = 1.0f;
    }
    mDistanceFader->SetTranspose(CalcTransposeFromSpeed(powed));
}

void ThreeDSound::CalculateFaderVolume() {
    float vol;
    if (unk20c >= mSilenceDistance) {
        vol = -96.0f;
    } else if (unk20c <= mMinFalloffDistance) {
        vol = 0.0f;
    } else {
        switch (mShape) {
        case 1:
            if (unk210 > mRadius) {
                vol = -96.0f;
                goto done;
            }
        case 0:
            break;
        default:
            MILO_FAIL("Calculating volume for unknown shape %d\n", mShape);
            break;
        }
        float invRange = 1.0f / (mMinFalloffDistance - mSilenceDistance);
        float t = invRange * unk20c + (1.0f - mMinFalloffDistance * invRange);
        EaseType e = mFalloffType;
        MILO_ASSERT(e >= kEaseLinear && e <= kEaseQuarterHalfStairstep, 0x16B);
        float eased = gEaseFuncs[e](t, mFalloffParameter, 0);
        eased = Clamp(0.0f, 1.0f, eased);
        vol = RatioToDb(eased);
        vol = Max(vol, -96.0f);
    }
done:
    mDistanceFader->SetVolume(vol);
}

void ThreeDSound::SetDistance(float mDistance, float radius) {
    unk20c = mDistance;
    mStartedPlaying = false;
    MILO_ASSERT(!IsNaN(mDistance), 0x182);
    unk210 = radius;
    CalculateFaderVolume();
    if (mIsLooping && mDistanceFader->DuckedValue() > kDbSilence) {
        if (mSamples.empty() && mDelayArgs.empty()) {
            Sound::Play(
                mDelayedVolume, mDelayedPan, mDelayedTranspose, mDelayedOwner, mDelayMs
            );
        }
        return;
    }
    if (mIsLooping && mDistanceFader->DuckedValue() <= kDbSilence) {
        if (!mSamples.empty() || !mDelayArgs.empty()) {
            Sound::Stop(nullptr, true);
        }
    }
}
