#include "synth_xbox/FxSendFlanger.h"
#include "FxSend.h"
#include "dsp/StandardEffect.h"
#include "synth/FlangerEffect.h"
#include "synth/Utl.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendFlanger360::FxSendFlanger360() : FxSend360(this) {}

FxSendFlanger360::~FxSendFlanger360() {}

void FxSendFlanger360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendFlanger360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendFlanger360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendFlanger360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    FlangerEffect::Params p;
    p.mDelayMs = mDelayMs;
    if (mTempoSync) {
        p.mRate = CalcRateForTempoSync(mSyncType, mTempo);
    } else {
        p.mRate = mRate;
    }
    p.mDepth = mDepthPct;
    p.mFeedback = mFeedbackPct;
    p.mWet = mOffsetPct;
    p.unk0 = mBypass;
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendFlanger360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<FlangerEffect>());
}
