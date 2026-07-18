#pragma once
#include "os/Platform.h"
#include "os/Timer.h"
#include "utl/FilePath.h"
#include "utl/MemMgr.h"
#include <list>

enum LoaderPos {
    kLoadFront = 0,
    kLoadBack = 1,
    kLoadFrontStayBack = 2,
    kLoadStayBack = 3,
};

class Loader {
public:
    class Callback {
    public:
        Callback() {}
        virtual ~Callback() {}
        virtual void FinishLoading(Loader *) {}
        virtual void FailedLoading(Loader *) {}
    };
    Loader(const FilePath &, LoaderPos);
    virtual ~Loader();
    virtual const char *DebugText() = 0;
    virtual bool IsLoaded() const = 0;
    virtual const char *StateName() const { return "Unknown"; }

    LoaderPos GetPos() const { return mPos; }
    FilePath &LoaderFile() { return mFile; }

    MEM_OVERLOAD(Loader, 0xA8);

    friend class LoadMgr;

protected:
    virtual void PollLoading() = 0;

    // TU5 re-added the DC3-era re-entrance counter that the earlier retail (TU0)
    // build had dropped. Verified from the TU5 base Loader ctor
    // (default_tu5.xex @0x827BF3D0): it writes mLoadCount=0 @0x4, mPos @0x8,
    // mFile @0xc (FilePath, 0xc bytes), mHeap @0x18. The debug load timer
    // (mLoadStartMs) is still absent in retail TU5 — the ctor writes no such
    // field — so it stays native-only. This +4 base insert cascades through
    // every Loader subclass (FileLoader/DirLoader), moving mFile 0x8->0xc, which
    // is what DirLoader::Find/FindLast and LoadMgr::GetLoader read.
    int mLoadCount; // 0x4 - snapshot of gLoadCount for re-entrance detection
    LoaderPos mPos; // 0x8
    FilePath mFile; // 0xc
#ifdef HX_NATIVE
    int mLoadStartMs; // (native-only) debug load timing: SystemMs() when tracking starts, -1 when inactive
#endif
    int mHeap; // 0x18 (native w/ mLoadStartMs present: 0x1c)
};

typedef Loader *LoaderFactoryFunc(const FilePath &, LoaderPos);

class LoadMgr {
private:
    std::list<Loader *> mLoaders; // 0x0
    // TU5 divergence (verified from AddLoader/Poll/LoadStream offset triangulation,
    // 2026-07-18): mFactories through mLoaderPos are each 8 bytes earlier than a
    // naive dc3/rb3-Wii-order layout would predict (mFactories@0x8 not 0x10,
    // mPeriod@0x10, mCurrentPeriod@0x14, mLoading@0x18, mTimer@0x20). That's
    // exactly the size of mPlatform+mEditMode+mCacheMode (4+1+1+2 pad = 8), so in
    // retail TU5 those three fields are declared AFTER mLoaderPos, not here.
    std::list<std::pair<class String, LoaderFactoryFunc *> > mFactories; // 0x8
    float mPeriod; // 0x10
    float mCurrentPeriod; // 0x14
    std::list<Loader *> mLoading; // 0x18
    Timer mTimer; // 0x20
    int mAsyncUnload; // 0x50
    LoaderPos mLoaderPos; // 0x54
    unsigned int mPlatform; // 0x58
    bool mEditMode; // 0x5C
    bool mCacheMode; // 0x5D

    static bool (*sFileOpenCallback)(const char *);
    void PollFrontLoader();

public:
    LoadMgr();
    ~LoadMgr();

    void StartAsyncUnload();
    void FinishAsyncUnload();
    bool EditMode() const { return mEditMode; }
    Platform GetPlatform() const { return (Platform)mPlatform; }
    int AsyncUnload() const;
    const std::list<Loader *> &Loaders() const { return mLoaders; }
    std::list<Loader *> &Loaders() { return mLoaders; }
    std::list<Loader *> &Loading() { return mLoading; }
    LoaderPos GetLoaderPos() const { return mLoaderPos; }
    float SetLoaderPeriod(float period) {
        float ret = mPeriod;
        mPeriod = period;
        mCurrentPeriod = period;
        return ret;
    }
    bool CheckSplit() { return mTimer.SplitMs() > mCurrentPeriod; }
    void SetCurrentPeriod(float p) { mCurrentPeriod = p; }
    Loader *GetFirstLoading() {
        if (mLoading.empty()) {
            return nullptr;
        } else {
            return mLoading.front();
        }
    }

    void SetEditMode(bool);
    void SetCacheMode(bool mode) { mCacheMode = mode; }
    void RegisterFactory(const char *, LoaderFactoryFunc *);
    void PollUntilLoaded(Loader *, Loader *);
    Loader *GetLoader(const FilePath &) const;
    Loader *ForceGetLoader(const FilePath &);
    Loader *AddLoader(const FilePath &, LoaderPos);
    void Poll();
    void Print();
    void Init();

    static const char *LoaderPosString(LoaderPos, bool);
};

extern LoadMgr TheLoadMgr;

#define LOADMGR_EDITMODE TheLoadMgr.EditMode()

class FileLoader;
typedef void (FileLoader::*FileLoaderStateFunc)(void);

class FileLoader : public Loader {
public:
    FileLoader(const FilePath &, const char *, LoaderPos, int, bool, bool, BinStream *);
    virtual ~FileLoader();
    virtual const char *DebugText();
    virtual bool IsLoaded() const;
    virtual void PollLoading();

    int GetSize(); // { return mBufLen; }
    char *GetBuffer(int *);

    static void SaveData(BinStream &, void *, int);

private:
    void AllocBuffer();
    void OpenFile();
    void LoadFile();
    void DoneLoading();
    void LoadStream();

    File *mFile; // 0x18
    BinStream *mStream; // 0x1c
    const char *mBuffer; // 0x20
    int mBufLen; // 0x24
    bool mAccessed; // 0x28
    bool mTemp; // 0x29
    bool mWarn; // 0x2a
    int mFlags; // 0x2c
    String mFilename; // 0x30
    int mBytesLoaded; // 0x3c
    int mChunkSize; // 0x40
    FileLoaderStateFunc mState; // 0x44
};
