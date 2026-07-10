#include "FxSendCompress.h"
#include "FxSend.h"
#include "dsp/StandardEffect.h"
#include "synth/CompressionEffect.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendCompress360::FxSendCompress360() : FxSend360(this) {}

FxSendCompress360::~FxSendCompress360() {}

void FxSendCompress360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendCompress360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendCompress360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendCompress360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    CompressionEffect::Params p;
    p.unk0 = mBypass;
    p.mThresholdDb = mThresholdDB;
    p.mRatio = mRatio;
    p.mOutputGainDb = mOutputLevel;
    p.mAttackTime = mAttack;
    p.mReleaseTime = mRelease;
    p.mPostGain = mExpRatio;
    p.mPeakAttackTime = mExpAttack;
    p.mPeakReleaseTime = mExpRelease;
    p.mGateThreshDb = mGateThresholdDB;
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendCompress360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<CompressionEffect>());
}
