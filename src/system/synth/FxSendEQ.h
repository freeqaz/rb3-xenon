#pragma once
#include "obj/Object.h"
#include "synth/FxSend.h"

/** "An equalizer effect." */
class FxSendEQ : public FxSend {
public:
    virtual ~FxSendEQ();
    OBJ_CLASSNAME(FxSendEQ);
    OBJ_SET_TYPE_ENGINE(FxSendEQ);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0xF);
    NEW_OBJ(FxSendEQ)

#ifdef HX_NATIVE
public:
#else
protected:
#endif
    FxSendEQ();

    /** "High frequency cutoff, in Hz". Ranges from 0 to 24000. */
    float mHighFreqCutoff; // 0x54
    /** "High frequency gain, in dB". Ranges from -42 to 42. */
    float mHighFreqGain; // 0x58
    /** "Mid frequency cutoff, in Hz". Ranges from 0 to 24000. */
    float mMidFreqCutoff; // 0x5c
    /** "Mid frequency bandwidth, in Hz". Ranges from 0 to 24000. */
    float mMidFreqBandwidth; // 0x60
    /** "Mid frequency gain, in dB". Ranges from -42 to 42. */
    float mMidFreqGain; // 0x64
    /** "Low frequency cutoff, in Hz". Ranges from 0 to 24000. */
    float mLowFreqCutoff; // 0x68
    /** "Low frequency gain, in dB". Ranges from -42 to 42. */
    float mLowFreqGain; // 0x6c
    /** "Low pass filter cutoff, in Hz". Ranges from 20 to 20000. */
    float mLowPassCutoff; // 0x70
    /** "Low pass filter resonance, in dB". Ranges from -25 to 25. */
    float mLowPassReso; // 0x74
    /** "High pass filter cutoff, in Hz". Ranges from 20 to 20000. */
    float mHighPassCutoff; // 0x78
    /** "High pass filter resonance, in dB". Ranges from -25 to 25. */
    float mHighPassReso; // 0x7c
};
