#pragma once
#include "obj/Object.h"
#include "os/User.h"
#include "utl/Cache.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include <vector>

class CacheMgr {
public:
    enum OpType {
        kOpNone = 0,
        kOpSearch = 1,
        kOpChoose = 2,
        kOpMount = 3,
        kOpUnmount = 4,
        kOpDelete = 5,
    };

    struct CacheIDStoreEntry {
        CacheIDStoreEntry(Symbol s, CacheID *cid) : mName(s), mCacheID(cid) {}
        Symbol mName; // 0x0
        CacheID *mCacheID; // 0x4
    };

    virtual ~CacheMgr();
    virtual void Poll() = 0;
    virtual bool SearchAsync(const char *, CacheID **);
    virtual bool
    ShowUserSelectUIAsync(LocalUser *, u64, const char *, const char *, CacheID **);
    virtual bool
    CreateCacheIDFromDeviceID(unsigned int, const char *, const char *, CacheID **);
    virtual bool
    CreateCacheID(const char *, const char *, const char *, const char *, const char *, int, CacheID **);
    virtual bool MountAsync(CacheID *, Cache **, Hmx::Object *) = 0;
    virtual bool UnmountAsync(Cache **, Hmx::Object *) = 0;
    virtual bool DeleteAsync(CacheID *) = 0;

    CacheID *GetCacheID(Symbol);
    void RemoveCacheID(CacheID *);
    void AddCacheID(CacheID *, Symbol);
    bool IsDone();
    CacheResult GetLastResult();

    static CacheMgr *CreateCacheMgr();

    MEM_OVERLOAD(CacheMgr, 0x24);

protected:
    CacheMgr();
    OpType GetOp();
    void SetOp(OpType);
    void SetLastResult(CacheResult);

    std::vector<CacheIDStoreEntry> mCacheIDStore; // 0x4
    OpType mOpCur; // 0x10
    CacheResult mLastResult; // 0x14
};

void CacheMgrTerminate();
void CacheMgrInit();
bool IsDeviceConnected(DWORD);

extern CacheMgr *TheCacheMgr;
