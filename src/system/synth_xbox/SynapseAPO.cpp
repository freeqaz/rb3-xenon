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
        if (m.noteHz != t.noteHz) {
            mSynapse->SetVoiceTargetNote(i, t.noteHz);
        }
        if (m.transposition != t.transposition) {
            mSynapse->SetVoiceTransposition(i, t.transposition);
        }
        if (m.amount != t.amount) {
            mSynapse->SetVoiceAmount(i, t.amount);
        }
        if (m.proximityEffect != t.proximityEffect) {
            mSynapse->SetVoiceProximityEffect(i, t.proximityEffect);
        }
        if (m.proximityFocus != t.proximityFocus) {
            mSynapse->SetVoiceProximityFocus(i, t.proximityFocus);
        }
    }
    if (mParams.attackSmoothing != params.attackSmoothing) {
        mSynapse->SetAttackSmoothing(params.attackSmoothing);
    }
    if (mParams.releaseSmoothing != params.releaseSmoothing) {
        mSynapse->SetReleaseSmoothing(params.releaseSmoothing);
    }
    memcpy(&mParams, &params, sizeof(SynapseAPOParams));
}

void SynapseAPO::DoProcess(
    const SynapseAPOParams &, float *__restrict buffer, unsigned int numFrames, unsigned int
) {
    // retail 0x... : tail-calls Synapse::ProcessInPlace(numFrames, buffer)
    if (mSynapse) {
        mSynapse->ProcessInPlace(numFrames, buffer);
    }
}

}  // namespace DSP

// m_regProps and the CSampleXAPOBase ctor now come from the shared primary
// template in xdk/xaudio2/xapobase.h. m_regProps used to be declared here as an
// UNINITIALIZED explicit specialization, which emitted a zeroed .bss block and
// no ??__E dynamic initializer at all; retail has one, at 0x82C436F0.
