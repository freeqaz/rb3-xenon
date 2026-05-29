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

// Retail/match 2-arg pool entry points. Bug doc §7: the retail RB3 XEX's
// POOL_OVERLOAD operator new/delete pass NO debug info to the pool allocator —
// the call site only sets r3/r4 (classSize/reqSize), leaving r5/r6/r7 untouched
// (verified on operator>>(BinStream&, BSPNode*&): the target omits the
// __FILE__/__LINE__/#class loads our 5-arg form emits). The retail build's
// MILO_ASSERT/MemTrack debug strings are compiled out, so the pool allocator
// ignores file/line/name and these 2-arg overloads ICF-fold onto it. (Hand-
// written debug call sites in synth360, e.g. StreamReceiver360/Voice, keep the
// 5-arg form and survive as real path strings, so we leave the 5-arg overload.)
void *PoolAlloc(int classSize, int reqSize);
void PoolFree(int idx, void *mem);

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
    static void *operator new(unsigned int s) { return PoolAlloc(s, s); }                \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    static void operator delete(void *v) { PoolFree(sizeof(class_name), v); }
#endif

// rb3-Wii style aliases. dc3's engine uses POOL_OVERLOAD(class, line) but the
// rb3-Wii headers (which we port verbatim) use the older NEW_POOL_OVERLOAD/
// DELETE_POOL_OVERLOAD spelling. Provide both so beatmatch/* headers compile
// without modification.
#define NEW_POOL_OVERLOAD(obj)                                                           \
    static void *operator new(unsigned int s) { return PoolAlloc(s, s); }                \
    static void *operator new(unsigned int, void *place) { return place; }

#define DELETE_POOL_OVERLOAD(obj)                                                        \
    static void operator delete(void *v) { PoolFree(sizeof(obj), v); }
