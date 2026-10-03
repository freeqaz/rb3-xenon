#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendSynapse.h"

class FxSendSynapse360 : public FxSendSynapse, public FxSend360 {
public:
    OBJ_CLASSNAME(FxSendSynapse)
    OBJ_SET_TYPE_ENGINE(FxSendSynapse360)
    // Inline: retail emits these with the vtable in Synth.cpp's code, right
    // after FxSendSynapse360::ClassName (0x82B5B1D0 / 1D8 / 1E0 = slots 27 / 22
    // / 26: `addi r3,r3,0x78; b` FxSend360::SyncEffectParams / Refresh /
    // UpdateVolumes). FxSendCompress360's out-of-line bodies are byte-identical
    // (its FxSend360 base is also at +0x78) and fold onto the same addresses.
    virtual void Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }
    virtual void UpdateMix() { FxSend360::UpdateVolumes(); }
    virtual void OnParametersChanged() { FxSend360::SyncEffectParams(); }
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;
    // Retail's FxSend360-table slot 2 is the `li r3,0; blr` fold, where
    // FxSend360's own is `li r3,1` (lane W16-OT).
    virtual bool IsStandard() const { return false; }

    NEW_OBJ(FxSendSynapse360)
    FXSEND360_NEW(FxSendSynapse)

    FxSendSynapse360() : FxSend360(this) {}

protected:
    virtual IUnknown *CreateFx();
};
