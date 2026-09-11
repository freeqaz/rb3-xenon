#include "SynapseAPO.h"
#include "Synapse_dsp.h"
#include <string.h>

// The local `struct XAPO_REGISTRATION_PROPERTIES { char data[0x58]; };` that used
// to sit here was a stub of the WRONG SIZE (the real one is 0x42c) and an ODR
// violation against xdk/xaudio2/xapo.h. Gone with the duplicate base classes.

namespace DSP {

SynapseAPO::SynapseAPO() : ATG::CSampleXAPOBase<SynapseAPO, SynapseAPOParams>(), mSynapse(nullptr) {
    SetSamplingRate(48000.0f);
}

SynapseAPO::~SynapseAPO() {
    Synapse::Synapse* prevSynapse = mSynapse;
    if (prevSynapse) {
        delete prevSynapse;
    }
}

void SynapseAPO::SetSamplingRate(float rate) {
    Synapse::Synapse* prevSynapse = mSynapse;
    if (prevSynapse) {
        delete prevSynapse;
    }
    mSynapse = new Synapse::Synapse(rate);
}

void SynapseAPO::OnSetParameters(const SynapseAPOParams& params) {
    for (unsigned int i = 0; i < 3; i++) {
        SynapseBand &m = mParams.bands[i];
        const SynapseBand &t = params.bands[i];
        if (m.enabled != t.enabled) {
            mSynapse->SetVoiceEnabled(i, t.enabled);
        }
        if (m.gain != t.gain) {
            mSynapse->SetVoiceGain(i, t.gain);
        }
        if (m.freq != t.freq) {
            mSynapse->SetVoiceTargetNote(i, t.freq);
        }
        if (m.q != t.q) {
            mSynapse->SetVoiceTransposition(i, t.q);
        }
        if (m.coeff0 != t.coeff0) {
            mSynapse->SetVoiceAmount(i, t.coeff0);
        }
        if (m.coeff1 != t.coeff1) {
            mSynapse->SetVoiceProximityEffect(i, t.coeff1);
        }
        if (m.coeff2 != t.coeff2) {
            mSynapse->SetVoiceProximityFocus(i, t.coeff2);
        }
    }
    if (mParams.lowCutoffFreq != params.lowCutoffFreq) {
        mSynapse->SetAttackSmoothing(params.lowCutoffFreq);
    }
    if (mParams.highCutoffFreq != params.highCutoffFreq) {
        mSynapse->SetReleaseSmoothing(params.highCutoffFreq);
    }
    memcpy(&mParams, &params, sizeof(SynapseAPOParams));
}

void SynapseAPO::DoProcess(
    const SynapseAPOParams &, float *__restrict, unsigned int, unsigned int
) {}

}  // namespace DSP

// m_regProps and the CSampleXAPOBase ctor now come from the shared primary
// template in xdk/xaudio2/xapobase.h. m_regProps used to be declared here as an
// UNINITIALIZED explicit specialization, which emitted a zeroed .bss block and
// no ??__E dynamic initializer at all; retail has one, at 0x82C436F0.
