#include "FxSendDelay.h"
#include "FxSend.h"
#include "dsp/StandardEffect.h"
#include "synth/DelayEffect.h"
#include "synth/Utl.h"

FxSendDelay360::FxSendDelay360() : FxSend360(this) {}

FxSendDelay360::~FxSendDelay360() {}

void FxSendDelay360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendDelay360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendDelay360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendDelay360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    DelayEffect::Params p;
    if (mTempoSync) {
        p.mDelaySamples = 1 / CalcRateForTempoSync(mSyncType, mTempo);
    } else {
        p.mDelaySamples = mDelayTime;
    }
    p.unk0 = mBypass;
    p.mDecayDb = mGain;
    p.mWetPercent = mPingPongPct;
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendDelay360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<DelayEffect>());
}
