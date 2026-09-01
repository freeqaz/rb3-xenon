#pragma once
#include "obj/Object.h"
#include "synth/Faders.h"
#include "synth/Stream.h"
#include "math/Rand.h"
#include "meta/MetaMusicScene.h"
#include "utl/MemMgr.h"
#include "utl/Loader.h"

class MetaMusicLoader;
typedef void (MetaMusicLoader::*MetaMusicLoaderStateFunc)(void);

class MetaMusicLoader : public Loader {
public:
    MetaMusicLoader(File *f, int &bytes, unsigned char *buf, int size);
    virtual ~MetaMusicLoader() {}
    virtual bool IsLoaded() const { return mState == &MetaMusicLoader::DoneLoading; }
    // Retail runs exactly ONE state step per poll -- the loop belongs to the
    // caller, not here.  Pinned by retail bytes at 0x8270FED8 (vtable slot 4),
    // whose ENTIRE body is `lwz r11,0x2c(r3); mtctr r11; bctr` (12 B, an
    // unconditional indirect TAIL call).  A `while (!CheckSplit() && ...)`
    // form cannot compile to that: there is no compare, no branch, and the
    // dispatch is `bctr` rather than `bctrl`.  The looping version came from
    // the newer dc3 engine; do not restore it.
    virtual void PollLoading() { (this->*mState)(); }
    // Retail formats the loader's path; it does NOT return a bare constant.
    // Pinned by retail bytes at 0x8270FF88 (vtable slot 1 -- Loader declares
    // DebugText first): it copy-constructs a FilePath temp from `this+0xc`
    // (== Loader::mFile, NOT our shadowing `File *mFile` at 0x18) and tail-
    // calls MakeString<FilePath>("MetaMusic: %s", temp).  Same house idiom as
    // DirLoader ("DL: %s") and DirUnloader ("UnLoader: %s").
    virtual const char *DebugText() { return MakeString("MetaMusic: %s", Loader::mFile); }
    virtual const char *StateName() const { return "MetaMusicLoader"; }

    void DoneLoading();
    void LoadFile();
    void OpenFile();

    File *mFile; // 0x1c
    int &mBytesRead; // 0x20
    unsigned char *mBuf; // 0x24
    int mBufSize; // 0x28
    MetaMusicLoaderStateFunc mState; // 0x2c
};

class MetaMusic : public Hmx::Object {
public:
    MetaMusic(const char *);
    virtual ~MetaMusic();
    virtual DataNode Handle(DataArray *, bool);

    int ChooseStartMs() const;
    bool IsFading() const;
    bool IsPlaying() const;
    bool Loaded();
    void Mute();
    void UnloadStreamFx();
    void UnMute();
    void Stop();
    void Start();
    void AddFader(Fader *);
    void SetScene(MetaMusicScene *);
    void LoadStreamFx();
    void Load(const char *, float, bool, bool);
    void Poll();
    void UpdateMix();

    float SomeMinusFunc() { return 1.0f - (float)unk84 / 90.0f; }
    float SomePlusFunc() { return (float)unk84 / 90.0f; }

    Stream *mStream; // 0x28
    bool mLoop; // 0x2c
    float mFadeTime; // 0x30
    float mVolume; // 0x34
    bool mPlayFromBuffer; // 0x38
    bool mRndHeap; // 0x39
    String mFilename; // 0x3c
    MemHandle *mBufferH; // 0x48
    unsigned char *mBuf; // 0x4c
    File *mFile; // 0x50
    Symbol mExt; // 0x54
    int mBufSize; // 0x58
    int mBytesRead; // 0x5c
    Fader *mFader; // 0x60
    Fader *mFaderMute; // 0x64
    ObjPtrList<Fader> mExtraFaders; // 0x68
    MetaMusicLoader *mLoader; // 0x7c
    std::vector<ObjDirPtr<ObjectDir> > unk70; // 0x80
    bool unk78; // 0x8c
    DataArray *m_CurrentFxConfig; // 0x90
    DataArray *unk80; // 0x94
    int unk84; // 0x98
    const char *unk88; // 0x9c
    bool unk8c; // 0xa0
    std::vector<int> mStartTimes; // 0xa4 - basing this off of the ChooseStartMs function
};
