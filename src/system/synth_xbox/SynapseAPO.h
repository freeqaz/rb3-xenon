#pragma once
#include "synth_xbox/FxSendSynapse.h"

struct XAPO_REGISTRATION_PROPERTIES;

// CXAPOBase: base class for XAPO processing
// Offset 0x00, size 0x20 bytes (vtable + 0x1c data)
class CXAPOBase {
public:
    virtual void CXAPOBase_virt0();

private:
    char mCXAPOBasePad[0x1c];
};

// IXAPOParameters: interface for XAPO parameter get/set
class IXAPOParameters {
public:
    virtual void IXAPOParameters_virt0();
};

// CXAPOParametersBase: inherits CXAPOBase + IXAPOParameters
// Total size: 0x40 bytes
class CXAPOParametersBase : public CXAPOBase, public IXAPOParameters {
public:
    CXAPOParametersBase(const XAPO_REGISTRATION_PROPERTIES* pRegProps, unsigned char* pParamBlocks, unsigned int uParamBlockByteSize, int fProducer);
    virtual ~CXAPOParametersBase();

private:
    char mCXAPOParametersBasePad[0x1c]; // data from 0x24 to 0x3f
};

namespace ATG {

template <typename T, typename Params>
class CSampleXAPOBase : public CXAPOParametersBase {
protected:
    virtual ~CSampleXAPOBase() {}
    __declspec(noinline) CSampleXAPOBase();
    virtual void OnSetParameters(const Params& params) = 0;
    virtual void DoProcess(const Params& params, unsigned int* arg1, float& arg2, unsigned int arg3, unsigned int arg4) = 0;

private:
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
    Params m_paramBlocks[3]; // 3 parameter blocks for triple-buffering (offset 0x40)
    char m_extra[0x14]; // remaining state (offset 0x154 to 0x168)
};

} // namespace ATG

namespace DSP {

namespace Synapse {
class Synapse;
}

class SynapseAPO : public ATG::CSampleXAPOBase<SynapseAPO, SynapseAPOParams> {
public:
    SynapseAPO();
    virtual ~SynapseAPO();
    void SetSamplingRate(float rate);
    void OnSetParameters(const SynapseAPOParams& params);
    void DoProcess(const SynapseAPOParams& params, unsigned int* arg1, float& arg2, unsigned int arg3, unsigned int arg4);

private:
    Synapse::Synapse* mSynapse;   // at offset 0x168
    SynapseAPOParams mParams;     // at offset 0x16c
};

}  // namespace DSP
