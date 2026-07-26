#include "SynapseAPO.h"
#include "Synapse_dsp.h"
#include <string.h>

extern "C" void XMemSet(void* dst, int val, int size);

struct XAPO_REGISTRATION_PROPERTIES {
    char data[0x58];
};

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

void SynapseAPO::DoProcess(const SynapseAPOParams& params, unsigned int* arg1, float& arg2, unsigned int arg3, unsigned int arg4) {}

}  // namespace DSP

namespace ATG {

template <>
XAPO_REGISTRATION_PROPERTIES CSampleXAPOBase<DSP::SynapseAPO, DSP::SynapseAPOParams>::m_regProps;

template <typename Derived, typename Params>
CSampleXAPOBase<Derived, Params>::CSampleXAPOBase()
    : CXAPOParametersBase(&m_regProps, (unsigned char*)m_paramBlocks, sizeof(Params), 0)
{
    XMemSet(m_paramBlocks, 0, sizeof(Params) * 3);
}

template class CSampleXAPOBase<DSP::SynapseAPO, DSP::SynapseAPOParams>;

} // namespace ATG
