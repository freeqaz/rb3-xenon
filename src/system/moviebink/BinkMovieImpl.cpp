#include "BinkMovieImpl.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/ThreadCall.h"
#include "utl/MakeString.h"
#include <cstring>
#ifndef HX_NATIVE
#include <stl/_vector.h>
#include <stl/_algobase.h>
#include <map>
#endif
#include <algorithm>

std::vector<BinkMovieImpl *> BinkMovieImpl::sActiveMovies;

extern void *kNoHandle;


#ifndef HX_NATIVE
// Explicit template instantiation for vector<BINK*, StlNodeAlloc<BINK*>>
namespace stlpmtx_std {

template class vector<BINK*, StlNodeAlloc<BINK*>>;

} // namespace stlpmtx_std
#endif

MovieInternalBuffers::MovieInternalBuffers() {
    // Zero out padding region (0x44-0xBC)
    memset(reinterpret_cast<char*>(this) + 0x44, 0, 0x78);

    // Zero out all pointer fields
    for (int i = 0; i < 17; i++) {
        mBinks[i] = nullptr;
    }
    mUnknown = nullptr;
}

MovieInternalBuffers::~MovieInternalBuffers() {
    delete mBinks[16];
    mBinks[16] = nullptr;
    for (int j = 0; (unsigned int)j < 2; j++) {
        for (int i = 0; (unsigned int)i < 2; i++) {
            int base = j * 2 + i;
            delete mBinks[base];
            mBinks[base] = nullptr;
            delete mBinks[base + 4];
            mBinks[base + 4] = nullptr;
            delete mBinks[base + 8];
            mBinks[base + 8] = nullptr;
            delete mBinks[base + 12];
            mBinks[base + 12] = nullptr;
        }
    }
}

BinkMovieImpl::BinkMovieImpl()
    : mLoader(0), mLoader2(0), mFilename(), mBink(0), mLoop(false),
      mWidth(0), mHeight(0), mReady(false),
      mFrame(0), mNumFrames(0), mMsPerFrame(0), mPaused(false),
      mPlayTimer(), mLoadTimer(),
      mVolume(0), mVolumeTarget(0), mHandle(kNoHandle),
      mOpen(false)
{
    mTreeCount = 0;
    mTreeColor = 0;
    mTreeParent = 0;
    mTreeLeft = &mTreeColor;
    mTreeRight = &mTreeColor;
    mEndianSwapped = false;
    mHasAudio = false;
    mBinkVolume = 0x8000;
    mBufferOffset = 0;
    mThreadId = gMainThreadID;

    if (mThreadId != (unsigned int)GetCurrentThreadId()) {
        if (mThreadId == (unsigned int)-1 && MainThread()) {
            return;
        }
        TheDebug.Fail(
            MakeString(
                "%s called in the wrong thread (expected %d, cur thread is %d)",
                "BinkMovieImpl::BinkMovieImpl",
                mThreadId,
                GetCurrentThreadId()
            ),
            0
        );
    }
}

bool BinkMovieImpl::Paused() const { return mPaused; }

BinkMovieImpl::~BinkMovieImpl() {
    if (mThreadId != (unsigned int)GetCurrentThreadId()) {
        if (mThreadId == (unsigned int)-1 && MainThread()) {
            goto done;
        }
        TheDebug.Fail(
            MakeString(
                "%s called in the wrong thread (expected %d, cur thread is %d)",
                "BinkMovieImpl::~BinkMovieImpl",
                mThreadId,
                GetCurrentThreadId()
            ),
            0
        );
    }
done:
    End();
#ifndef HX_NATIVE
    reinterpret_cast<std::map<void*, String>*>(&mTreeColor)->clear();
#endif
}

// Native stub implementations (no Bink video on desktop native)
#if defined(HX_NATIVE) && !defined(__EMSCRIPTEN__)

void BinkMovieImpl::SetWidthHeight(int, int) {}
bool BinkMovieImpl::Ready() const { return true; }
bool BinkMovieImpl::BeginFromFile(
    const char *, float, bool, bool, bool, bool, int, BinStream *, LoaderPos
) {
    return false;
}
void BinkMovieImpl::Draw() {}
bool BinkMovieImpl::Poll() { return true; }
void BinkMovieImpl::Save(BinStream *) {}
void BinkMovieImpl::End() {}
bool BinkMovieImpl::IsOpen() const { return false; }
bool BinkMovieImpl::IsLoading() const { return false; }
bool BinkMovieImpl::CheckOpen(bool) { return false; }
bool BinkMovieImpl::SetPaused(bool) { return false; }
void BinkMovieImpl::UnlockThread() {}
void BinkMovieImpl::LockThread() {}
int BinkMovieImpl::GetFrame() const { return 0; }
float BinkMovieImpl::MsPerFrame() const { return 33.33f; }
int BinkMovieImpl::NumFrames() const { return 0; }
void BinkMovieImpl::SetVolume(float) {}
void BinkMovieImpl::Terminate() {}
bool BinkMovieImpl::PlatformCacheFile(const char *) { return false; }

#endif // HX_NATIVE && !__EMSCRIPTEN__
