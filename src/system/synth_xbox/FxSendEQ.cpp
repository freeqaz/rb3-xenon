#include "FxSendEQ.h"
#include "FxSend.h"
#include "synth/EQEffect.h"
#include "dsp/StandardEffect.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendEQ360::FxSendEQ360() : FxSend360(this) {}

FxSendEQ360::~FxSendEQ360() {}

void FxSendEQ360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendEQ360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendEQ360::UpdateMix() { FxSend360::UpdateVolumes(); }

// Marshals the FxSendEQ base members into the XAPO EQEffect::Params (size 0x38). Our
// Params keeps its native-facing band field names; the target only cares about the
// per-offset store order: bypass@0x0, highFreqCutoff@0x4 .. lrMode@0x30, transition@0x34.
void FxSendEQ360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    EQEffect::Params p;
    // Statement order mirrors the target marshalling (FxSendEQ::SyncEffectParams):
    // bypass, high*, mid*, low*, highPassCutoff, lowPassCutoff, lowPassReso, highPassReso,
    // lrMode, transitionTime. The order pins the FPR assignment of the lowPass/highPass block.
    p.unk0 = mBypass;                  // bypass            @0x0
    p.mBand1Freq = mHighFreqCutoff;    // highFreqCutoff    @0x4
    p.mBand1Gain = mHighFreqGain;      // highFreqGain      @0x8
    p.mBand1Q = mMidFreqCutoff;        // midFreqCutoff     @0xc
    p.mBand2Freq = mMidFreqBandwidth;  // midFreqBandwidth  @0x10
    p.mBand2Gain = mMidFreqGain;       // midFreqGain       @0x14
    p.mBand2Q = mLowFreqCutoff;        // lowFreqCutoff     @0x18
    p.mBand3Freq = mLowFreqGain;       // lowFreqGain       @0x1c
    p.mBand4Freq = mHighPassCutoff;    // highPassCutoff    @0x28
    p.mBand3Gain = mLowPassCutoff;     // lowPassCutoff     @0x20
    p.mBand3Q = mLowPassReso;          // lowPassReso       @0x24
    p.mBand4Gain = mHighPassReso;      // highPassReso      @0x2c
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendEQ360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<EQEffect>());
}
