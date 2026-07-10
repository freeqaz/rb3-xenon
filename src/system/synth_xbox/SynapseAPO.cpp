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
    if (mSynapse) {
        delete mSynapse;
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
        if (mParams.bands[i].enabled != params.bands[i].enabled) {
            mSynapse->SetVoiceEnabled(i, params.bands[i].enabled);
        }
        if (mParams.bands[i].gain != params.bands[i].gain) {
            mSynapse->SetVoiceGain(i, params.bands[i].gain);
        }
        if (mParams.bands[i].freq != params.bands[i].freq) {
            mSynapse->SetVoiceTargetNote(i, params.bands[i].freq);
        }
        if (mParams.bands[i].q != params.bands[i].q) {
            mSynapse->SetVoiceTransposition(i, params.bands[i].q);
        }
        if (mParams.bands[i].coeff0 != params.bands[i].coeff0) {
            mSynapse->SetVoiceAmount(i, params.bands[i].coeff0);
        }
        if (mParams.bands[i].coeff1 != params.bands[i].coeff1) {
            mSynapse->SetVoiceProximityEffect(i, params.bands[i].coeff1);
        }
        if (mParams.bands[i].coeff2 != params.bands[i].coeff2) {
            mSynapse->SetVoiceProximityFocus(i, params.bands[i].coeff2);
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
