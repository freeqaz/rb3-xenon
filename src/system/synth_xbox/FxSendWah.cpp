#include "FxSendWah.h"
#include "FxSend.h"
#include "dsp/StandardEffect.h"
#include "synth/WahEffect.h"
#include "math/Utl.h"
#include "synth/Utl.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendWah360::FxSendWah360() : FxSend360(this) {}

void FxSendWah360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendWah360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendWah360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendWah360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    WahEffect::Params p;
    p.mGain = mResonance;     // resonance @0x4
    p.mFreqHi = mUpperFreq;   // upperFreq @0x8
    p.mFreqLo = mLowerFreq;   // lowerFreq @0xc
    p.mBandwidth = mMagic;    // magic     @0x14
    if (mTempoSync) {
        p.mResonance = CalcRateForTempoSync(mSyncType, mTempo); // lfoFreq @0x10
    } else {
        p.mResonance = mLfoFreq;
    }
    if (mTempoSync) {
        p.mSweepRate = Clamp(0.0f, 1.0f, mBeatFrac); // beatFrac @0x18
    } else {
        p.mSweepRate = -1;
    }
    p.mSweepRange = mDistAmount;      // distAmount @0x1c
    p.unk0 = mBypass;                 // bypass     @0x0
    p.mStaticSweep = mFrequency;      // frequency  @0x24
    p.mUnk28 = mDump;                 // dump       @0x28
    p.mEnvAmount = mAutoWah != false; // autoWah    @0x20
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendWah360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<WahEffect>());
}
