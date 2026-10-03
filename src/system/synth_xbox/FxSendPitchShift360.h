#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendPitchShift.h"

class FxSendPitchShift360 : public FxSendPitchShift, public FxSend360 {
public:
    OBJ_CLASSNAME(FxSendPitchShift)
    OBJ_SET_TYPE_ENGINE(FxSendPitchShift360)
    // Retail slots 22 / 26 / 27 are the FxSend360 forwarders, ICF-shared with
    // FxSendDistortion360 (base +0x58) (same FxSend360 base offset) -- lane W16-OT.
    virtual void Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }
    virtual void UpdateMix() { FxSend360::UpdateVolumes(); }
    virtual void OnParametersChanged() { FxSend360::SyncEffectParams(); }
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;
    // Retail's FxSend360-table slot 2 is the `li r3,0; blr` fold, where
    // FxSend360's own is `li r3,1` (lane W16-OT).
    virtual bool IsStandard() const { return false; }

    NEW_OBJ(FxSendPitchShift360)
    FXSEND360_NEW(FxSendPitchShift)

    FxSendPitchShift360() : FxSend360(this) {}

protected:
    virtual IUnknown *CreateFx();
};
