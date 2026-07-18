#pragma once
#include "obj/Object.h"
#include "synth/ADSR.h"
#include "synth/FxSend.h"
#include "synth/MoggClipMap.h"
#include "synth/Sequence.h"
#include "synth/SynthSample.h"
#include "synth/SampleInst.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"

class SfxMap {
    friend bool PropSync(SfxMap &, DataNode &, DataArray *, int, PropOp);

public:
    SfxMap(Hmx::Object *);
    void Save(BinStream &) const;
    void Load(BinStream &);
    // RB3-360 retail reads the parent Sfx's rev via a TU-static (rb3-Wii idiom,
    // assert stripped) rather than threading a BinStreamRev wrapper. Sfx::Load
    // stashes its rev here right before `bs >> mMaps`.
    static int gRev;
    SynthSample *Sample() const { return mSample; }
    float Volume() const { return mVolume; }
    float Pan() const { return mPan; }
    float Transpose() const { return mTranspose; }
    FXCore GetFXCore() const { return mFXCore; }
    const ADSRImpl &ADSR() const { return mADSR; }

private:
    /** "Which sample to play" */
    ObjPtr<SynthSample> mSample; // 0x0
    /** "Volume in dB (0 is full volume, -96 is silence)" */
    float mVolume; // 0x14
    /** "Surround pan, between -4 and 4" */
    float mPan; // 0x18
    /** "Transpose in half steps" */
    float mTranspose; // 0x1c
    /** "Which core's digital FX should be used in playing this sample" */
    FXCore mFXCore; // 0x20
    ADSRImpl mADSR; // 0x24
};

BinStream &operator<<(BinStream &, const SfxMap &);
BinStream &operator>>(BinStream &, SfxMap &);

class SfxInst : public SeqInst {
public:
    SfxInst(class Sfx *);
    virtual ~SfxInst();
    virtual void Stop();
    virtual bool IsRunning();
    virtual void UpdateVolume();
    virtual void SetPan(float);
    virtual void SetTranspose(float);
    virtual void StartImpl();

    void Pause(bool);
    void SetSend(FxSend *);
    void SetReverbMixDb(float);
    void SetReverbEnable(bool);
    void SetSpeed(float);

    POOL_OVERLOAD(SfxInst, 0x4E);

private:
    // TU5 retail layout (matches rb3-Wii SfxInst): mSamples is the first
    // SfxInst-specific member at 0x40 (right after the SeqInst base). The retail
    // binary does NOT keep an mSfx back-pointer; instead SfxInst owns its own
    // ObjPtrList<MoggClipMap> populated from Sfx::MoggClipMaps() in the ctor, and
    // every clip loop iterates that list (verified: IsRunning loads the list head
    // from this+0x54 == ObjPtrList mNodes@+8, a next-pointer traversal, not an
    // ObjVector array walk via mSfx). ObjPtrList is 0x14 bytes → mStartProgress
    // lands at 0x60.
    std::vector<SampleInst *> mSamples; // 0x40
    ObjPtrList<MoggClipMap> mMoggClips; // 0x4c (mNodes head @ 0x54)
    float mStartProgress; // 0x60
};

/** "Legacy sound effect object.  Plays several samples with a given volume, pan,
 * transpose, and envelope settings." */
class Sfx : public Sequence {
public:
    // Hmx::Object
    OBJ_CLASSNAME(Sfx);
    OBJ_SET_TYPE(Sfx);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // Sequence
    virtual SeqInst *MakeInstImpl();
    // SynthPollable
    virtual void SynthPoll() { Sequence::SynthPoll(); }

    void SetSend(FxSend *);
    void SetReverbMixDb(float);
    void SetReverbEnable(bool);
    FxSend *GetSend() const { return mSend; }
    float GetReverbMixDb() const { return mReverbMixDb; }
    bool GetReverbEnable() const { return mReverbEnable; }
    ObjVector<SfxMap> &SfxMaps() { return mMaps; }
    ObjVector<MoggClipMap> &MoggClipMaps() { return mMoggClipMaps; }

    OBJ_MEM_OVERLOAD(0x1E);
    NEW_OBJ(Sfx);
    static void Init() { REGISTER_OBJ_FACTORY(Sfx) }

    void Pause(bool);

protected:
    Sfx();

    ObjVector<SfxMap> mMaps; // 0x80
    ObjVector<MoggClipMap> mMoggClipMaps; // 0x90
    /** "Effect chain to use" */
    ObjPtr<FxSend> mSend; // 0xa0
    /** "Reverb send for this sfx" */
    float mReverbMixDb; // 0xb4
    /** "Enable reverb send" */
    bool mReverbEnable; // 0xb8
    ObjPtrList<SfxInst> mSfxInsts; // 0xbc
};
