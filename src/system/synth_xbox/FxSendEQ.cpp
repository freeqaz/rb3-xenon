#include "FxSendEQ.h"
#include "FxSend.h"

FxSendEQ360::FxSendEQ360() : FxSend360(this) {}

FxSendEQ360::~FxSendEQ360() {}

void FxSendEQ360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendEQ360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendEQ360::UpdateMix() { FxSend360::UpdateVolumes(); }
