#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendPitchShift.h"

class FxSendPitchShift360 : public FxSendPitchShift, public FxSend360 {
public:
    OBJ_CLASSNAME(FxSendPitchShift)
    OBJ_SET_TYPE_ENGINE(FxSendPitchShift360)
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendPitchShift360)
    FXSEND360_NEW(FxSendPitchShift)

    FxSendPitchShift360() : FxSend360(this) {}

protected:
    virtual IUnknown *CreateFx();
};
