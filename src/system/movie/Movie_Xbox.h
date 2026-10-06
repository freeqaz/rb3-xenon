#pragma once
// The Xbox Movie::Impl (Bink player) shared by Movie.cpp and Movie_Xbox.cpp.
// Retail RB3 defines Movie::Impl::PlatformCacheFile outside Movie.cpp (Begin calls it
// out of line), so the class lives in a header both TUs can see.
#include "movie/Movie.h"
#include "movie/BinkSdk.h"
#include "movie/TexMovie.h"
#include "moviebink/BinkMovieSys.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Task.h"
#include "os/Block.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Mat.h"
#include "rndobj/Rnd_NG.h"
#include "rndobj/ShaderMgr.h"
#include "rndobj/Tex.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include <list>
#include <map>
#include <string.h>
#include <vector>

// Bink SDK structures, at the offsets the retail player reads.  BINK itself is
// in movie/BinkSdk.h, shared with every other TU that reads one.

struct BINKSUMMARY {
    unsigned int Width; // 0x0
    unsigned int Height; // 0x4
    unsigned int rest[0x1e];
};

struct BINKPLANE {
    int Allocate; // 0x0
    void *Buffer; // 0x4
    unsigned int BufferPitch; // 0x8
};

struct BINKFRAMEPLANESET {
    BINKPLANE YPlane; // 0x0
    BINKPLANE cRPlane; // 0xc
    BINKPLANE cBPlane; // 0x18
    BINKPLANE APlane; // 0x24
};

struct BINKFRAMEBUFFERS {
    int TotalFrames; // 0x0
    unsigned int YABufferWidth; // 0x4
    unsigned int YABufferHeight; // 0x8
    unsigned int cRcBBufferWidth; // 0xc
    unsigned int cRcBBufferHeight; // 0x10
    unsigned int FrameNum; // 0x14
    BINKFRAMEPLANESET Frames[2]; // 0x18
};

extern "C" {
void BinkSetMemory(void *(*)(unsigned int), void (*)(void *));
BINK *BinkOpen(const char *, unsigned int);
void BinkClose(BINK *);
int BinkWait(BINK *);
int BinkShouldSkip(BINK *);
void BinkNextFrame(BINK *);
int BinkPause(BINK *, int);
char *BinkGetError();
void BinkSetSoundOnOff(BINK *, int);
void BinkGetSummary(BINK *, BINKSUMMARY *);
void BinkSetSoundTrack(unsigned int, unsigned int *);
void BinkGetFrameBuffersInfo(BINK *, BINKFRAMEBUFFERS *);
void BinkRegisterFrameBuffers(BINK *, BINKFRAMEBUFFERS *);
int BinkStartAsyncThread(int, void *);
int BinkDoFrameAsync(BINK *, unsigned int, unsigned int);
int BinkDoFrameAsyncWait(BINK *, int);
}

extern void *kNoHandle;

// The thread a movie is bound to; -1 means "any thread that is the main one".
static const unsigned int kNoThread = (unsigned int)-1;

// Every Impl entry point checks its caller's thread. Retail keeps the
// evaluation (GetCurrentThreadId, then MainThread() when unbound) and drops the
// failure path.
#define MOVIE_THREAD_CHECK()                                                             \
    MILO_ASSERT(                                                                         \
        mThreadId == CurrentThreadId() || (mThreadId == kNoThread && MainThread()), 0     \
    )

// Decode targets shared by every movie that opened together: three planes
// (Y, cR, cB), each double-buffered per Bink frame buffer.
class MovieInternalBuffers {
public:
    MovieInternalBuffers();
    ~MovieInternalBuffers();
    static MovieInternalBuffers *New(std::vector<BINK *>);

    RndTex *mTex[3][2][2]; // 0x0
    RndMat *mMat; // 0x30
    BINKFRAMEBUFFERS mBuffers; // 0x34
    int mRefs; // 0xac
    int mNextFrame; // 0xb0
};

class Movie::Impl {
public:
    class MovieLoader : public Loader {
    public:
        MovieLoader(const FilePath &, LoaderPos, Movie::Impl *);
        virtual ~MovieLoader();
        virtual const char *DebugText() { return MakeString("ML: %s", Loader::mFile.c_str()); }
        virtual bool IsLoaded() const { return mOpenState == &MovieLoader::DoneLoading; }
        virtual const char *StateName() const { return "MovieLoader"; }
        virtual void PollLoading() { (this->*mOpenState)(); }

        void OpenFile();
        void LoadFile();
        void DoneLoading() {}

        typedef void (MovieLoader::*StateFunc)();

        File *mFile; // 0x1c
        StateFunc mOpenState; // 0x20
        char mBuffer[0x20]; // 0x24
        Movie::Impl *mImpl; // 0x44
    };

    Impl();
    ~Impl();
    static void Init();
    static void SharedFinishOpen(bool);

    bool IsOpen() const;
    bool IsLoading() const;
    bool Ready() const;
    void SetRect();
    void FinishOpen();
    void NextFrame();
    void BeginFrame();
    void EndFrame();
    float (*SetTimeCallback(float (*)()))();
    void Draw();
    int GetFrame() const;
    float MsPerFrame() const;
    int NumFrames() const;
    void StartFrame();
    void MovieOpen(const char *, unsigned int);
    void SetPaused(bool);
    bool FinishFrame();
    void DoFrame();
    void DiscContentionPublish();
    void MovieClose();
    void End();
    void Terminate();
    void DiscContentionCheck(Loader *);
    bool CheckOpen(bool);
    bool Poll();
    void Save(BinStream *);
    bool Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);
    bool PlatformCacheFile(const char *);

    void LockThread() {
        MILO_ASSERT(mThreadId == kNoThread, 0);
        mThreadId = CurrentThreadId();
    }
    void UnlockThread() {
        MILO_ASSERT(mThreadId == CurrentThreadId(), 0);
        mThreadId = kNoThread;
    }
    void SetWidthHeight(int w, int h) {
        MOVIE_THREAD_CHECK();
        mWidth = w;
        mHeight = h;
    }

    static std::vector<Impl *> sActiveMovies;
    static Impl *sAsyncMovie;
    static int sActivePending;
    static Impl *sNextMovie;

    Loader *mLoader; // 0x0
    MovieLoader *mLoader2; // 0x4
    String mFilename; // 0x8
    BINK *mBink; // 0x14
    bool mPreloaded; // 0x18
    char *mPreloadBuf; // 0x1c
    int mPreloadBufLen; // 0x20
    bool mLoop; // 0x24
    bool mSoundDisabled; // 0x25
    bool mFillWidth; // 0x26
    float mAspect; // 0x28
    float mRect[4]; // 0x2c
    int mWidth; // 0x3c
    int mHeight; // 0x40
    bool mPaused; // 0x44
    Timer mPollTimer; // 0x48
    Timer mFrameTimer; // 0x78
    float (*mTimeCallback)(); // 0xa8
    int mCurFrame; // 0xac
    int mCurHalf; // 0xb0
    void *mBinkHandle; // 0xb4
    std::map<void *, String> mDiscContention; // 0xb8
    bool mLoading; // 0xd0
    bool mMidFrame; // 0xd1
    bool mAsync; // 0xd2
    unsigned int mThreadId; // 0xd4
    int mTrack; // 0xd8
    MovieInternalBuffers *mBuffers; // 0xdc
};
