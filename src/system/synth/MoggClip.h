#pragma once
#include "obj/Object.h"
#include "synth/Faders.h"
#include "synth/Pollable.h"
#include "synth/StandardStream.h"
#include "utl/FilePath.h"

class FileLoader;
class FxSend;

/** "Allows dynamic playback of Mogg-based audio clips, most notably crowd audio loops."
 */
class MoggClip : public Hmx::Object, public SynthPollable {
public:
    struct PanInfo {
        PanInfo(int, float);
        int channel;
        float panning;
    };

    virtual ~MoggClip();
    OBJ_CLASSNAME(MoggClip);
    OBJ_SET_TYPE_ENGINE(MoggClip);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ⛔ NONE of the eight below is virtual in retail, and they were the single
    // largest vtable defect in the tree: retail's MoggClip Hmx::Object-subobject
    // vtable (0x820f8f34) holds 21 slots -- exactly Object's set, ending at
    // FindPathName -- while ours held 29.  The eight surplus slots were these,
    // appended in declaration order as MoggClip's own NEW virtuals, so retail's
    // MoggClip introduces NO new virtual at all.  Controls: name coverage on
    // that table is 21/21 (not a coverage artifact); the word after slot 20 is
    // 0xffffffff followed by (VA, index) pairs, i.e. a handler table, so the
    // read is not truncated; and the SynthPollable subobject table is 3/3
    // EXACT, which is where GetSoundDisplayName/SynthPoll correctly live.
    //
    // The `// Playable` block was DC3's newer-engine MI refactor
    // (PlayableSample/SynthPollable -- see the note in SampleInst.h), which
    // retail's MoggClip predates.  Two of these still carry a DC3 signature
    // ALONGSIDE retail's real one further down this header: Play(float) beside
    // the non-virtual no-arg Play() at 0x8270DE60, and SetPan(float) beside the
    // non-virtual SetPan(int, float) at 0x8270EB10.  Both retail forms are
    // mapped and matching; the DC3 forms are left declared (non-virtual) rather
    // than deleted, because "not virtual" is what the vtable proves and "does
    // not exist" is a separate claim.
    //
    // ⚠ De-virtualizing is a RENAME (U -> Q in the mangling).  Pause and
    // SetVolume are MAPPED (0x8270D690, 0x8270D748) and were scoring 100%, so
    // scripts/target_symbol_map.json is corrected in the same commit; without
    // that, both rows un-pair to 0% permanently.  The other six are unmapped.
    //
    // Nothing derives from MoggClip and neither base declares any of these, so
    // no call can reach them through a base pointer: behaviour is unchanged.
    bool IsPlaying() const { return mPlaying; }
    // SynthPollable -- these two ARE virtual (table 0 is 3/3 exact)
    virtual const char *GetSoundDisplayName();
    virtual void SynthPoll();
    void Play(float);
    void Stop();
    void Pause(bool);
    bool DonePlaying();
    void SetVolume(float);
    void SetPan(float);
    void SetSend(FxSend *);

    void SetLoop(bool, int, int);
    // Retail RB3 (and the rb3-Wii oracle) carry a 1-arg SetLoop plus trivial
    // SetLoopStart/SetLoopEnd; SyncProperty's loop / loop_start_sample /
    // loop_end_sample properties call them.  Target ?SetLoop@..@QAAX_N@Z lives
    // at 0x8270D770 (mLoop 0x44, mStream 0x4c, start 0x80, end 0x84, vtable
    // slots 0xa4 ClearJump / 0xd8 SetJumpSamples) -- splits.txt mis-attributes
    // that address to BinkClip.cpp and target_symbol_map.json labels it
    // ?SetLoop@BinkClip@@QAAX_N@Z, but the body is unambiguously MoggClip's.
    // SetLoopStart/SetLoopEnd are inlined at their call sites in the target,
    // so they stay header-inline here.
    void SetLoop(bool);
    // Retail RB3-360 has a NON-VIRTUAL, NO-ARG Play() at 0x8270DE60 (proven: the
    // address is a .pdata entry, never a vtable slot, and every caller reaches it
    // with a direct `bl` -- CrowdAudio 0x82312730, Sfx 0x8271AADC, VoiceoverPanel
    // 0x8262F568 + 0x8262F6D8).  Its body reads NO float parameter: f1 is loaded
    // from a 0.0f constant and the volume comes from mVolume + mControllerVolume.
    // This is the rb3-Wii MoggClip::Play() shape; the virtual Play(float) below is
    // DC3's newer-engine form and has no retail counterpart.
    void Play();
    void SetLoopStart(int i) { mLoopStartSample = i; }
    void SetLoopEnd(int i) { mLoopEndSample = i; }
    void EndLoop();
    // Out-of-line (matches rb3-Wii MoggClip.h:51 + retail's direct `bl` at the
    // SfxInst::UpdateVolume call site); an in-class body would inline instead.
    void SetControllerVolume(float);
    bool IsStreaming() const;
    void FadeOut(float);
    void UnloadWhenFinishedPlaying(bool);
    bool IsReadyToPlay() const;
    void SetFile(const char *);
    void SetPan(int, float);
    void SetupPanInfo(float, float, bool);
    void AddFader(Fader *);
    void RemoveFader(Fader *);
    const FilePath Path() const { return mMoggFile; }
    StandardStream *GetStream() const { return mStream; }
    bool HasStream() const { return mStream; }
    int NumChannels() const { return mNumChannels; }

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(MoggClip)
    static void Init() { REGISTER_OBJ_FACTORY(MoggClip) }

private:
    void ApplyLoop(bool, int, int);
    void KillStream();
    void UnloadData();
    bool EnsureLoaded();
    void UpdateFaders();
    void UpdatePanInfo();
    void LoadNumChannels();
    void LoadFile(BinStream *);

protected:
    MoggClip();

    /** "The mogg audio file to be played." */
    FilePath mMoggFile; // 0x34
    /** "Volume in dB (0 is full volume, -96 is silence)." */
    float mControllerVolume; // 0x40
    bool mLoop; // 0x44
    float mVolume; // 0x48
    StandardStream *mStream; // 0x4c
    float unk50; // 0x50
    void *mData; // 0x54
    int mDataSize; // 0x58
    FileLoader *mLoader; // 0x5c
    std::vector<Fader *> mFaders; // 0x60
    std::vector<PanInfo> mPanInfos; // 0x6c
    Fader *mFader; // 0x78
    bool unk7c; // 0x7c
    bool mUnloadWhenFinished; // 0x7d
    bool mPlaying; // 0x7e
    int mLoopStartSample; // 0x80
    int mLoopEndSample; // 0x84
    int mNumChannels; // 0x88
    FxSend *mFxSend; // 0x8c
};
