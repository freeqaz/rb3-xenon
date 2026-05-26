#pragma once
#include "beatmatch/HxMaster.h"
#include "meta/DataArraySongInfo.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "synth/Faders.h"
#include "synth/Stream.h"
#include "math/Rand.h"
#include "meta/MetaMusicScene.h"
#include "utl/MemMgr.h"
#include "utl/Loader.h"

class MetaMusic : public Hmx::Object {
public:
    MetaMusic(HxMaster *, const char *);
    virtual ~MetaMusic();
    virtual DataNode Handle(DataArray *, bool);

    bool IsFading() const;
    bool IsPlaying() const;
    bool Loaded();
    void Mute();
    void UnMute();
    void Stop();
    void Start();
    void AddFader(Fader *);
    void Load(float, bool, bool);
    void Poll();
#ifdef HX_NATIVE
    void Kill(); // Immediate stop — no fade, no Poll() needed
#endif
    bool IsActive() const;
    void SetQuietVolume(float);
    bool IsStarted() const;

    // float SomeMinusFunc() { return 1.0f - (float)unk84 / 90.0f; }
    // float SomePlusFunc() { return (float)unk84 / 90.0f; }
    DataArraySongInfo *SongInfo() { return mSongInfo; }

private:
    Stream *GetStream() const;
    int NumChans() const;
    int ChooseStartMs() const;
    void LoadStreamFx();
    void UnloadStreamFx();
    void UpdateMix();

    bool mPlaying; // 0x2c
    bool mLoop; // 0x2d
    float mElapsedTime; // 0x30
    float mFadeTime; // 0x34
    float mMuteFadeTime; // 0x38
    float mVolume; // 0x3c
    Symbol unk40; // 0x40
    Fader *mFader; // 0x44
    Fader *mFaderMute; // 0x48
    ObjPtrList<Fader> mExtraFaders; // 0x4c
    FilePath mShellFxPath; // 0x60
    ObjDirPtr<ObjectDir> mShellFx; // 0x68
    std::vector<ObjectDir *> mStreamChanFx; // 0x7c
    bool mStarted; // 0x88
    DataArray *mPreMix; // 0x8c
    DataArray *mPostMix; // 0x90
    int mCrossfadeFrame; // 0x94
    bool mRestartEnabled; // 0x98
    std::vector<int> mStartTimes; // 0x9c
    bool mMuted; // 0xa8
    bool mHasStarted; // 0xa9
    DataArraySongInfo *mSongInfo; // 0xac
    HxMaster *mMaster; // 0xb0
};

extern MetaMusic *TheMetaMusic;
