#include "FxSendWah.h"
#include "FxSend.h"

FxSendWah360::FxSendWah360() : FxSend360(this) {}

void FxSendWah360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendWah360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendWah360::UpdateMix() { FxSend360::UpdateVolumes(); }
