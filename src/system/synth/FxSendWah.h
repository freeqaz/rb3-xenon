#pragma once
#include "obj/Object.h"
#include "synth/FxSend.h"

/** "wah-wah effect" */
class FxSendWah : public FxSend {
public:
    OBJ_CLASSNAME(FxSendWah);
    OBJ_SET_TYPE_ENGINE(FxSendWah);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);

    OBJ_MEM_OVERLOAD(0x10);
    NEW_OBJ(FxSendWah)

#ifdef HX_NATIVE
public:
#else
protected:
#endif
    FxSendWah();

    /** "amount of resonance (1-10)" */
    float mResonance; // 0x54
    /** "high frequency peak of resonant filter (Hz)". Ranges from 100 to 10000. */
    float mUpperFreq; // 0x58
    /** "low frequency peak of resonant filter (Hz)". Ranges from 100 to 10000. */
    float mLowerFreq; // 0x5c
    /** "rate of LFO oscillations (Hz)". Ranges from 0.1 to 10. */
    float mLfoFreq; // 0x60
    /** "magic number (0-1)" */
    float mMagic; // 0x64
    /** "Post wah distortion amount". Ranges from 0 to 1. */
    float mDistAmount; // 0x68
    bool mAutoWah; // 0x6c
    float mFrequency; // 0x70
    /** "Dump". */
    float mDump; // 0x74
    /** "Sync wah to song tempo?" */
    bool mTempoSync; // 0x78
    /** "Note value of delay". Sync type options: (sixteenth eighth dotted_eighth quarter
     * dotted_quarter half whole) */
    Symbol mSyncType; // 0x7c
    /** "Tempo for delay; can be driven by game code". Ranges from 20 to 300. */
    float mTempo; // 0x80
    float mBeatFrac; // 0x84
};
