#pragma once
#include "synth/SampleInst.h"
#include "synth_xbox/SynthSample.h"

class Voice;

class SampleInst360 : public SampleInst {
public:
    SampleInst360(SynthSample360 *, bool, int, int);
    virtual ~SampleInst360();

    // SampleInst pure virtuals
    virtual bool IsPlaying() const;
    virtual void SetFXCore(FXCore);
    virtual void Pause(bool);
    virtual void SetADSR(const ADSRImpl &);

    POOL_OVERLOAD(SampleInst360, 0x16)

protected:
    virtual void StartImpl();
    virtual void StopImpl(bool);
    virtual void SetVolumeImpl(float);
    virtual void SetPanImpl(float);
    virtual void SetSpeedImpl(float);

private:
    Voice *mVoice; // 0x54
    int unk_ac; // 0x58
};
