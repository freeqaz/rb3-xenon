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
    OBJ_SET_TYPE(MoggClip);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual bool IsPlaying() const { return mPlaying; }
    // SynthPollable
    virtual const char *GetSoundDisplayName();
    virtual void SynthPoll();
    // Playable
    virtual void Play(float);
    virtual void Stop(bool);
    virtual void Pause(bool);
    virtual bool DonePlaying();
    virtual void SetVolume(float);
    virtual void SetPan(float);
    virtual void SetSend(FxSend *);

    void SetLoop(bool, int, int);
    void EndLoop();
    void SetControllerVolume(float vol) {
        mControllerVolume = vol;
        if (mStream) {
            mStream->Stream::SetVolume(mControllerVolume + mVolume);
        }
    }
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
