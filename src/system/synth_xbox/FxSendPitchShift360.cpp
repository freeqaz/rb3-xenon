#include "synth_xbox/FxSendPitchShift360.h"

FxSendPitchShift360::FxSendPitchShift360() {}
FxSendPitchShift360::~FxSendPitchShift360() {}

IUnknown *FxSendPitchShift360::CreateFx() { return nullptr; }

void FxSendPitchShift360::SyncEffectParams(IXAudio2SubmixVoice *) const {}
