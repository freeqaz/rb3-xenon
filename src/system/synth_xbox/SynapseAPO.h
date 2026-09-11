#pragma once
#include "synth_xbox/FxSendSynapse.h"
#include "xdk/xaudio2/xapobase.h"

// This header used to redeclare CXAPOBase, IXAPOParameters, CXAPOParametersBase
// and a SECOND ATG::CSampleXAPOBase with a different DoProcess signature -- an
// ODR violation against xdk/xaudio2/xapobase.h, and factually wrong: retail
// .rdata carries exactly ONE ?$CSampleXAPOBase template, with thirteen
// instantiations, and its SynapseAPO one is
//   .?AV?$CSampleXAPOBase@VSynapseAPO@DSP@@USynapseAPOParams@2@@ATG@@

namespace DSP {

namespace Synapse {
class Synapse;
}

// CLSID read directly out of retail band.exe: the registration block at
// 0x82CA8768 opens with {03004D97-D165-4CC0-ABDD-6A98F04E6EB7}, and its ??__E
// dynamic initializer is at 0x82C436F0. DC3 carries the same uuid.
class __declspec(uuid("03004d97-d165-4cc0-abdd-6a98f04e6eb7")) SynapseAPO
    : public ATG::CSampleXAPOBase<SynapseAPO, SynapseAPOParams> {
public:
    SynapseAPO();
    virtual ~SynapseAPO();
    void SetSamplingRate(float rate);
    // Signature taken from retail's own mangled name, not from our old guess:
    // ?DoProcess@SynapseAPO@DSP@@UAAXABUSynapseAPOParams@2@PIAMII@Z
    //   => (const SynapseAPOParams &, float *__restrict, unsigned, unsigned)
    // We used to declare (const Params &, unsigned int *, float &, unsigned,
    // unsigned), which mangles differently and therefore paired with nothing.
    virtual void
    DoProcess(const SynapseAPOParams &, float *__restrict, unsigned int, unsigned int);

private:
    virtual void OnSetParameters(const SynapseAPOParams &);

    Synapse::Synapse *mSynapse; // at offset 0x168
    SynapseAPOParams mParams; // at offset 0x16c
};

} // namespace DSP
