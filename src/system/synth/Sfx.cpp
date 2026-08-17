#include "synth/Sfx.h"
#include "SampleInst.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "synth/MoggClip.h"
#include "synth/MoggClipMap.h"
#include "synth/Sequence.h"
#include "synth/Synth.h"
#include "synth/SynthSample.h"
#include "synth/Utl.h"

#pragma region SfxInst

SfxInst::SfxInst(Sfx *sfx)
    : SeqInst(sfx), mMoggClips(this, kObjListNoNull), mStartProgress(0) {
    FOREACH (it, sfx->SfxMaps()) {
        SampleInst *inst = nullptr;
        if (it->Sample()) {
            inst = it->Sample()->NewInst(false, 0, -1);
        }
        if (inst) {
            inst->SetBankVolume(it->Volume() + mRandVol);
            inst->SetBankPan(it->Pan() + mRandPan);
            inst->SetBankSpeed(CalcSpeedFromTranspose(it->Transpose() + mRandTp));
            inst->SetFXCore(it->GetFXCore());
            inst->SetADSR(it->ADSR());
            inst->SetSend(sfx->GetSend());
            inst->SetReverbMixDb(sfx->GetReverbMixDb());
            inst->SetReverbEnable(sfx->GetReverbEnable());
            mSamples.push_back(inst);
        }
    }
    FOREACH (it, sfx->MoggClipMaps()) {
        mMoggClips.push_back(&*it);
    }
}

SfxInst::~SfxInst() { DeleteAll(mSamples); }

void SfxInst::Stop() {
    FOREACH (it, mSamples) {
        (*it)->Stop(false);
    }
    FOREACH (it, mMoggClips) {
        MoggClip *clip = (*it)->GetMoggClip();
        if (clip) {
            clip->MoggClip::Stop();
        }
    }
}

bool SfxInst::IsRunning() {
    FOREACH (it, mSamples) {
        if ((*it)->IsPlaying())
            return true;
    }
    FOREACH (it, mMoggClips) {
        MoggClipMap *moggClipMap = *it;
        MoggClip *clip = moggClipMap->GetMoggClip();
        if (clip) {
            if (clip->HasStream())
                return true;
        }
    }
    return false;
}

void SfxInst::UpdateVolume() {
    FOREACH (it, mSamples) {
        (*it)->SetVolume(mVolume + mOwner->Faders().GetVal());
    }
    FOREACH (it, mMoggClips) {
        MoggClip *clip = (*it)->GetMoggClip();
        if (clip) {
            clip->MoggClip::SetVolume(mRandVol + mVolume + mOwner->Faders().GetVal());
        }
    }
}

void SfxInst::SetPan(float f1) {
    FOREACH (it, mSamples) {
        (*it)->SetPan(f1);
    }
}

void SfxInst::SetTranspose(float f1) { SetSpeed(CalcSpeedFromTranspose(f1)); }

void SfxInst::StartImpl() {
    FOREACH (it, mSamples) {
        (*it)->SetStartProgress(mStartProgress);
        (*it)->Play(0);
    }
    FOREACH (it, mMoggClips) {
        MoggClipMap *moggClipMap = *it;
        MoggClip *clip = moggClipMap->GetMoggClip();
        if (clip) {
            clip->SetVolume(moggClipMap->Volume());
            clip->SetupPanInfo(
                moggClipMap->Pan(), moggClipMap->PanWidth(), moggClipMap->Stereo()
            );
            clip->Play(0);
        }
    }
}

void SfxInst::Pause(bool b1) {
    FOREACH (it, mSamples) {
        (*it)->Pause(b1);
    }
    FOREACH (it, mMoggClips) {
        MoggClip *clip = (*it)->GetMoggClip();
        if (clip) {
            clip->Pause(b1);
        }
    }
}

void SfxInst::SetSend(FxSend *send) {
    FOREACH (it, mSamples) {
        (*it)->SetSend(send);
    }
}

void SfxInst::SetReverbMixDb(float db) {
    FOREACH (it, mSamples) {
        (*it)->SetReverbMixDb(db);
    }
}

void SfxInst::SetReverbEnable(bool enable) {
    FOREACH (it, mSamples) {
        (*it)->SetReverbEnable(enable);
    }
}

void SfxInst::SetSpeed(float speed) {
    FOREACH (it, mSamples) {
        (*it)->SetSpeed(speed);
    }
}

#pragma endregion
#pragma region Sfx

Sfx::Sfx()
    : mMaps(this), mMoggClipMaps(this), mSend(this), mReverbMixDb(kDbSilence),
      mReverbEnable(false), mSfxInsts(this) {
    mFaders.Add(TheSynth->MasterFader());
    mFaders.Add(TheSynth->SfxFader());
}

BEGIN_HANDLERS(Sfx)
    HANDLE_ACTION(add_map, mMaps.push_back())
    HANDLE_SUPERCLASS(Sequence)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(ADSRImpl)
    SYNC_PROP(attack_mode, (int &)o.mAttackMode)
    SYNC_PROP(attack_rate, o.mAttackRate)
    SYNC_PROP(decay_rate, o.mDecayRate)
    SYNC_PROP(sustain_mode, (int &)o.mSustainMode)
    SYNC_PROP(sustain_rate, o.mSustainRate)
    SYNC_PROP(sustain_level, o.mSustainLevel)
    SYNC_PROP(release_mode, (int &)o.mReleaseMode)
    SYNC_PROP(release_rate, o.mReleaseRate)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(SfxMap)
    SYNC_PROP(sample, o.mSample)
    SYNC_PROP(volume, o.mVolume)
    SYNC_PROP(pan, o.mPan)
    SYNC_PROP(transpose, o.mTranspose)
    SYNC_PROP(fx_core, (int &)o.mFXCore)
    SYNC_PROP(adsr, o.mADSR)
END_CUSTOM_PROPSYNC

BEGIN_CUSTOM_PROPSYNC(MoggClipMap)
    SYNC_PROP(moggclip, o.mMoggClip)
    SYNC_PROP(volume, o.mVolume)
    SYNC_PROP(pan, o.mPan)
    SYNC_PROP(pan_width, o.mPanWidth)
    SYNC_PROP(is_stereo, o.mIsStereo)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(Sfx)
    SYNC_PROP(sfxmaps, mMaps)
    SYNC_PROP(moggclip_maps, mMoggClipMaps)
    SYNC_PROP_SET(send, GetSend(), SetSend(_val.Obj<FxSend>()))
    SYNC_PROP_SET(reverb_mix_db, GetReverbMixDb(), SetReverbMixDb(_val.Float()))
    SYNC_PROP_SET(reverb_enable, GetReverbEnable(), SetReverbEnable(_val.Int()))
    SYNC_PROP(faders, mFaders)
    SYNC_SUPERCLASS(Sequence)
END_PROPSYNCS

BEGIN_SAVES(Sfx)
    SAVE_REVS(0xC, 0)
    SAVE_SUPERCLASS(Sequence)
    bs << mMaps;
    bs << mMoggClipMaps;
    bs << mSend;
    mFaders.Save(bs);
    bs << mReverbMixDb;
    bs << mReverbEnable;
END_SAVES

BEGIN_COPYS(Sfx)
    COPY_SUPERCLASS(Sequence)
    CREATE_COPY(Sfx)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mMaps)
            COPY_MEMBER(mMoggClipMaps)
        }
        COPY_MEMBER(mSend)
        COPY_MEMBER(mReverbMixDb)
        COPY_MEMBER(mReverbEnable)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0xD, 0)

BEGIN_LOADS(Sfx)
    // RB3-360 retail reads the rev as a plain int and compares it directly — no
    // BinStreamRev hi/lo split, no ASSERT_REVS. The child SfxMap/MoggClipMap
    // loaders read the parent rev via their own TU-statics (rb3-Wii idiom, assert
    // stripped). MILO_WARN is a retail no-op, so rev>0xC just skips the body.
    int rev;
    bs >> rev;
    if (rev > 0xC) {
        MILO_WARN("Can't load new Sfx");
    } else {
        if (rev >= 6) {
            Sequence::Load(bs);
        } else if (rev >= 2) {
            Hmx::Object::Load(bs);
        }
        SfxMap::gRev = rev;
        bs >> mMaps;
        if (rev >= 10) {
            MoggClipMap::sRev = rev;
            bs >> mMoggClipMaps;
        }
        if (rev > 4) {
            bs >> mSend;
            if (rev <= 7) {
                int x;
                bs >> x;
            }
        }
        if (rev >= 9) {
            mFaders.Load(bs);
        }
        if (rev >= 0xC) {
            bs >> mReverbMixDb >> mReverbEnable;
        }
    }
END_LOADS

SeqInst *Sfx::MakeInstImpl() {
    SfxInst *inst = new SfxInst(this);
    mSfxInsts.push_back(inst);
    return inst;
}

void Sfx::Pause(bool b1) {
    FOREACH (it, mSfxInsts) {
        (*it)->Pause(b1);
    }
}

void Sfx::SetSend(FxSend *send) {
    mSend = send;
    FOREACH (it, mSfxInsts) {
        (*it)->SetSend(mSend);
    }
}

void Sfx::SetReverbMixDb(float f) {
    mReverbMixDb = f;
    FOREACH (it, mSfxInsts) {
        (*it)->SetReverbMixDb(mReverbMixDb);
    }
}

void Sfx::SetReverbEnable(bool b) {
    mReverbEnable = b;
    FOREACH (it, mSfxInsts) {
        (*it)->SetReverbEnable(mReverbEnable);
    }
}


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
// ⚠ NATIVE: guarded because synth/Synth.cpp would otherwise be emitted TWICE in
// rb3-milo's link -- once through this edge and once through
// utl/CheatProvider.cpp. cmake/ScatterIncludes.cmake prunes an includee
// from standalone compilation, which handles ONE includer; with two, one of
// them has to go inert. X360 keeps both (it never links, so a duplicate
// COMDAT is invisible there -- and both placements are load-bearing for it).
#ifndef HX_NATIVE
#define gRev gRev_Synth
#define gAltRev gAltRev_Synth
#include "synth/Synth.cpp"
#undef gRev
#undef gAltRev
#endif

// sw2 scatter-include (default/Sfx <- world/CameraShot.cpp)
// ⚠ NATIVE: guarded because world/CameraShot.cpp would otherwise be emitted TWICE in
// rb3-milo's link -- once through this edge and once through
// world/LightPreset.cpp. cmake/ScatterIncludes.cmake prunes an includee
// from standalone compilation, which handles ONE includer; with two, one of
// them has to go inert. X360 keeps both (it never links, so a duplicate
// COMDAT is invisible there -- and both placements are load-bearing for it).
#ifndef HX_NATIVE
#define gRev gRev_CameraShot
#define gAltRev gAltRev_CameraShot
#include "world/CameraShot.cpp"
#undef gRev
#undef gAltRev
#endif
