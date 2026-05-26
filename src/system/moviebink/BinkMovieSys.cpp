#include "moviebink/BinkMovieSys.h"
#include "movie/MovieSys.h"
#include "moviebink/BinkMovieImpl.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

#ifdef HX_FFMPEG
#include "platform/FFmpegMovieImpl.h"
#elif defined(__EMSCRIPTEN__)
#include "platform/WebMovieImpl.h"
#endif

BinkMovieSys gBinkMovieSys;

extern "C" void BinkSetMemory(void *(*)(int), void (*)(void *));
extern "C" int BinkStartAsyncThread(int, int);
extern "C" void *RadAlloc(int);

BinkMovieSys::BinkMovieSys()
    : MovieSys(), mCriticalSection(0),
      mBinkCore0(-1), mBinkCore1(-1), mMovieCount(0) {
    mHasAsyncThread = true;
    mNumAsyncThreads = 1;
}

BinkMovieSys::~BinkMovieSys() {
    delete mCriticalSection;
    mCriticalSection = 0;
}

DataNode BinkMovieSys::OnMovieSetTrack(DataArray *) {
    return DataNode();
}

void BinkMovieSys::Init() {
    bool wasInit = isInitalized;

    MovieSys::Init();

    MILO_ASSERT(IsInitialized(), 0x67);

    if (mCriticalSection == nullptr) {
        void *ptr = MemAlloc(sizeof(CriticalSection), __FILE__, __LINE__, "CriticalSection", 0);
                if (ptr) {
            mCriticalSection = new (ptr) CriticalSection();
        } else {
            mCriticalSection = nullptr;
        }
    }

    CriticalSection *sec = mCriticalSection;
    if (sec != nullptr) {
        sec->Enter();
    }

    DataArray *cfg = SystemConfig(Symbol("movie"));
    cfg->FindData(Symbol("bink_core0"), mBinkCore0, true);
    cfg->FindData(Symbol("bink_core1"), mBinkCore1, true);

    if (!wasInit) {
#ifndef HX_FFMPEG
        BinkSetMemory(RadAlloc, operator delete);
        BinkMovieSys::PlatformInit();

        if (mHasAsyncThread && (BinkStartAsyncThread(mBinkCore0, 0) == 0 || BinkStartAsyncThread(mBinkCore1, 0) == 0)) {
            TheDebug.Fail(FormatString("Error starting bink async thread").Str(), nullptr);
        }
#endif
    }

    DataRegisterFunc(Symbol("set_bink_track"), OnMovieSetTrack);

    if (sec != nullptr) {
        sec->Exit();
    }
}

void BinkMovieSys::Terminate() {
    CriticalSection *cs = mCriticalSection;
    if (cs) {
        cs->Enter();
    }

    while (mMovies.size() > 0) {
        mMovies.back()->Terminate();
    }

    if (cs) {
        cs->Exit();
    }

    delete mCriticalSection;
    mCriticalSection = 0;

    MovieSys::Terminate();
}

MovieImpl* BinkMovieSys::CreateMovieImpl() {
#ifdef HX_FFMPEG
    return new FFmpegMovieImpl();
#elif defined(__EMSCRIPTEN__)
    return new WebMovieImpl();
#else
    return new BinkMovieImpl();
#endif
}

// Native stub implementations (no Bink SDK on desktop native)
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)

struct BINKTRACK;

void BinkMovieSys::PlatformInit() {}

void BinkClose(BINK *) {}
void BinkCloseTrack(BINKTRACK *) {}
unsigned int BinkGetTrackData(BINKTRACK *, void *) { return 0; }
void BinkNextFrame(BINK *) {}
BINKTRACK *BinkOpenTrack(BINK *, unsigned char) { return nullptr; }

#endif // HX_NATIVE && !__EMSCRIPTEN__
