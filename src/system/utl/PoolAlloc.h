#pragma once
#include "utl/MemMgr.h"
#include "utl/TextStream.h"
#include <vector>

// forward declaration
class ChunkAllocator;

#define MAX_FIXED_ALLOCS 64

class FixedSizeAlloc {
    friend class ChunkAllocator;

public:
    FixedSizeAlloc(int allocSizeWords, int nodesPerChunk);
    virtual ~FixedSizeAlloc() {}

    void *Alloc();
    void Free(void *);

    MEM_OVERLOAD(FixedSizeAlloc, 0x1C);

protected:
    virtual int *RawAlloc(int size);

    void Refill();

    int mAllocSizeWords; // 0x4
    int mNumAllocs; // 0x8
    int mMaxAllocs; // 0xc
    int mNumChunks; // 0x10
    int *mFreeList; // 0x14
    int mNodesPerChunk; // 0x18
};

class ChunkAllocator {
public:
    ChunkAllocator();
    void *Alloc(int);
    void Free(void *, int);
    void Print(TextStream &);

    MEM_OVERLOAD(ChunkAllocator, 0x38);

private:
    FixedSizeAlloc *mAllocs[MAX_FIXED_ALLOCS]; // 0x0
};

class ReclaimableAlloc : public FixedSizeAlloc {
public:
    ReclaimableAlloc(int, const char *name);

    void *CustAlloc(int bytes);
    void CustFree(void *mem);

protected:
    virtual int *RawAlloc(int size);

    void DeallocAll();

    const char *mName; // 0x1c
    std::vector<void *> mChunks; // 0x20
};

void PoolAllocInit(class DataArray *cfg);
void *PoolAlloc(int classSize, int reqSize, const char *file, int line, const char *name);
void PoolFree(int, void *mem, const char *file, int line, const char *name);
void PoolReport(TextStream &);

#ifdef HX_NATIVE
#define POOL_OVERLOAD(class_name, line_num)                                              \
    static void *operator new(size_t s) {                                                \
        return PoolAlloc(s, s, __FILE__, line_num, #class_name);                         \
    }                                                                                    \
    static void *operator new(size_t s, void *place) { return place; }                   \
    static void operator delete(void *v) {                                               \
        PoolFree(sizeof(class_name), v, __FILE__, line_num, #class_name);                \
    }
#else
#define POOL_OVERLOAD(class_name, line_num)                                              \
    static void *operator new(unsigned int s) {                                          \
        return PoolAlloc(s, s, __FILE__, line_num, #class_name);                         \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    static void operator delete(void *v) {                                               \
        PoolFree(sizeof(class_name), v, __FILE__, line_num, #class_name);                \
    }
#endif

// rb3-Wii style aliases. dc3's engine uses POOL_OVERLOAD(class, line) but the
// rb3-Wii headers (which we port verbatim) use the older NEW_POOL_OVERLOAD/
// DELETE_POOL_OVERLOAD spelling. Provide both so beatmatch/* headers compile
// without modification.
#define NEW_POOL_OVERLOAD(obj)                                                           \
    static void *operator new(unsigned int s) {                                          \
        return PoolAlloc(s, s, __FILE__, 0, #obj);                                       \
    }                                                                                    \
    static void *operator new(unsigned int, void *place) { return place; }

#define DELETE_POOL_OVERLOAD(obj)                                                        \
    static void operator delete(void *v) { PoolFree(sizeof(obj), v, __FILE__, 0, #obj); }
