#pragma once
#include "obj/Object.h"
#include "synth/Faders.h"
#include "synth/FxSend.h"
#include "synth/Pollable.h"
#include "synth/SampleZone.h"
#include "synth/SampleInst.h"
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"

class MidiInstrument;

class NoteVoiceInst : public Hmx::Object {
public:
    NoteVoiceInst(
        MidiInstrument *owner,
        SampleZone *zone,
        unsigned char triggerNote,
        unsigned char ratio,
        int durFramesLeft,
        int glideID,
        float fineTuneCents
    );
    virtual ~NoteVoiceInst();
    virtual void Start();
    virtual void Stop();
    virtual bool IsRunning();
    virtual bool Started() { return mStarted; }
    virtual bool Stopped() { return mStopped; }
    virtual void SetTranspose(float);
    virtual void UpdateVolume();
    virtual void UpdatePan();
    virtual void SetPan(float);
    virtual void SetVolume(float);

    void Poll();
    void GlideToNote(unsigned char, int);
    void SetFineTune(float);
    int GlideID() const { return mGlideID; }
    SampleInst *Sample() const { return mSample; }

    POOL_OVERLOAD(NoteVoiceInst, 0x1E);

private:
    float getCalculatedSpeed(float);

    SampleInst *mSample; // 0x28
    float mVolume; // 0x30
    float mStartProgress; // 0x34
    unsigned char mTriggerNote; // 0x38
    unsigned char mCenterNote; // 0x35
    bool mStarted; // 0x3a
    bool mStopped; // 0x3b
    int mGlideID; // 0x3c
    int mGlideFrames; // 0x3c
    float mGlideToNote; // 0x40
    float mGlideFromNote; // 0x44
    int mGlideFramesLeft; // 0x48
    float mFineTune; // 0x4c
    int mDurationFramesLeft; // 0x50
    MidiInstrument *mOwner; // 0x58
};

/** "Basic sound effect object.  Plays several samples with a given volume, pan,
 * transpose, and envelope settings." */
// Retail RB3 does NOT derive SynthPollable (rb3-Wii oracle: MidiInstrument is
// driven by a MidiInstrumentMgr, with a NON-virtual Poll()). DC3 (newer) added
// the SynthPollable base + global poll list, which inserts a vfptr+mItr+mIsActive
// (0xc) before the first member and shifts every member down. Gate the DC3 form
// behind HX_NATIVE so the native runtime keeps its working poll loop while the
// matching build reproduces retail's layout/vtable.
class MidiInstrument : public Hmx::Object
#ifdef HX_NATIVE
                       ,
                       public SynthPollable
#endif
{
public:
    // Hmx::Object
    virtual ~MidiInstrument();
    OBJ_CLASSNAME(MidiInstrument);
    OBJ_SET_TYPE_ENGINE(MidiInstrument);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
#ifdef HX_NATIVE
    // SynthPollable
    virtual void SynthPoll();
#else
    void Poll();
#endif

    FxSend *GetSend() const { return mSend; }
    FaderGroup &Faders() { return mFaders; }
    float GetReverbMixDb() const { return mReverbMixDb; }
    bool GetReverbEnable() const { return mReverbEnable; }

    void SetReverbMixDb(float);
    void SetReverbEnable(bool);
    void SetSend(FxSend *);
    NoteVoiceInst *MakeNoteInst(SampleZone *, unsigned char, unsigned char, int, int);
    void Pause(bool);
    void SetFineTune(float);
    void PressNote(unsigned char, unsigned char, int, int);
    void ReleaseNote(unsigned char);
    void PlayNote(unsigned char, unsigned char, int);

    OBJ_MEM_OVERLOAD(0x71);
    NEW_OBJ(MidiInstrument)
private:
    void StartSample(unsigned char, unsigned char, int, int);

protected:
    MidiInstrument();

    ObjVector<SampleZone> mMultiSampleMap; // 0x28
    int mPatchNumber; // 0x38
    /** "Effect chain to use" */
    ObjPtr<FxSend> mSend; // 0x4c
    /** "Reverb send for this instrument". Ranges from -96 to 20. */
    float mReverbMixDb; // 0x48
    /** "Enable reverb send" */
    bool mReverbEnable; // 0x4c
    ObjPtrList<NoteVoiceInst> mActiveVoices; // 0x50
    /** "Faders affecting this sound effect" */
    FaderGroup mFaders; // 0x7c
    float mFineTuneCents; // 0x7c
};
