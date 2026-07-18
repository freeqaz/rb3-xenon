#pragma once
#include "obj/Object.h"
#include "synth/ADSR.h"
#include "synth/FxSend.h"
#include "synth/Pollable.h"

class PlayableSample : public SynthPollable {
public:
    virtual void Play(float) = 0;
    virtual void Stop(bool) = 0;
    virtual void Pause(bool) = 0;
    virtual bool DonePlaying() = 0;
    virtual void SetADSR(const ADSRImpl &) = 0;
    virtual void SetEventReceiver(Hmx::Object *) = 0;
    virtual Hmx::Object *GetEventReceiver() = 0;
    virtual void EndLoop() = 0;
    virtual float ElapsedTime() = 0;
};
