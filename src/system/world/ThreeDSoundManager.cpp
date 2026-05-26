#include "world/ThreeDSoundManager.h"
#include "macros.h"
#include "math/Mtx.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "synth/Sound.h"
#include "synth/ThreeDSound.h"
#include "ui/PanelDir.h"
#include "world/Dir.h"
#include <cmath>
#include <float.h>
#include <string.h>

ThreeDSoundManager::ThreeDSoundManager(WorldDir *dir)
    : mParent(dir), mSounds(dir), mListener(dir), mListenerDirty(0), mDopplerPower(1) {}

ThreeDSoundManager::~ThreeDSoundManager() {}

void ThreeDSoundManager::SyncObjects() {
    ObjPtrList<ThreeDSound> sounds(mParent);
    HarvestSounds(mParent, sounds);
    FOREACH (it, sounds) {
        mSounds.remove(*it);
    }
    FOREACH (it, mSounds) {
        (*it)->Stop(nullptr, false);
    }
    mSounds = sounds;
}

void ThreeDSoundManager::HarvestSounds(ObjectDir *dir, ObjPtrList<ThreeDSound> &sounds) {
    MILO_ASSERT(dir, 0x31);
    for (ObjDirItr<ThreeDSound> it(dir, true); it != nullptr; ++it) {
        sounds.push_back(&*it);
        MILO_NOTIFY(
            "Warning, found ThreeDSound object %s in %s!", it->Name(), PathName(dir)
        );
    }
}

void ThreeDSoundManager::Poll() {
    START_AUTO_TIMER("sound_mgr_poll");
    RndTransformable *listener = mListener;
    if (!listener) {
        listener = mParent->Cam();
    }
    if (listener) {
        const Transform &listenerXfm = listener->WorldXfm();
        bool listenerMoved = listenerXfm != mLastListenerXfm;
        float dt = TheTaskMgr.DeltaSeconds();
        float invDt = 0.0f;
        if (dt != 0.0f) {
            invDt = 1.0f / dt;
        }
        int loopCount = 0;
        FOREACH (it, mSounds) {
            if ((listenerMoved || (*it)->HasMoved() || (*it)->StartedPlaying())
                && (*it)->IsPlaying()) {
                float distance, radius;
                CalculateDistance(*it, listenerXfm, distance, radius);
                if (!(*it)->mLoop || distance > (*it)->mSilenceDistance) {
                    (*it)->SetDistance(distance, radius);
                } else {
                    if (loopCount == 100) {
                        MILO_NOTIFY_ONCE(
                            "Over %d looping 3D sounds are currently trying to play - "
                            "ignoring some",
                            100
                        );
                        (*it)->SetDistance(FLT_MAX, FLT_MAX);
                    } else {
                        (*it)->SetDistance(distance, radius);
                        loopCount++;
                    }
                }
                if ((*it)->mPanEnabled) {
                    float angle = CalculateAngle(*it, listenerXfm);
                    (*it)->SetAngle(angle);
                }
                if ((*it)->mDopplerEnabled && !mListenerDirty) {
                    float doppler =
                        CalculateDoppler(*it, listenerXfm, dt, invDt, distance);
                    (*it)->SetDoppler(doppler);
                }
            }
            (*it)->SaveWorldXfm();
        }
        mListenerDirty = false;
        memcpy(&mLastListenerXfm, &listenerXfm, sizeof(Transform));
    }
}

float ThreeDSoundManager::CalculateAngle(ThreeDSound *sound, const Transform &xfm) {
    Vector3 vout;
    MultiplyTranspose(sound->WorldXfm().v, xfm, vout);
    return atan2(vout.x, vout.y);
}

void ThreeDSoundManager::CalculateDistance(
    ThreeDSound *sound, const Transform &xfm, float &f1, float &f2
) {
    Vector3 vdiff;
    Subtract(xfm.v, sound->WorldXfm().v, vdiff);
    f1 = Length(vdiff);
    if (IsNaN(f1)) {
        MILO_NOTIFY("Sound %s is NaN meters away", PathName(sound));
        f1 = 0;
        f2 = 0;
    } else if (sound->GetShape() == 1) {
        Vector3 v50;
        Normalize(sound->WorldXfm().m.x, v50);
        Vector3 vneg;
        Negate(v50, vneg);
        float fscalar = Dot(vneg, vdiff);
        Vector3 vtotal;
        ScaleAdd(vdiff, v50, fscalar, vtotal);
        f2 = Length(vtotal);
    }
}

float ThreeDSoundManager::CalculateDoppler(
    ThreeDSound *sound, const Transform &listenerXfm, float dt, float invDt, float distance
) {
    Vector3 listenerVel;
    Subtract(listenerXfm.v, mLastListenerXfm.v, listenerVel);

    Vector3 soundVel;
    sound->GetVelocity(soundVel);

    const Transform &soundXfm = sound->WorldXfm();
    Vector3 dir;
    Subtract(listenerXfm.v, soundXfm.v, dir);

    float invDist = 0.0f;
    if (distance != 0.0f) {
        invDist = 1.0f / distance;
    }

    float listenerApproach = (Dot(listenerVel, dir) * (invDist * invDt));
    float soundApproach = (Dot(soundVel, dir) * (invDist * invDt));

    static const float kDopplerConsts[3] = { 340.29f, 0.7491535544395447f, 1.3348398208618164f };
    float speedOfSound = kDopplerConsts[0];
    listenerApproach = -listenerApproach;
    float ratio = (listenerApproach + speedOfSound) / (soundApproach + speedOfSound);
    float result = powf(ratio, mDopplerPower);

    // Clamp to +-5 semitones
    float kMaxDoppler = kDopplerConsts[2];
    float kMinDoppler = kDopplerConsts[1];
    result = Min(result, kMaxDoppler);
    result = Max(result, kMinDoppler);
    return result;
}
