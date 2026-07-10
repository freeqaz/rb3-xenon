#pragma once
#include "FxSend.h"
#include "obj/Object.h"
#include "synth/FxSendFlanger.h"
#include "xdk/xaudio2/xaudio2.h"

class FxSendFlanger360 : public FxSendFlanger, public FxSend360 {
public:
    // Hmx::Object
    virtual ~FxSendFlanger360();
    OBJ_CLASSNAME(FxSendFlanger)
    OBJ_SET_TYPE_ENGINE(FxSendFlanger360)
    virtual void Recreate(std::vector<FxSend *> &);
    virtual void UpdateMix();
    virtual void OnParametersChanged();

    // FxSendFlanger
    virtual void SyncEffectParams(IXAudio2SubmixVoice *) const;

    NEW_OBJ(FxSendFlanger360)

    FxSendFlanger360();

protected:
    virtual IUnknown *CreateFx();
};
