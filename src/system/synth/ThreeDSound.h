#pragma once
#include "math/Easing.h"
#include "math/Mtx.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include "synth/Faders.h"
#include "synth/Sound.h"
#include "utl/MemMgr.h"

/** "Sound effect object tied to a position.  Changes volume and pan based on the current
 * camera position." */
class ThreeDSound : public RndTransformable, public Sound {
    friend class ThreeDSoundManager;

public:
    // Hmx::Object
    virtual ~ThreeDSound() { RELEASE(mDistanceFader); }
    OBJ_CLASSNAME(ThreeDSound)
    OBJ_SET_TYPE(ThreeDSound)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    // RndTransformable
    virtual bool StartedPlaying() const { return mStartedPlaying; }
    virtual void Highlight();
    // SynthPollable
    virtual void Play(float, float, float, Hmx::Object *, float);
    virtual void Stop(Hmx::Object *, bool);
    virtual bool IsPlaying() const;

    OBJ_MEM_OVERLOAD(0x14);
    NEW_OBJ(ThreeDSound)

    void EnablePan(bool);
    void SaveWorldXfm();
    bool HasMoved();
    void GetVelocity(Vector3 &);
    void SetAngle(float);
    void SetDistance(float, float);
    void SetDoppler(float);

    void EnableDoppler(bool enable) {
        mDistanceFader->SetTranspose(0);
        mDopplerEnabled = enable;
    }
    void SetFalloffType(EaseType type) {
        mFalloffType = type;
        CalculateFaderVolume();
    }
    void SetFalloffParameter(float param) {
        mFalloffParameter = param;
        CalculateFaderVolume();
    }
    void SetMinFalloffDistance(float dist) {
        if (dist <= 0) {
            dist = 1.1754944E-38f;
        }
        mMinFalloffDistance = Min(dist, mSilenceDistance);
        CalculateFaderVolume();
    }
    void SetSilenceDistance(float dist) {
        if (dist <= 0) {
            dist = 1.1754944E-38f;
        }
        mSilenceDistance = Max(dist, mMinFalloffDistance);
        CalculateFaderVolume();
    }
    void SetShape(int shape) {
        mShape = shape;
        CalculateFaderVolume();
    }
    void SetRadius(float rad) {
        mRadius = rad;
        CalculateFaderVolume();
    }
    int GetShape() const { return mShape; }

private:
    void CalculateFaderVolume();

protected:
    ThreeDSound();

    bool mIsLooping; // 0x160
    bool unk195; // 0x161
    float mDelayedVolume; // 0x164
    float mDelayedPan; // 0x168
    float mDelayedTranspose; // 0x16c
    Hmx::Object *mDelayedOwner; // 0x170
    float mDelayMs; // 0x174
    /** "Equation used to determine falloff.
        See http://deki/Projects/Tool_Projects/Milo/Flow/Easing_equations" */
    EaseType mFalloffType; // 0x178
    /** "Optional parameter for falloff equation". Ranges from 0 to 100. */
    float mFalloffParameter; // 0x17c
    /** "Distance before any falloff is applied". Ranges from 0 to mSilenceDistance. */
    float mMinFalloffDistance; // 0x180
    /** "Distance at which this sound is silent".
        Ranges from mMinFalloffDistance to 2147483647. */
    float mSilenceDistance; // 0x184
    /** "Enable the doppler effect on this object" */
    bool mDopplerEnabled; // 0x188
    bool mPanEnabled; // 0x189
    int mShape; // 0x18c
    float mRadius; // 0x190
    Fader *mDistanceFader; // 0x194
    Transform mSavedWorldTransform; // 0x198
    float unk20c; // 0x1d8
    float unk210; // 0x1dc
    float mDopplerPower; // 0x1e0
    bool mStartedPlaying; // 0x1e4
};
