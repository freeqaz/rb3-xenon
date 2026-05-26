#pragma once

struct MeterEffectParams {
    int unknown;
};

// Stub for the parent template class
template <typename Derived, typename Params>
class CSampleXAPOBase {
protected:
    CSampleXAPOBase() {}
    virtual ~CSampleXAPOBase() {}
public:
    virtual void SetParameters(const void *pParameters, unsigned int cbParameters) {}
    virtual void GetParameters(void *pParameters, unsigned int cbParameters) {}
};

class MeterEffect : public CSampleXAPOBase<MeterEffect, MeterEffectParams> {
public:
    MeterEffect();
    virtual ~MeterEffect();
    virtual void OnSetParameters(const MeterEffectParams &params);
    virtual void DoProcess(const MeterEffectParams &params, unsigned int *pInputSamples, float &fOutputLevel, unsigned int uNumInputFrames, unsigned int uNumInputChannels);

protected:
    char padding[0x50];  // Padding to match object layout
    int unk_90;
    float floats[6];
};
