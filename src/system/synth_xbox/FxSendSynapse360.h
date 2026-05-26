#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendSynapse.h"

class FxSendSynapse360 : public FxSendSynapse, public FxSend360 {
public:
    virtual ~FxSendSynapse360();
    OBJ_CLASSNAME(FxSendSynapse360)
    OBJ_SET_TYPE(FxSendSynapse360)
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendSynapse360)

    FxSendSynapse360();

protected:
    virtual IUnknown *CreateFx();
};
