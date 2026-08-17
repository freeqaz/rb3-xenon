#include "utl/PoolAlloc.h"
#include "MemMgr.h"
#include "math/Utl.h"
#include <cstdio>
#include <cstdlib>
#include "os/CritSec.h"
#include "os/Debug.h"
#include "obj/Data.h"
#include "utl/TextStream.h"
#include "utl/Std.h"

int gBigHunk = 0xC800;
int gSmallHunk = 0xC800;
int gPoolCapacity = 0;
bool gPoolAllocInitted = 0;
ChunkAllocator *gChunkAlloc = nullptr;
static int *sPoolEnd;
static int *sPoolBuf;

void PoolAllocInit(DataArray *a) {
    a->FindData("big_hunk", gBigHunk);
    gPoolAllocInitted = true;
}

void *
PoolAlloc(int classSize, int reqSize, const char *file, int line, const char *name) {
    MILO_ASSERT_FMT(classSize >= 0, "PoolAlloc class size is < 0: %d", classSize);
#ifdef HX_NATIVE
    // On 64-bit native, the pool allocator's free list uses int-sized slots
    // which can't hold 64-bit pointers. Just use malloc instead.
    return malloc(reqSize);
#else
    // Retail/match: the 5-arg entry point ignores file/line/name, same as the
    // 2-arg POOL_OVERLOAD form below (see PoolAlloc.h §7 comment) — the retail
    // XEX strips MemTrack instrumentation from this call site entirely (no
    // MemTrackAlloc call, no debug-arg register setup), which is why the 2-arg
    // overload ICF-folds onto this body.
    (void)file;
    (void)line;
    (void)name;
    CritSecTracker tracker(gMemLock);
    if (!gChunkAlloc) {
        gChunkAlloc = new ChunkAllocator();
    }
    MILO_ASSERT(reqSize == classSize, 0x15F);
    void *alloced = gChunkAlloc->Alloc(classSize);
    return alloced;
#endif
}

void PoolFree(int idx, void *mem, const char *file, int line, const char *name) {
#ifdef HX_NATIVE
    free(mem);
#else
    CritSecTracker tracker(gMemLock);
    MemTrackFree(mem);
    MILO_ASSERT(gChunkAlloc, 0x16F);
    gChunkAlloc->Free(mem, idx);
#endif
}

// Retail/match 2-arg pool entry points (see PoolAlloc.h §7). The retail XEX's
// POOL_OVERLOAD operator new/delete call these without debug args; the debug
// instrumentation is compiled out so they are ICF-equivalent to the 5-arg form.
void *PoolAlloc(int classSize, int reqSize) {
    MILO_ASSERT_FMT(classSize >= 0, "PoolAlloc class size is < 0: %d", classSize);
#ifdef HX_NATIVE
    return malloc(reqSize);
#else
    CritSecTracker tracker(gMemLock);
    if (!gChunkAlloc) {
        gChunkAlloc = new ChunkAllocator();
    }
    MILO_ASSERT(reqSize == classSize, 0x15F);
    void *alloced = gChunkAlloc->Alloc(classSize);
    // NO MemTrackAlloc here. This overload previously carried one, which made it
    // 272 bytes against the 156-byte retail body -- and contradicted this file's
    // own header comment claiming the two PoolAlloc forms ICF-fold. The retail
    // bytes are unambiguous and asymmetric: ?PoolAlloc@@YAPAXHHPBDH0@Z (156 B)
    // matches 100% with NO `bl ?MemTrackAlloc@@...` anywhere in it, while
    // ?PoolFree@@YAXHPAX@Z (132 B) matches 100% and DOES call MemTrackFree at
    // instruction 14. Retail really does track pool frees but not pool allocs;
    // that asymmetry is what the bytes say, not an oversight in the port.
    return alloced;
#endif
}

void PoolFree(int idx, void *mem) {
#ifdef HX_NATIVE
    free(mem);
#else
    CritSecTracker tracker(gMemLock);
    MemTrackFree(mem);
    MILO_ASSERT(gChunkAlloc, 0x16F);
    gChunkAlloc->Free(mem, idx);
#endif
}

void PoolReport(TextStream &ts) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(gChunkAlloc, 0x179);
    gChunkAlloc->Print(ts);
}

#pragma region FixedSizeAlloc

FixedSizeAlloc::FixedSizeAlloc(int allocSizeWords, int nodesPerChunk)
    : mAllocSizeWords(allocSizeWords), mNumAllocs(0), mMaxAllocs(0), mNumChunks(0),
      mFreeList(nullptr), mNodesPerChunk(nodesPerChunk) {
    MILO_ASSERT(mAllocSizeWords != 0, 0x9D);
}

void *FixedSizeAlloc::Alloc() {
    if (!mFreeList) {
        Refill();
    }
    int *ret = mFreeList;
    int numAllocs = mNumAllocs + 1;
    int *next = (int *)*ret;
    mNumAllocs = numAllocs;
    mFreeList = next;
    if (numAllocs > mMaxAllocs) {
        mMaxAllocs = numAllocs;
    }
    return ret;
}

void FixedSizeAlloc::Free(void *v) {
    *(int **)v = mFreeList;
    mFreeList = (int *)v;
    MILO_ASSERT_FMT(mNumAllocs > 0, "mNumAllocs is %d", mNumAllocs);
    mNumAllocs--;
}

int *FixedSizeAlloc::RawAlloc(int size) {
    int *buf = sPoolBuf;
    // Retail/match: the pool is walked in INT UNITS, not bytes. Retail's body
    // computes `srawi r11, r4, 2` then `slwi r28, r11, 2` -- two separate
    // instructions -- and reuses r28 for both the bounds check (`add r8, r28,
    // r3`) and the bump (`add r11, r28, r3`). MSVC folds the byte-wise spelling
    // `(size >> 2) << 2` into a single `clrrwi r28, r4, 2`, so retail cannot
    // have written that. A srawi/slwi PAIR that does not fold is the signature
    // of `int *` pointer arithmetic: the `>> 2` is the source's, the `slwi 2`
    // is the compiler's sizeof(int) scaling, and those are emitted by different
    // passes so they never combine. Same pattern appears twice (here and at
    // sPoolEnd below).
    int words = size >> 2;
    gPoolCapacity += size;

    if (buf + words > sPoolEnd) {
        if (MemNumHeaps() > 0) {
            // NOTE: retail's block here is bare -- `bl MemNumHeaps ; cmpwi r3,0
            // ; ble +0xc ; li r3,0 ; bl MemPushHeap` -- with NO
            // gBigHunk == gSmallHunk test and no printf. Confirmed by a
            // string scan of band.exe with a control that fires: "POOL REPORT"
            // (0x117084) and the AllocType literals are PRESENT, while
            // "PoolAlloc warning", "allocating small pool chunk", "PoolChunk"
            // and "PoolAlloc.cpp" are ABSENT. Inherited dev-build dead code.
            MemPushHeap(0);
        }

        // Retail/match: retail is `li r4, 0; bl 0x827BCD38` =
        // MemAlloc(gBigHunk, 0) -- the PERSISTENT allocator. Decoded from
        // band.exe at 0x827BAF14: lis r11,0x82C8 ; li r4,0 ; lwz r3,-0x721C(r11)
        // (= gBigHunk) ; bl 0x827BCD38. 0x827BCFF0 (_MemAllocTemp) appears
        // nowhere in the 176-byte body.
        //
        // A pool chunk is NEVER FREED, so taking it from the temp heap
        // (MemHeap::kLastFit, top-down) rather than the default bottom-up heap
        // is a genuine behavioural bug, not naming noise.
        //
        // align is 0 here, so the plain 4-arg debug form is enough: MemMgr.h's
        // macro rewrites it to exactly (MemAlloc)((gBigHunk), 0). Under
        // HX_NATIVE the same call binds to the 5-arg debug overload, so native
        // gets the fix too and no #ifdef is needed.
        sPoolBuf = (int *)MemAlloc(gBigHunk, __FILE__, 0x71, "PoolChunk");

        if (MemNumHeaps() > 0) {
            MemPopHeap();
        }

        // gBigHunk is re-read from memory here, not cached across the calls:
        // retail loads it twice (once as the MemAlloc argument, once again
        // after MemPopHeap), which is what a plain global read either side of
        // an opaque call produces.
        buf = sPoolBuf + 0x10;
        sPoolEnd = sPoolBuf + (gBigHunk >> 2);
        gBigHunk = gSmallHunk;
    }

    sPoolBuf = buf + words;
    return buf;
}

void FixedSizeAlloc::Refill() {
    MILO_ASSERT(mFreeList == 0, 0xCA);
    int allocSize = mAllocSizeWords * mNodesPerChunk;
    mFreeList = RawAlloc(allocSize * 4);
    mNumChunks++;

    int *cur = mFreeList;
    int *end = mFreeList + (allocSize - mAllocSizeWords);
    while (cur < end) {
        int *next = cur + mAllocSizeWords;
        *cur = (int)next;
        cur = next;
    }
    *cur = 0;
}

#pragma endregion
#pragma region ChunkAllocator

ChunkAllocator::ChunkAllocator() {
    for (int i = 0; i < MAX_FIXED_ALLOCS; i++) {
        mAllocs[i] = new FixedSizeAlloc((i + 1) * 4, 20);
    }
}

void *ChunkAllocator::Alloc(int idx) {
    int fixedSizeIndex = (idx - 1) >> 4;
    MILO_ASSERT(fixedSizeIndex < MAX_FIXED_ALLOCS, 0x116);
    return mAllocs[fixedSizeIndex]->Alloc();
}

void ChunkAllocator::Free(void *v, int idx) {
    int fixedSizeIndex = (idx - 1) >> 4;
    MILO_ASSERT(fixedSizeIndex < MAX_FIXED_ALLOCS, 0x122);
    MILO_ASSERT(mAllocs[fixedSizeIndex], 0x123);
    mAllocs[fixedSizeIndex]->Free(v);
}

void ChunkAllocator::Print(TextStream &ts) {
    ts << MakeString("\n*** POOL REPORT (Total Capacity: %d)***\n", gPoolCapacity);
    ts << MakeString("   NodeSize   NumAllocs  MaxAllocs  Capacity  Wasted\n");
    int wasted = 0;
    // Was a hardcoded 64 (the DC3 value) while every other loop in this file
    // already uses MAX_FIXED_ALLOCS; retail fn_827BAFF8 emits `li r27, 0x20`.
    for (int i = 0; i < MAX_FIXED_ALLOCS; i++) {
        if (mAllocs[i]) {
            FixedSizeAlloc *cur = mAllocs[i];
            int numAllocs = cur->mNumAllocs;
            int capacity = cur->mNodesPerChunk * cur->mNumChunks;
            int maxAllocs = cur->mMaxAllocs;
            int nodeSize = cur->mAllocSizeWords * 4;
            int curWasted = (capacity - cur->mNumAllocs) * cur->mAllocSizeWords * 4;
            ts << MakeString(
                "   %8d  %8d  %8d  %8d  %8d\n",
                nodeSize,
                numAllocs,
                maxAllocs,
                capacity,
                curWasted
            );
            wasted += curWasted;
        }
    }
    ts << MakeString("                             Total Waste = %8d\n", wasted);
}

#pragma endregion
#pragma region ReclaimableAlloc

ReclaimableAlloc::ReclaimableAlloc(int x, const char *name)
    : FixedSizeAlloc(((x + 15) >> 2) & ~3, 0x2800 / x), mName(name) {}

int *ReclaimableAlloc::RawAlloc(int num) {
    void *alloced = MemAlloc(num, __FILE__, 0x196, mName);
    mChunks.push_back(alloced);
    return (int *)alloced;
}

void *ReclaimableAlloc::CustAlloc(int bytes) {
    MILO_ASSERT(bytes <= mAllocSizeWords * 4, 0x188);
    return Alloc();
}

void ReclaimableAlloc::CustFree(void *mem) {
    Free(mem);
    if (mNumAllocs == 0) {
        DeallocAll();
    }
}

void ReclaimableAlloc::DeallocAll() {
    MILO_ASSERT(mNumAllocs == 0, 0x19D);
    FOREACH (it, mChunks) {
        MemFree(*it);
    }
    mChunks.clear();
    mNumChunks = 0;
    mFreeList = nullptr;
}
