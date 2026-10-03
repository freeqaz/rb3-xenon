#pragma once
#include "synth/SampleInst.h"
#include "synth_xbox/SynthSample.h"

class Voice;

class SampleInst360 : public SampleInst {
public:
#ifdef HX_NATIVE
    SampleInst360(SynthSample360 *, bool, int, int);
#else
    SampleInst360(SynthSample360 *);
#endif
    virtual ~SampleInst360();

    // SampleInst pure virtuals
    virtual bool IsPlaying() const;
    virtual void SetFXCore(FXCore);
    virtual void Pause(bool);
    virtual void SetADSR(const ADSRImpl &);

    POOL_OVERLOAD(SampleInst360, 0x16)

protected:
    virtual void StartImpl();
    virtual void StopImpl();
    virtual void SetVolumeImpl(float);
    virtual void SetPanImpl(float);
    virtual void SetSpeedImpl(float);
    // Retail slots 32-34 (0x82B6E138 / 190 / 198) forward to the voice; ours
    // inherited SampleInst's empty bodies (lane W16-OT).
    virtual void SetSendImpl(FxSend *);
    virtual void SetReverbMixDbImpl(float);
    virtual void SetReverbEnableImpl(bool);

private:
    Voice *mVoice; // 0x54
    // sizeof 0x58: retail ??_G passes 0x58 to the pool delete.
};
