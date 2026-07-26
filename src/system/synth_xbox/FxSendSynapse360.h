#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendSynapse.h"

class FxSendSynapse360 : public FxSendSynapse, public FxSend360 {
public:
    OBJ_CLASSNAME(FxSendSynapse)
    OBJ_SET_TYPE_ENGINE(FxSendSynapse360)
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendSynapse360)
    FXSEND360_NEW(FxSendSynapse)

    FxSendSynapse360() : FxSend360(this) {}

protected:
    virtual IUnknown *CreateFx();
};
