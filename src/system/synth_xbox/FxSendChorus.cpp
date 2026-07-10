#include "FxSendChorus.h"
#include "FxSend.h"
#include "dsp/StandardEffect.h"
#include "synth/FlangerEffect.h"
#include "math/Utl.h"
#include "synth/Utl.h"
#include "xdk/xaudio2/xaudio2.h"

FxSendChorus360::FxSendChorus360() : FxSend360(this) {}

FxSendChorus360::~FxSendChorus360() {}

void FxSendChorus360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendChorus360::UpdateMix() { FxSend360::UpdateVolumes(); }

void FxSendChorus360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendChorus360::SyncEffectParams(IXAudio2SubmixVoice *voice) const {
    FlangerEffect::Params p;
    p.mDelayMs = mDelayMs;
    float rate = mRate;
    if (mTempoSync) {
        rate = CalcRateForTempoSync(mSyncType, mTempo);
    }
    p.mRate = rate;
    float transpose = CalcTransposeFromSpeed(rate * mDelayMs * 0.0062831855f + 1.0f) * 100.0f;
    p.mDepth = Clamp(0.0f, 100.0f, mDepth / transpose * 100.0f);
    p.mFeedback = mFeedbackPct;
    p.mWet = mOffsetPct;
    p.unk0 = mBypass;
    voice->SetEffectParameters(0, &p, sizeof(p), 0);
}

IUnknown *FxSendChorus360::CreateFx() {
    return static_cast<CXAPOBase *>(new StandardEffect<FlangerEffect>());
}
