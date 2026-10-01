#include "synth_xbox/FxSendSynapse.h"
#include "synth_xbox/FxSendSynapse360.h"
#include "synth_xbox/SynapseAPO.h"

// Retail 0x82B6A160.
IUnknown *FxSendSynapse360::CreateFx() {
    return static_cast<CXAPOBase *>(new DSP::SynapseAPO());
}

// Retail 0x82B6A220. Voice 0 sings note 1. Voices 1 and 2 sing notes 2 and 3
// (detuned by 1/1.004 and 1.004) when those are set; otherwise they double
// note 1 (detuned by 1/1.0096 and 1.0096), audible only in unison-trio mode,
// and keep the proximity effect that the harmony voices drop.
void FxSendSynapse360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    DSP::SynapseAPOParams p;

    p.bands[0].enabled = true;
    p.bands[0].noteHz = mNote1Hz;
    p.bands[0].gain = 1.0f;
    p.bands[0].amount = mAmount;
    p.bands[0].proximityEffect = mProximityEffect;
    p.bands[0].proximityFocus = mProximityFocus;
    p.attackSmoothing = mAttackSmoothing;
    p.releaseSmoothing = mReleaseSmoothing;

    float gain = 1.0f;
    float note;
    p.bands[1].enabled = true;
    p.bands[1].amount = mAmount;
    p.bands[1].proximityFocus = mProximityFocus;
    if (mNote2Hz == 0.0f) {
        if (mUnisonTrio) {
            gain = 1.0f;
        } else {
            gain = 0.0f;
        }
        p.bands[1].proximityEffect = mProximityEffect;
        note = mNote1Hz * 0.9904912f;
    } else {
        p.bands[1].proximityEffect = 0.0f;
        note = mNote2Hz * 0.99601597f;
    }
    p.bands[1].noteHz = note;
    p.bands[1].gain = gain;

    gain = 1.0f;
    p.bands[2].enabled = true;
    p.bands[2].amount = mAmount;
    p.bands[2].proximityFocus = mProximityFocus;
    if (mNote3Hz == 0.0f) {
        if (!mUnisonTrio) {
            gain = 0.0f;
        }
        p.bands[2].proximityEffect = mProximityEffect;
        note = mNote1Hz * 1.0096f;
    } else {
        p.bands[2].proximityEffect = 0.0f;
        note = mNote3Hz * 1.004f;
    }
    p.bands[2].noteHz = note;
    p.bands[2].gain = gain;

    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}
