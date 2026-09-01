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

    FilePath mFile; // 0x34
    float mVolume; // 0x40
    bool mLoop; // 0x44
    bool mPreload; // 0x45
    int mUnk3c; // 0x48 — retail RB3-360 member absent from the rb3-Wii decomp (member-delta R1)
    StandardStream *mStream; // 0x4c
    float mPlaybackVolumeOffset; // 0x50
    void *mData; // 0x54
    int mSize; // 0x58
    FileLoader *mLoader; // 0x5c
    std::vector<Fader *> mFaders; // 0x60
    std::vector<PanInfo> mPanInfo; // 0x6c
    Fader *mFadeOutFader; // 0x78
    bool mFadingOut; // 0x7c
    bool mUnloadWhenFinishedPlaying; // 0x7d
    bool mPlaying; // 0x7e
    Loader *mStreamLoader; // 0x80
};
