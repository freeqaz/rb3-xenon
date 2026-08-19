#pragma once
#include "obj/Object.h"
#include "os/File.h"
#include "stl/_vector.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/PoolAlloc.h"

class FileCacheHelper {
public:
    virtual ~FileCacheHelper() {}
    virtual const char *CacheFile(const char *) { return 0; }
};

class FileCacheEntry {
public:
    FileCacheEntry(FilePath const &, FilePath const &, int);
    FileCacheEntry(FilePath const &, char *, int);
    ~FileCacheEntry();

    bool ReadDone(bool);
    void StartRead(LoaderPos, bool);
    File *MakeFile();
    bool CheckSize() { return mSize > -1; }
    bool Fail() { return mSize == 0 && !mBuf; }
    void AddRef() {
        mRefCount++;
        mReads++;
    }
    void Release() { mRefCount--; }
    int Size() const { return mSize; }
    int Priority() const { return mPriority; }
    const char *Buf() const { return mBuf; }
    const FilePath &FileName() const { return mFileName; }
    FileLoader *Loader() const { return mLoader; }
    int RefCount() const { return mRefCount; }
    void SetPriority(int prio) { mPriority = prio; }
    float LastRead() const { return mLastRead; }

    POOL_OVERLOAD(FileCacheEntry, 0x5F);

    friend class FileCache;

private:
    FilePath mFileName; // 0x0
    FilePath mReadFileName; // 0xc
    const char *mBuf; // 0x18
    FileLoader *mLoader; // 0x1c
    int mSize; // 0x20
    int mRefCount; // 0x24
    int mPriority; // 0x28
    int mReads; // 0x2c
    float mLastRead; // 0x30
};

class FileCacheFile : public File {
public:
    FileCacheFile(FileCacheEntry *entry)
        : mParent(entry), mBytesRead(0), mData(0), mPos(0) {
        mParent->AddRef();
    }
    // File
    virtual ~FileCacheFile();
    virtual int Read(void *, int);
    virtual bool ReadAsync(void *, int);
    virtual int Write(const void *, int) {
        MILO_FAIL("not implemented");
        return 0;
    }
    virtual int Seek(int, int);
    virtual int Tell() { return mPos; }
    virtual void Flush() {}
    virtual bool Eof() { return mParent->Size() <= mPos; }
    virtual bool Fail() { return mParent->Fail(); }
    virtual int Size() { return mParent->Size(); }
    virtual int UncompressedSize() { return 0; }
    virtual bool ReadDone(int &);
    virtual bool GetFileHandle(void *&) { return false; }

    POOL_OVERLOAD(FileCacheFile, 0x2D);

private:
    FileCacheEntry *mParent; // 0x4
    int mBytesRead; // 0x8
    void *mData; // 0xc
    int mPos; // 0x10
};

class FileCache {
public:
    // NOTE(NCCC-0731-ab7e/f268/opus): THREE params, not four. DC3 (newer) added
    // a trailing `bool` that gates a "Forced to dump entry with size" debug
    // spew in DumpOverSize; RB3 has no such parameter. Proven from retail
    // codegen at PreloadPanel::PreloadPanel, which materializes only r4/r5/r6
    // for this call (base with the DC3 signature emitted r4..r7), and
    // corroborated by the rb3-Wii oracle, whose FileCache ctor is
    // `FileCache(int, LoaderPos, bool)` at BOTH of its call sites.
    FileCache(int, LoaderPos, bool);
    ~FileCache();

    bool DoneCaching();
    bool FileCached(char const *);
    void StartSet(int);
    void Clear();
    void PollUntilLoaded();
    void Add(FilePath const &, int, FilePath const &);
    void Add(FilePath const &, char *, int);
    void EndSet();
    void SetSize(int);

    static void Init();
    static void Terminate();
    static void PollAll();
    static File *GetFileAll(char const *);
    static void RegisterResourceCacheHelper(class FileCacheHelper *);
    static void RegisterWavCacheHelper(class FileCacheHelper *);

    MEM_OVERLOAD(FileCache, 0x21);

protected:
    int mMaxSize; // 0x0
    bool mTryClear; // 0x4
    std::vector<FileCacheEntry *> mEntries; // 0x8
    LoaderPos mLoaderPos;
    bool unk18;
    bool unk19;

    static FileCacheHelper *sResourceCacheHelper;
    static FileCacheHelper *sWavCacheHelper;

    File *GetFile(char const *);
    int CurSize() const;
    void DumpOverSize(int);
    void Poll();
};
