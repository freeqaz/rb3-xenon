// Movie: the full-screen / texture movie player.
//
// Native builds keep the MovieSys/MovieImpl front end (HX_NATIVE below).
// The Xbox build is RB3's own Bink player, Movie::Impl, defined at the end of
// this file. Its whole TU is one contiguous retail block (0x82742C08-0x82746128).
#ifdef HX_NATIVE
#include "movie/Movie.h"
#include "MovieImpl.h"
#include "MovieSys.h"
#include "macros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Timer.h"
#include "synth/Faders.h"
#include "synth/Synth.h"
#include "ui/UIListState.h"
#include "utl/BinStream.h"
#include "utl/Symbol.h"

Movie::Movie() : mImpl(nullptr) {
    mImpl = TheMovieSys.CreateMovieImpl();
    MILO_ASSERT(mImpl, 0x98);
}

Movie::~Movie() { RELEASE(mImpl); }

void Movie::Init() { TheMovieSys.Init(); }

void Movie::Terminate() { TheMovieSys.Terminate(); }

void Movie::Validate() { TheMovieSys.Validate(); }

void Movie::Save(BinStream *stream) { mImpl->Save(stream); }

void Movie::End() { mImpl->End(); }

bool Movie::IsOpen() const { return mImpl->IsOpen(); }

bool Movie::IsLoading() const { return mImpl->IsLoading(); }

bool Movie::CheckOpen(bool b) { return mImpl->CheckOpen(b); }

bool Movie::Ready() const { return mImpl->Ready(); }

void Movie::SetPaused(bool b) { mImpl->SetPaused(b); }

void Movie::UnlockThread() { mImpl->UnlockThread(); }

void Movie::LockThread() { mImpl->LockThread(); }

int Movie::GetFrame() const { return mImpl->GetFrame(); }

float Movie::MsPerFrame() const { return mImpl->MsPerFrame(); }

int Movie::NumFrames() const { return mImpl->NumFrames(); }

void Movie::SetVolume(float f) { mImpl->SetVolume(f); }

int Movie::LocalizationTrack() {
    Symbol language = HongKongExceptionMet() ? Symbol("eng") : SystemLanguage();

    DataArray *langs = SupportedLanguages(false);
    int i;
    for (i = 0; i < langs->Size(); i++) {
        if (langs->Sym(i) == language)
            break;
    }
    return (i < langs->Size() ? i : 0) + 1;
}

bool Movie::BeginFromFile(
    char const *c,
    float f,
    bool b1,
    bool b2,
    bool b3,
    bool b4,
    int i,
    BinStream *stream,
    LoaderPos lp
) {
    MILO_ASSERT(TheMovieSys.IsInitialized(), 0xdc);
    return mImpl->BeginFromFile(c, f, b1, b2, b3, b4, i, stream, lp);
}

bool Movie::BeginFromFile(
    char const *c, float f, bool b1, bool b2, bool b3, bool b4, int i, BinStream *stream
) {
    return BeginFromFile(c, f, b1, b2, b3, b4, i, stream, kLoadFront);
}

void Movie::Draw() {
    START_AUTO_TIMER("movie");
    mImpl->Draw();
}

bool Movie::Poll() {
    START_AUTO_TIMER("movie");
    return mImpl->Poll();
}

void Movie::SetWidthHeight(int a, int b) { mImpl->SetWidthHeight(a, b); }

#else // !HX_NATIVE
#include "movie/Movie.h"
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

// Bink SDK structures, at the offsets the retail player reads.
struct BINK {
    unsigned int Width; // 0x0
    unsigned int Height; // 0x4
    unsigned int Frames; // 0x8
    unsigned int FrameNum; // 0xc
    unsigned int LastFrameNum; // 0x10
    unsigned int FrameRate; // 0x14
    unsigned int FrameRateDiv; // 0x18
    unsigned int ReadError; // 0x1c
};

struct BINKSUMMARY {
    unsigned int Width; // 0x0
    unsigned int Height; // 0x4
    unsigned int rest[0x20];
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

static void *RadAlloc(unsigned int size) { return MemAlloc(size, 0x80); }
static void RadFree(void *mem) { MemFree(mem); }

// Flushes a plane texture's texels out of the CPU cache once Bink has decoded
// into it.
static void StoreTexture(RndTex *tex) {
    unsigned int size = tex->TexelsPitch() * tex->Height();
    void *bits;
    if (tex->TexelsLock(bits)) {
        BinkMovieSys::PlatformStoreCache(bits, size);
        tex->TexelsUnlock();
    }
}

float TaskMgrDeltaSeconds() { return TheTaskMgr.DeltaSeconds(); }

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

MovieInternalBuffers::MovieInternalBuffers() {
    memset(&mBuffers, 0, sizeof(mBuffers));
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                mTex[i][j][k] = nullptr;
            }
        }
    }
    mMat = nullptr;
    mRefs = 0;
    mNextFrame = 0;
}

MovieInternalBuffers::~MovieInternalBuffers() {
    RELEASE(mMat);
    for (unsigned int i = 0; i < 2; i++) {
        for (unsigned int j = 0; j < 2; j++) {
            RELEASE(mTex[0][i][j]);
            RELEASE(mTex[1][i][j]);
            RELEASE(mTex[2][i][j]);
        }
    }
}

static void EndianSwapBuffer(void *buf, int size) {
    MILO_ASSERT(size % sizeof(unsigned int) == 0, 0);
    unsigned int *p = (unsigned int *)buf;
    unsigned int *end = (unsigned int *)((char *)buf + size);
    while (p < end) {
        unsigned int w = *p;
        *p++ = (w << 24) | ((w << 8) & 0xFF0000) | ((w >> 8) & 0xFF00) | (w >> 24);
    }
}

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

std::vector<Movie::Impl *> Movie::Impl::sActiveMovies;
Movie::Impl *Movie::Impl::sAsyncMovie;
int Movie::Impl::sActivePending;
Movie::Impl *Movie::Impl::sNextMovie;

static CriticalSection gMovieCrit;
static std::list<Movie::Impl *> gOpenMovies;
static bool gInitialized;
static int gBinkCores[2] = { -1, -1 };

int gForceTrack;

bool Movie::Impl::IsOpen() const {
    MOVIE_THREAD_CHECK();
    return mBink != nullptr;
}

bool Movie::Impl::IsLoading() const {
    MOVIE_THREAD_CHECK();
    return mLoader || mLoader2;
}

bool Movie::Impl::Ready() const {
    MOVIE_THREAD_CHECK();
    if (mLoader) {
        return mLoader->IsLoaded();
    } else if (mLoader2) {
        return mLoader2->IsLoaded();
    } else {
        return true;
    }
}

void Movie::Impl::SetRect() {
    MOVIE_THREAD_CHECK();
    float w;
    float h;
    float avail;
    if (mWidth != 0) {
        w = mWidth;
        h = mHeight;
        avail = h;
    } else {
        w = TheRnd.Width();
        h = TheRnd.Height();
        avail = Min(TheRnd.YRatio() * w, h);
    }
    float movieW;
    float movieH;
    if (mFillWidth) {
        movieH = mAspect * w;
        movieW = w;
        if (movieH > avail) {
            movieH = avail;
            movieW = avail / mAspect;
        }
    } else {
        movieW = avail / mAspect;
        movieH = avail;
        if (movieW > w) {
            movieW = w;
            movieH = mAspect * w;
        }
    }
    float dx = (w - movieW) * 0.5f;
    float dy = (h - Min(movieH / avail * h, h)) * 0.5f;
    mRect[0] = dx;
    mRect[2] = w - dx * 2.0f;
    mRect[1] = dy;
    mRect[3] = h - dy * 2.0f;
}

void Movie::Impl::FinishOpen() {
    MOVIE_THREAD_CHECK();
    if (!mBink) {
        MILO_WARN("BinkOpen '%s' error: %s\n", mFilename, BinkGetError());
    } else {
        mSoundDisabled |= mBuffers->mRefs != 1;
        BinkSetSoundOnOff(mBink, !mSoundDisabled);
        BINKSUMMARY summary;
        BinkGetSummary(mBink, &summary);
        mAspect = (float)summary.Height / (float)summary.Width;
        SetRect();
        SetPaused(true);
    }
}

void Movie::Impl::NextFrame() {
    MOVIE_THREAD_CHECK();
    BinkNextFrame(mBink);
}

void Movie::Impl::BeginFrame() {
    MOVIE_THREAD_CHECK();
    sAsyncMovie = this;
    MovieInternalBuffers *bufs = mBuffers;
    mMidFrame = true;
    mCurFrame = (bufs->mBuffers.FrameNum + 1) % bufs->mBuffers.TotalFrames;
    mCurHalf = (bufs->mNextFrame + 1) % (bufs->mBuffers.TotalFrames * 2)
        >= bufs->mBuffers.TotalFrames;
    BINKFRAMEPLANESET &set = bufs->mBuffers.Frames[mCurFrame];
    set.YPlane.BufferPitch = bufs->mTex[0][mCurFrame][mCurHalf]->TexelsPitch();
    set.cRPlane.BufferPitch = mBuffers->mTex[1][mCurFrame][mCurHalf]->TexelsPitch();
    set.cBPlane.BufferPitch = mBuffers->mTex[2][mCurFrame][mCurHalf]->TexelsPitch();
    mBuffers->mTex[0][mCurFrame][mCurHalf]->TexelsLock(set.YPlane.Buffer);
    mBuffers->mTex[1][mCurFrame][mCurHalf]->TexelsLock(set.cRPlane.Buffer);
    mBuffers->mTex[2][mCurFrame][mCurHalf]->TexelsLock(set.cBPlane.Buffer);
}

void Movie::Impl::EndFrame() {
    MOVIE_THREAD_CHECK();
    if (mMidFrame) {
        mBuffers->mTex[0][mCurFrame][mCurHalf]->TexelsUnlock();
        mBuffers->mTex[1][mCurFrame][mCurHalf]->TexelsUnlock();
        mBuffers->mTex[2][mCurFrame][mCurHalf]->TexelsUnlock();
        MovieInternalBuffers *bufs = mBuffers;
        int half = bufs->mNextFrame >= bufs->mBuffers.TotalFrames;
        int frame = bufs->mBuffers.FrameNum;
        StoreTexture(bufs->mTex[0][frame][half]);
        StoreTexture(mBuffers->mTex[1][frame][half]);
        StoreTexture(mBuffers->mTex[2][frame][half]);
        sAsyncMovie = nullptr;
        mMidFrame = false;
        mBuffers->mNextFrame++;
        if (mBuffers->mNextFrame >= mBuffers->mBuffers.TotalFrames * 2) {
            mBuffers->mNextFrame = 0;
        }
    }
}

float (*Movie::Impl::SetTimeCallback(float (*cb)()))() {
    MOVIE_THREAD_CHECK();
    float (*old)() = mTimeCallback;
    mTimeCallback = cb;
    return old;
}

void Movie::Impl::Draw() {
    MOVIE_THREAD_CHECK();
    if (mBink && mBuffers) {
        MovieInternalBuffers *bufs = mBuffers;
        int half = bufs->mNextFrame >= bufs->mBuffers.TotalFrames;
        int frame = bufs->mBuffers.FrameNum;
        mBuffers->mMat->SetDiffuseTex(mBuffers->mTex[0][frame][half]);
        mBuffers->mMat->SetSpecularMap(mBuffers->mTex[1][frame][half]);
        mBuffers->mMat->SetEmissiveMap(mBuffers->mTex[2][frame][half]);
        SetRect();
        TheNgRnd.DrawRect(
            *(Hmx::Rect *)mRect, mBuffers->mMat, kMovieShader, Hmx::Color(), nullptr, nullptr
        );
        TheShaderMgr.SetPConstant((PShaderConstant)0, (RndTex *)nullptr);
        TheShaderMgr.SetPConstant((PShaderConstant)2, (RndTex *)nullptr);
        TheShaderMgr.SetPConstant((PShaderConstant)3, (RndTex *)nullptr);
    }
}

int Movie::Impl::GetFrame() const {
    MOVIE_THREAD_CHECK();
    if (mBink) {
        if (mBink->FrameNum == 1) {
            return mBink->Frames;
        } else {
            return mBink->FrameNum - 1;
        }
    } else {
        return 0;
    }
}

float Movie::Impl::MsPerFrame() const {
    MOVIE_THREAD_CHECK();
    if (mBink) {
        return (float)mBink->FrameRateDiv * 1000.0f / (float)mBink->FrameRate;
    } else {
        return 0.0f;
    }
}

int Movie::Impl::NumFrames() const {
    MOVIE_THREAD_CHECK();
    if (mBink) {
        return mBink->Frames;
    } else {
        return 0;
    }
}

bool Movie::Ready() const { return mImpl->Ready(); }
bool Movie::IsOpen() const { return mImpl->IsOpen(); }
bool Movie::IsLoading() const { return mImpl->IsLoading(); }
void Movie::UnlockThread() { mImpl->UnlockThread(); }
void Movie::LockThread() { mImpl->LockThread(); }
int Movie::GetFrame() const { return mImpl->GetFrame(); }
float Movie::MsPerFrame() const { return mImpl->MsPerFrame(); }
int Movie::NumFrames() const { return mImpl->NumFrames(); }
void Movie::Draw() { mImpl->Draw(); }
float (*Movie::SetTimeCallback(float (*cb)()))() { return mImpl->SetTimeCallback(cb); }
void Movie::SetWidthHeight(int w, int h) { mImpl->SetWidthHeight(w, h); }

static DataNode OnMovieSetTrack(DataArray *arr) {
    gForceTrack = arr->Int(1);
    return DataNode();
}

void Movie::Impl::StartFrame() {
    if (!mMidFrame && mAsync) {
        BeginFrame();
        BinkDoFrameAsync(mBink, gBinkCores[0], gBinkCores[1]);
    }
}

void Movie::Impl::MovieOpen(const char *file, unsigned int flags) {
    MOVIE_THREAD_CHECK();
    mPaused = false;
    if (gInitialized) {
        if (mTrack != 0) {
            if (gForceTrack != 0) {
                mTrack = gForceTrack;
            }
            unsigned int track = mTrack - 1;
            BinkSetSoundTrack(1, &track);
            flags |= 0x4000;
        }
        mBink = BinkOpen(file, flags);
        if (mBink) {
            gOpenMovies.push_back(this);
        }
    }
}

MovieInternalBuffers *MovieInternalBuffers::New(std::vector<BINK *> binks) {
    MovieInternalBuffers *bufs = new MovieInternalBuffers();
    for (int i = 0; i < binks.size(); i++) {
        BINK *bink = binks[i];
        if (bink) {
            BINKFRAMEBUFFERS info;
            memset(&info, 0, sizeof(info));
            BinkGetFrameBuffersInfo(bink, &info);
            BINKFRAMEBUFFERS &b = bufs->mBuffers;
            b.TotalFrames = Max(b.TotalFrames, info.TotalFrames);
            b.YABufferWidth = Max(b.YABufferWidth, info.YABufferWidth);
            b.YABufferHeight = Max(b.YABufferHeight, info.YABufferHeight);
            b.cRcBBufferWidth = Max(b.cRcBBufferWidth, info.cRcBBufferWidth);
            b.cRcBBufferHeight = Max(b.cRcBBufferHeight, info.cRcBBufferHeight);
            BinkRegisterFrameBuffers(bink, &b);
        }
    }
    if (bufs->mBuffers.TotalFrames == 0) {
        delete bufs;
        return nullptr;
    }
    for (int i = 0; i < bufs->mBuffers.TotalFrames; i++) {
        BINKFRAMEPLANESET &set = bufs->mBuffers.Frames[i];
        for (int j = 0; j < 2; j++) {
            bufs->mTex[0][i][j] = Hmx::Object::New<RndTex>();
            bufs->mTex[1][i][j] = Hmx::Object::New<RndTex>();
            bufs->mTex[2][i][j] = Hmx::Object::New<RndTex>();
            bufs->mTex[0][i][j]->SetBitmap(
                bufs->mBuffers.YABufferWidth,
                bufs->mBuffers.YABufferHeight,
                8,
                (RndTex::Type)0x24,
                false,
                nullptr
            );
            bufs->mTex[1][i][j]->SetBitmap(
                bufs->mBuffers.cRcBBufferWidth,
                bufs->mBuffers.cRcBBufferHeight,
                8,
                (RndTex::Type)0x24,
                false,
                nullptr
            );
            bufs->mTex[2][i][j]->SetBitmap(
                bufs->mBuffers.cRcBBufferWidth,
                bufs->mBuffers.cRcBBufferHeight,
                8,
                (RndTex::Type)0x24,
                false,
                nullptr
            );
            set.YPlane.BufferPitch = bufs->mTex[0][i][j]->TexelsPitch();
            set.cRPlane.BufferPitch = bufs->mTex[1][i][j]->TexelsPitch();
            set.cBPlane.BufferPitch = bufs->mTex[2][i][j]->TexelsPitch();
            if (bufs->mTex[0][i][j]->TexelsLock(set.YPlane.Buffer)) {
                bufs->mTex[0][i][j]->TexelsUnlock();
            }
            if (bufs->mTex[1][i][j]->TexelsLock(set.cRPlane.Buffer)) {
                bufs->mTex[1][i][j]->TexelsUnlock();
            }
            if (bufs->mTex[2][i][j]->TexelsLock(set.cBPlane.Buffer)) {
                bufs->mTex[2][i][j]->TexelsUnlock();
            }
        }
    }
    bufs->mMat = Hmx::Object::New<RndMat>();
    bufs->mMat->SetPreLit(true);
    bufs->mMat->SetUseEnv(false);
    bufs->mMat->SetBlend(RndMat::kBlendSrc);
    bufs->mMat->SetAlphaWrite(false);
    bufs->mMat->SetZMode(kZModeDisable);
    bufs->mMat->SetTexWrap(kTexWrapClamp);
    return bufs;
}

void Movie::Impl::SetPaused(bool paused) {
    if (mTimeCallback && mTimeCallback() == 0.0f) {
        paused = true;
    }
    if (mPaused == paused || !mBink) {
        return;
    }
    if (mBuffers->mRefs > 1 && sAsyncMovie) {
        if (!paused) {
            sNextMovie = this;
        }
        return;
    }
    if (!paused) {
        LockThread();
    } else {
        FinishFrame();
    }
    if (!paused && mPreloadBuf && mTimeCallback) {
        MovieClose();
        MovieOpen(mPreloadBuf, 0x4000400);
        BINKFRAMEBUFFERS info;
        BinkGetFrameBuffersInfo(mBink, &info);
        BinkRegisterFrameBuffers(mBink, &mBuffers->mBuffers);
        mPaused = true;
        FinishOpen();
    }
    BinkPause(mBink, paused);
    if (!paused) {
        StartFrame();
    }
    mPaused = paused;
    if (paused) {
        UnlockThread();
    }
}

void Movie::SetPaused(bool paused) { mImpl->SetPaused(paused); }

void Movie::Impl::SharedFinishOpen(bool unpause) {
    if (--sActivePending > 0) {
        return;
    }
    std::vector<Impl *> movies;
    std::vector<BINK *> binks;
    for (int i = 0; i < sActiveMovies.size(); i++) {
        Impl *cur = sActiveMovies[i];
        if (!cur->mBuffers) {
            movies.push_back(cur);
            binks.push_back(cur->mBink);
        }
    }
    unsigned int count = movies.size();
    bool shared = count > 1;
    MovieInternalBuffers *bufs = MovieInternalBuffers::New(binks);
    if (bufs) {
        bufs->mRefs = count;
        for (int i = 0; i < count; i++) {
            movies[i]->mBuffers = bufs;
            movies[i]->FinishOpen();
        }
    }
    if (unpause && !shared) {
        movies[0]->SetPaused(false);
    }
}

bool Movie::Impl::FinishFrame() {
    if (!mMidFrame) {
        return false;
    }
    if (mAsync && BinkDoFrameAsyncWait(mBink, mBuffers->mRefs == 1 ? 0 : -1)) {
        EndFrame();
    }
    if (mMidFrame) {
        return false;
    }
    if (mTimeCallback) {
        bool stopped = mTimeCallback() == 0.0f;
        if (stopped) {
            SetPaused(stopped);
            return false;
        }
    }
    if (sNextMovie) {
        SetPaused(true);
        sNextMovie->SetPaused(false);
        sNextMovie = nullptr;
        return false;
    }
    if (mBink->ReadError || (!mLoop && mBink->FrameNum == mBink->Frames)) {
        return false;
    }
    NextFrame();
    return true;
}

void Movie::Impl::DoFrame() {
    MOVIE_THREAD_CHECK();
    if (!mPreloaded) {
        TheBlockMgr.MarkDiscRead();
    }
    if (FinishFrame()) {
        StartFrame();
    }
}

void Movie::Impl::DiscContentionPublish() {
    MOVIE_THREAD_CHECK();
    bool first = true;
    int count = 0;
    String files;
    for (std::map<void *, String>::iterator it = mDiscContention.begin();
         it != mDiscContention.end();
         ++it) {
        if (!first) {
            files += ", ";
        }
        first = false;
        files += it->second;
        count++;
    }
    if (count != 0) {
        MILO_NOTIFY("Streaming Bink Thrashed with %d files: (%s)", count, files);
        mDiscContention.clear();
    }
}

void Movie::Impl::MovieClose() {
    MOVIE_THREAD_CHECK();
    FinishFrame();
    gOpenMovies.remove(this);
    BinkClose(mBink);
    mBink = nullptr;
}

void Movie::Impl::End() {
    MOVIE_THREAD_CHECK();
    if (mLoading) {
        mLoading = false;
        SharedFinishOpen(false);
    }
    for (std::vector<Impl *>::iterator it = sActiveMovies.begin();
         it != sActiveMovies.end();
         ++it) {
        if (*it == this) {
            sActiveMovies.erase(it);
            break;
        }
    }
    if (!mPreloadBuf) {
        DataArray *videos = SystemConfig()->FindArray("videos", false);
        if (videos) {
            DataArray *streamEnd = videos->FindArray("stream_end", false);
            if (streamEnd) {
                streamEnd->ExecuteScript(1, nullptr, nullptr, 1);
            }
        }
        DiscContentionPublish();
    }
    if (mBink) {
        MovieClose();
    }
    RELEASE(mLoader);
    RELEASE(mLoader2);
    if (mPreloadBuf) {
        MemFree(mPreloadBuf);
        mPreloadBuf = nullptr;
    }
    if (mBuffers) {
        mBuffers->mRefs--;
        if (mBuffers->mRefs == 0) {
            delete mBuffers;
        }
        mBuffers = nullptr;
    }
    mThreadId = gMainThreadID;
}

void Movie::Impl::Terminate() {
    MOVIE_THREAD_CHECK();
    if (mBink) {
        MovieClose();
    }
}

void Movie::End() { mImpl->End(); }

void Movie::Impl::DiscContentionCheck(Loader *except) {
    MOVIE_THREAD_CHECK();
    for (std::list<Loader *>::iterator it = TheLoadMgr.Loading().begin();
         it != TheLoadMgr.Loading().end();
         ++it) {
        Loader *cur = *it;
        if (cur != except) {
            mDiscContention[cur] = cur->LoaderFile();
        }
    }
}

void Movie::Terminate() {
    CritSecTracker tracker(&gMovieCrit);
    while (gOpenMovies.size() != 0) {
        gOpenMovies.back()->Terminate();
    }
    gInitialized = false;
}

Movie::Impl::Impl()
    : mLoader(nullptr), mLoader2(nullptr), mBink(nullptr), mPreloaded(false),
      mPreloadBuf(nullptr), mPreloadBufLen(0), mWidth(0), mHeight(0), mPaused(false),
      mTimeCallback(nullptr), mBinkHandle(kNoHandle), mLoading(false), mMidFrame(false),
      mThreadId(gMainThreadID), mBuffers(nullptr) {
    static Symbol movie("movie");
    static Symbol is_timed_movie("is_timed_movie");
    if (SystemConfig(movie, is_timed_movie)->ExecuteScript(1, nullptr, nullptr, 1)
        != DataNode(0)) {
        mTimeCallback = TaskMgrDeltaSeconds;
    }
    MOVIE_THREAD_CHECK();
}

Movie::Impl::~Impl() {
    MOVIE_THREAD_CHECK();
    End();
}

bool Movie::Impl::CheckOpen(bool unpause) {
    if (!mLoading) {
        return false;
    }
    if (mLoader) {
        if (!mLoader->IsLoaded()) {
            return true;
        }
        mLoading = false;
        mPreloadBuf = ((FileLoader *)mLoader)->GetBuffer(nullptr);
        mPreloadBufLen = ((FileLoader *)mLoader)->GetSize();
        RELEASE(mLoader);
        if (!mPreloadBuf) {
            SharedFinishOpen(unpause);
            End();
            return false;
        }
        if (strncmp(mPreloadBuf, "BIKi", 4) == 0) {
            EndianSwapBuffer(mPreloadBuf, mPreloadBufLen);
        }
        MovieOpen(mPreloadBuf, 0x4000400);
    } else {
        if (!mLoader2 || mBink) {
            return false;
        }
        if (!mLoader2->IsLoaded()) {
            return true;
        }
        mLoading = false;
        DataArray *videos = SystemConfig()->FindArray("videos", false);
        if (videos) {
            DataArray *streamBegin = videos->FindArray("stream_begin", false);
            if (streamBegin) {
                streamBegin->ExecuteScript(1, nullptr, nullptr, 1);
            }
        }
        File *file = NewFile(mFilename.c_str(), 2);
        if (file && file->GetFileHandle(mBinkHandle)) {
            MovieOpen((const char *)mBinkHandle, 0x800400);
        }
    }
    SharedFinishOpen(unpause);
    return false;
}

bool Movie::Impl::Poll() {
    MOVIE_THREAD_CHECK();
    if (sAsyncMovie && sAsyncMovie != this) {
        return sAsyncMovie->Poll();
    }
    if (CheckOpen(true)) {
        return true;
    }
    if (!mBink) {
        return false;
    }
    if (!mPreloadBuf) {
        DiscContentionCheck(nullptr);
    }
    if (mTimeCallback && !sAsyncMovie) {
        bool stopped = mTimeCallback() == 0.0f;
        if (!stopped) {
            SetPaused(stopped);
        }
    }
    mFrameTimer.Restart();
    if (BinkWait(mBink) == 0) {
        do {
            DoFrame();
        } while (BinkShouldSkip(mBink));
    }
    mFrameTimer.Stop();
    if (mBink->ReadError || (!mLoop && mBink->FrameNum == mBink->Frames)) {
        return false;
    }
    return true;
}

Movie::Movie() { mImpl = new Impl(); }

bool Movie::CheckOpen(bool unpause) { return mImpl->CheckOpen(unpause); }
bool Movie::Poll() { return mImpl->Poll(); }

void Movie::Impl::MovieLoader::LoadFile() {
    int bytes;
    if (mFile->ReadDone(bytes)) {
        if (!mFile->Fail()) {
            mImpl->DiscContentionCheck(this);
        }
        mOpenState = &MovieLoader::DoneLoading;
    }
}

void Movie::Impl::Init() {
    CritSecTracker tracker(&gMovieCrit);
    DataArray *cfg = SystemConfig("movie");
    cfg->FindData("bink_core0", gBinkCores[0], true);
    cfg->FindData("bink_core1", gBinkCores[1], true);
    if (!gInitialized) {
        REGISTER_OBJ_FACTORY(TexMovie)
        TheDebug.AddExitCallback(Movie::Terminate);
        BinkSetMemory(RadAlloc, RadFree);
        BinkMovieSys::PlatformInit();
        gInitialized = true;
        if (BinkStartAsyncThread(gBinkCores[0], nullptr)
            && gBinkCores[0] != gBinkCores[1]) {
            BinkStartAsyncThread(gBinkCores[1], nullptr);
        }
    }
    DataRegisterFunc("set_bink_track", OnMovieSetTrack);
}

void Movie::Impl::Save(BinStream *bs) {
    if (bs->Cached()) {
        while (CheckOpen(false)) {
            TheLoadMgr.Poll();
        }
        FileLoader::SaveData(*bs, mPreloadBuf, mPreloadBufLen);
    }
}

Movie::~Movie() { delete mImpl; }

void Movie::Save(BinStream *bs) { mImpl->Save(bs); }

void Movie::Impl::MovieLoader::OpenFile() {
    mFile = NewFile(Loader::mFile.c_str(), 2);
    if (mFile && !mFile->Fail()) {
        mFile->ReadAsync(mBuffer, 0x20);
        mOpenState = &MovieLoader::LoadFile;
    } else {
        MILO_WARN("Could not load: %s", FileLocalize(Loader::mFile.c_str(), nullptr));
        mOpenState = &MovieLoader::DoneLoading;
    }
}

void Movie::Init() { Impl::Init(); }

Movie::Impl::MovieLoader::MovieLoader(const FilePath &fp, LoaderPos pos, Movie::Impl *impl)
    : Loader(fp, pos), mFile(nullptr), mOpenState(&MovieLoader::OpenFile), mImpl(impl) {}

Movie::Impl::MovieLoader::~MovieLoader() { delete mFile; }

bool Movie::Impl::Begin(
    const char *file,
    float aspect,
    bool noSound,
    bool loop,
    bool preload,
    bool fillWidth,
    int track,
    BinStream *stream
) {
    MOVIE_THREAD_CHECK();
    mFilename = FileMakePath(FileRoot(), file);
    mPreloaded = preload;
    if (!PlatformCacheFile(file)) {
        return false;
    }
    mLoop = loop;
    mSoundDisabled = noSound;
    mTrack = track;
    mFillWidth = fillWidth;
    mAspect = 0.0f;
    mAsync = true;
    mBinkHandle = kNoHandle;
    mPollTimer.Reset();
    if (preload) {
        static int sPhysicalHeap = MemFindHeap("physical");
        MemHeapTracker tracker(sPhysicalHeap);
        mLoader = new FileLoader(
            FilePath(mFilename.c_str()),
            mFilename.c_str(),
            kLoadFront,
            0,
            true,
            true,
            stream && stream->Cached() ? stream : nullptr
        );
    } else {
        mLoader2 = new MovieLoader(FilePath(mFilename.c_str()), kLoadStayBack, this);
    }
    sActiveMovies.push_back(this);
    if (++sActivePending > 1 && !preload) {
        MILO_WARN("%s, multiple movies must be preloaded", mFilename);
    }
    mLoading = true;
    return true;
}

bool Movie::BeginFromFile(
    const char *file,
    float aspect,
    bool noSound,
    bool loop,
    bool preload,
    bool fillWidth,
    int track,
    BinStream *stream
) {
    return mImpl->Begin(file, aspect, noSound, loop, preload, fillWidth, track, stream);
}

void Movie::Validate() {}
#endif
