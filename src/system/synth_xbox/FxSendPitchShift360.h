#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendPitchShift.h"

class FxSendPitchShift360 : public FxSendPitchShift, public FxSend360 {
public:
    virtual ~FxSendPitchShift360();
    OBJ_CLASSNAME(FxSendPitchShift360)
    OBJ_SET_TYPE(FxSendPitchShift360)
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendPitchShift360)

    FxSendPitchShift360();

protected:
    virtual IUnknown *CreateFx();
};
