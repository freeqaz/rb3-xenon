#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "stl/_vector.h"
#include "synth/FxSend.h"
#include "synth/FxSendEQ.h"
#include "xdk/xapilibi/xbase.h"
#include "xdk/xaudio2/xaudio2.h"

class FxSendEQ360 : public FxSendEQ, public FxSend360 {
public:
    virtual ~FxSendEQ360();
    OBJ_CLASSNAME(FxSendEQ)
    OBJ_SET_TYPE_ENGINE(FxSendEQ360)
    virtual void Recreate(std::vector<FxSend *> &);
    virtual void UpdateMix();
    virtual void OnParametersChanged();
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendEQ360)
    FXSEND360_NEW(FxSendEQ)

    FxSendEQ360();

protected:
    virtual IUnknown *CreateFx();
};
