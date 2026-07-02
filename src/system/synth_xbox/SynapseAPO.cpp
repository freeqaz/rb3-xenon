#include "SynapseAPO.h"
#include "Synapse_dsp.h"
#include "oggvorbis/VorbisMem.h"

extern "C" void XMemSet(void* dst, int val, int size);

struct XAPO_REGISTRATION_PROPERTIES {
    char data[0x58];
};

namespace DSP {

SynapseAPO::SynapseAPO() : ATG::CSampleXAPOBase<SynapseAPO, SynapseAPOParams>(), mSynapse(nullptr) {
    SetSamplingRate(48000.0f);
}

SynapseAPO::~SynapseAPO() {
    // Retail destroys in place and frees through the ogg allocator
    // (target: bl ??1Synapse then bl OggFree, no operator delete).
    Synapse::Synapse* synapse = mSynapse;
    if (synapse) {
        synapse->~Synapse();
        OggFree(synapse);
    }
}

void SynapseAPO::SetSamplingRate(float rate) {
    Synapse::Synapse* prevSynapse = mSynapse;
    if (prevSynapse) {
        delete prevSynapse;
    }
    mSynapse = new Synapse::Synapse(rate);
}

void SynapseAPO::OnSetParameters(const SynapseAPOParams& params) {}

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
