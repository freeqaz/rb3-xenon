#include "synth_xbox/FxSendSynapse360.h"

FxSendSynapse360::FxSendSynapse360() {}
FxSendSynapse360::~FxSendSynapse360() {}

IUnknown *FxSendSynapse360::CreateFx() { return nullptr; }

void FxSendSynapse360::SyncEffectParams(IXAudio2SubmixVoice *) const {}
