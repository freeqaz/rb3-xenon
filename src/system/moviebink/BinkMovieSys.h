#pragma once

#include "movie/MovieSys.h"
#include <list>

class CriticalSection;
class DataArray;
class DataNode;
class BinkMovieImpl;

class BinkMovieSys : public MovieSys {
public:
    BinkMovieSys();
    virtual ~BinkMovieSys();

    virtual void Init();
    virtual void Terminate();
    virtual MovieImpl *CreateMovieImpl();

    // Both are static in retail: PlatformStoreCache takes the buffer in r3 and
    // the size in r4, and Movie::Impl::Init calls PlatformInit with no object.
    static void PlatformInit();
    static void PlatformStoreCache(void *, unsigned int);

    static DataNode OnMovieSetTrack(DataArray *);

private:
    CriticalSection *mCriticalSection; // 0x8
    bool mHasAsyncThread; // 0xC
    int mNumAsyncThreads; // 0x10
    int mBinkCore0; // 0x14
    int mBinkCore1; // 0x18
    int mMovieCount; // 0x1C
    std::list<BinkMovieImpl*> mMovies; // 0x20
};
