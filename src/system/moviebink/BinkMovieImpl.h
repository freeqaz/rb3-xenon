#pragma once

#include "movie/MovieImpl.h"
#include "os/Timer.h"
#include "utl/Str.h"
#include <vector>

struct BINK {
    virtual ~BINK();
};

class MovieInternalBuffers {
public:
    MovieInternalBuffers();
    ~MovieInternalBuffers();

private:
    // Pointers to BINK structures (offsets 0x0-0x40)
    BINK* mBinks[17];

    // Padding from 0x44 to 0xBC (0x78 bytes)
    // This is memset to 0 in constructor
    char mPadding[0x78];  // 0x44

    // Additional field at 0xBC
    void* mUnknown;  // 0xBC
};

#ifndef HX_NATIVE
static_assert(sizeof(MovieInternalBuffers) == 0xC0, "MovieInternalBuffers size mismatch");
#endif

class BinkMovieImpl : public MovieImpl {
public:
    BinkMovieImpl();
    virtual ~BinkMovieImpl();

    virtual void SetWidthHeight(int, int);
    virtual bool Ready() const;
    virtual bool BeginFromFile(
        char const *, float, bool, bool, bool, bool, int, BinStream *, LoaderPos
    );
    virtual void Draw();
    virtual bool Poll();
    virtual void Save(BinStream *);
    virtual void End();
    virtual bool IsOpen() const;
    virtual bool IsLoading() const;
    virtual bool CheckOpen(bool);
    virtual bool SetPaused(bool);
    virtual bool Paused() const;
    virtual void UnlockThread();
    virtual void LockThread();
    virtual int GetFrame() const;
    virtual float MsPerFrame() const;
    virtual int NumFrames() const;
    virtual void SetVolume(float);

    void Terminate();

private:
    static std::vector<BinkMovieImpl *> sActiveMovies;
    void SetRect();
    void BeginFrame();
    bool PlatformCacheFile(const char *);

    void* mLoader;        // 0x04 - async loader, checked in IsLoading/Ready
    void* mLoader2;       // 0x08 - fallback loader, checked in IsLoading/Ready
    String mFilename;     // 0x0C
    int mBink;            // 0x18 - BINK* handle, non-zero when open
    bool mLoop;           // 0x1c
    int mWidth;           // 0x20
    int mHeight;          // 0x24
    bool mReady;          // 0x28
    char mRect[0x18];     // 0x29 - uninitialized region (SetRect)
    int mFrame;           // 0x44
    int mNumFrames;       // 0x48
    int mMsPerFrame;      // 0x4c
    bool mPaused;         // 0x50
    Timer mPlayTimer;     // 0x58
    Timer mLoadTimer;     // 0x88
    int mVolume;          // 0xb8
    int mVolumeTarget;    // 0xbc
    void* mHandle;        // 0xc0
    int mTreeColor;       // 0xc4 - RB-tree _M_color
    int mTreeParent;      // 0xc8 - RB-tree _M_parent
    void* mTreeLeft;      // 0xcc - RB-tree _M_left
    void* mTreeRight;     // 0xd0 - RB-tree _M_right
    int mTreeCount;       // 0xd4 - RB-tree _M_node_count
    bool mOpen;           // 0xd8
    char _padD1[3];       // 0xd9 - align to 0xD4
    bool mEndianSwapped;  // 0xdc
    bool mHasAudio;       // 0xdd
    unsigned int mThreadId; // 0xe0
    int unkDC;            // 0xe4
    int mBinkVolume;      // 0xe8 - Bink volume level (0x8000 = max)
    int mBufferOffset;    // 0xec
};
