#pragma once
#include "obj/Object.h"
#include "synth/Faders.h"
#include "synth/Pollable.h"
#include "synth/StandardStream.h"
#include "utl/FilePath.h"

class FileLoader;
class Loader;

class BinkClip : public Hmx::Object, public SynthPollable {
public:
    struct PanInfo {
        PanInfo(int, float);
        int chan;
        float pan;
    };

    BinkClip();
    virtual ~BinkClip();
    OBJ_CLASSNAME(BinkClip);
    OBJ_SET_TYPE(BinkClip);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SynthPoll();

    void KillStream();
    void UnloadData();
    void Stop();
    void LoadFile(BinStream *);
    bool EnsureLoaded();
    void Play();
    void Pause(bool);
    void SetLoop(bool);
    void UpdateVolume();
    void UpdateFaders();
    void UpdatePanInfo();
    bool IsStreaming() const;
    void SetFile(const char *);
    void SetVolume(float);
    void AddFader(Fader *);
    void RemoveFader(Fader *);
    void SetPan(int, float);
    void FadeOut(float);
    void UnloadWhenFinishedPlaying(bool);
    bool IsReadyToPlay() const;
    void SetPreLoad(bool preload) { mPreload = preload; }

    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    NEW_OBJ(BinkClip);
    static void Init() { Register(); }
    static void Register() { REGISTER_OBJ_FACTORY(BinkClip) }

    FilePath mFile; // 0x28
    float mVolume; // 0x34
    bool mLoop; // 0x38
    bool mPreload; // 0x39
    int mUnk3c; // 0x3c — retail RB3-360 member absent from the rb3-Wii decomp (member-delta R1)
    StandardStream *mStream; // 0x40
    float mPlaybackVolumeOffset; // 0x44
    void *mData; // 0x48
    int mSize; // 0x4c
    FileLoader *mLoader; // 0x50
    std::vector<Fader *> mFaders; // 0x54
    std::vector<PanInfo> mPanInfo; // 0x5c
    Fader *mFadeOutFader; // 0x64
    bool mFadingOut; // 0x68
    bool mUnloadWhenFinishedPlaying; // 0x69
    bool mPlaying; // 0x6a
    Loader *mStreamLoader; // 0x6c
};
