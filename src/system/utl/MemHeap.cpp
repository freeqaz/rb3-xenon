#include "utl/MemHeap.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/CritSec.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/MemTracker.h"
#include "utl/TextStream.h"
#include "utl/AllocInfo.h"
#include "utl/MemTrack.h"
#include <cstdio>

namespace {
    int gTimeStamp;

    void PrintAlloc(TextStream &ts, int *ptr, int size, int count, const AllocInfo *info) {
        if (count > 0) {
            const char *str;
            if (count == 1) {
                str = MakeString("(%p ALLOC (size %6i)", ptr, size);
            } else {
                str = MakeString("(%p ALLOC (size %6i %i)", ptr, size, count);
            }
            ts << str;
            if (info != nullptr) {
#ifdef HX_NATIVE
                for (int i = 0; i < 0x10 && info->mStackTrace[i] != 0; i++) {
                    ts << *info;
                }
#else
                // retail (TU5): one line with the alloc type (info+0x8)
                ts << MakeString(" (type \"%s\")", info->mType);
#endif
            }
            ts << MakeString(")\n");
        }
    }
}

int MemHeap::GetSizeWords(int size) {
    unsigned int words = ((size + 3) >> 2) + 1;
    if (words >= 3)
        return words;
    return 3;
}

void MemHeap::FreeBlockStats(int &lFrags, int &rFrags, int &freeBytes, int &i4, int &i5) {
    int i = 0;
    int ivar5 = 0;
    int ivar3 = 0;
    int ivar6 = -1;
    for (FreeBlock *it = mFreeBlockChain; it != nullptr; it = it->mNextBlock, i++) {
        int size = it->mSizeWords * 4;
        if (ivar5 < size) {
            ivar5 = size;
            ivar6 = i;
        }
        ivar3 += size;
    }
    freeBytes = ivar3;
    i5 = ivar5;
    lFrags = ivar6;
    rFrags = (i - ivar6) - 1;
#ifdef HX_NATIVE
    mMinFreeBytes = Min<unsigned int>(ivar3, mMinFreeBytes);
    i4 = mMinFreeBytes;
#else
    i4 = ivar3;
#endif
}

void MemHeap::Print(TextStream &ts, bool verbose) {
    ts << MakeString(";---------------------------------------\n");
    const char *heapInfo = MakeString("; HEAP: %i (%s), starts %p, %d bytes\n", mNum, mName, mStart, mSizeWords * 4);
    ts << heapInfo;
    int lFrags, freeBytes, rFrags, biggest;
    FreeBlockStats(lFrags, rFrags, freeBytes, biggest);
    ts << MakeString("\n");
    ts << MakeString(
        ";   lFrags =  %8d\n;   rFrags =  %8d\n;   Total Free Bytes=  %8d\n",
        lFrags,
        rFrags,
        freeBytes
    );
    ts << MakeString("\n");
    int *curAllocPtr = nullptr;
    int curAllocSize = 0;
    int curAllocCount = 0;
    unsigned int *endPtr = (unsigned int *)End();
    unsigned int *curPtr = (unsigned int *)mStart;
    const AllocInfo *curAllocInfo = nullptr;
    unsigned int blockSizeWords = 0;

    unsigned int *curFreeBlock = (unsigned int *)mFreeBlockChain;
    for (; curPtr < endPtr; curPtr += blockSizeWords) {
        unsigned int *savedCurPtr = curPtr;

        if (curFreeBlock == nullptr || curPtr != curFreeBlock) {
            // Alloc block
            int hdr = *(int *)curPtr;
            unsigned int *headerPtr = curPtr;
            while (hdr == 0) {
                headerPtr++;
                hdr = *(int *)headerPtr;
            }
            blockSizeWords = *headerPtr >> 8;

            if (!verbose) {
                int *newPtr = (int *)(headerPtr + 1);
                const AllocInfo *newInfo = MemTrackGetInfo(newPtr);
                int newSize = blockSizeWords << 2;
                if (newSize == curAllocSize) {
                    curAllocCount++;
                } else {
                    PrintAlloc(ts, curAllocPtr, curAllocSize, curAllocCount, curAllocInfo);
                    curAllocPtr = newPtr;
                    curAllocSize = newSize;
                    curAllocCount = 1;
                    curAllocInfo = newInfo;
                }
            }
        } else {
            // Free block
            PrintAlloc(ts, curAllocPtr, curAllocSize, curAllocCount, curAllocInfo);
            curAllocSize = 0;
            curAllocCount = 0;
            const char *freeStr = " ; **** big free block!";
            unsigned int sizeWords = *curFreeBlock;
            int blockSize = sizeWords << 2;
            if (blockSize < 100000) {
                freeStr = "";
            }
            unsigned int timeStamp = curFreeBlock[1];
            ts << MakeString(
                "(%p FREE  (size %6d) (time %5d))%s\n",
                (int *)savedCurPtr,
                blockSize,
                timeStamp,
                freeStr
            );
            curFreeBlock = (unsigned int *)curFreeBlock[2];
            blockSizeWords = sizeWords;
        }
    }

    PrintAlloc(ts, curAllocPtr, curAllocSize, curAllocCount, curAllocInfo);
    ts << MakeString("\n\n");
}

void MemHeap::InsertFreeBlock(
    FreeBlock *iBlock, int size, FreeBlock *iPrevBlock, FreeBlock *iNextBlock, int time
) {
    MILO_ASSERT((iBlock != iPrevBlock) && (iBlock != iNextBlock), 0x68);
    iBlock->mSizeWords = size;
    iBlock->mNextBlock = iNextBlock;
    iBlock->mTimeStamp = time;
    if (iPrevBlock) {
        iPrevBlock->mNextBlock = iBlock;
    } else {
        mFreeBlockChain = iBlock;
    }
}

void MemHeap::Init(
    const char *name,
    int num,
    int *start,
    int size,
    bool handle,
    Strategy strat,
    int debugLevel,
    bool allowTemp
) {
    MILO_ASSERT_FMT(start, "Could not allocate %d bytes for heap %s\n", size * 4, name);
    // Retail 0x827bbb48 writes mStart twice (raw `start`, then the aligned
    // pointer) and sizes the heap from a zero-extended copy of the raw start
    // (`clrrwi r6, r6, 0`). The opaque first store keeps both: a plain
    // `mStart = start;` is dead-store-eliminated.
    int **pStart = &mStart;
    *pStart = start;
    int *rawStart = mStart;
    mName = name;
    mNum = num;
    mIsHandleHeap = handle;
    int *alignedStart = (int *)(((uintptr_t)start - 4 & ~(uintptr_t)0xFU) + 0x10);
    mStrategy = strat;
    mStart = alignedStart;
    mAllowTemp = allowTemp;
#ifdef HX_NATIVE
    mMinFreeBytes = -1;
#endif
    mDebugLevel = debugLevel;
    mSizeWords = size - (alignedStart - rawStart);
    // Arguments evaluate right to left: gTimeStamp++ first, then mSizeWords and
    // mStart are re-read (retail reloads 0xc(r3) and 0x4(r3) after the store).
    InsertFreeBlock((FreeBlock *)mStart, mSizeWords, nullptr, nullptr, gTimeStamp++);
    if (1 <= mDebugLevel) {
        FreeBlock *blockStart = mFreeBlockChain;
        int *blockStartInt = (int *)blockStart;
        int *blockEnd = blockStartInt + blockStart->mSizeWords;
        for (int *ptr = blockStartInt + 3; ptr < blockEnd; ptr++) {
            *ptr = 0xDEADDEAD;
        }
    }
}

int MemHeap::AllocSize(int *ptr) {
    if ((ptr >= mStart) && (ptr < mStart + mSizeWords)) {
        unsigned int header = *(unsigned int *)(ptr - 1);
        unsigned int blockSizeWords = header >> 8;
        unsigned int blockSizeControl = (header >> 4) & 0xF;
        return (blockSizeWords - blockSizeControl - 1) * 4;
    }
    return 0;
}

void MemHeap::FirstFit(int size, int align, FreeBlockInfo &blockinfo) {
    FreeBlock *prev = nullptr;
    for (FreeBlock *block = mFreeBlockChain; block != nullptr; block = block->mNextBlock) {
        // Calculate the data start position (after FreeBlock header)
        intptr_t start = ((intptr_t)block >> 2) + 1;
        // Calculate padding needed to align data to (1 << align) bytes
        intptr_t pad = ((((uintptr_t)(1 << align) + start) - 1) >> align) << align;
        pad = pad - start;
        if ((int)block->mSizeWords >= pad + size) {
            blockinfo.mSizeWords = block->mSizeWords;
            blockinfo.mPadWords = pad;
            blockinfo.mBlock = block;
            blockinfo.mPrevBlock = prev;
            return;
        }
        prev = block;
    }
}

void MemHeap::LastFit(int size, int align, FreeBlockInfo &blockinfo) {
    FreeBlock *block = mFreeBlockChain;
    FreeBlock *prev = nullptr;
    if (block == nullptr) {
        return;
    }
    int alignShift = align + 2;
    do {
        intptr_t blockAddr = (intptr_t)block;
        int blockSize = block->mSizeWords;
        intptr_t allocEnd = blockAddr + (blockSize - size) * 4;
        intptr_t alignedEnd = (allocEnd >> alignShift) << alignShift;
        int pad = (int)(((alignedEnd - blockAddr) - 4) >> 2);

        if (pad >= 0) {
            blockinfo.mSizeWords = blockSize;
            blockinfo.mPadWords = pad;
            blockinfo.mBlock = block;
            blockinfo.mPrevBlock = prev;
        }
        prev = block;
        block = block->mNextBlock;
    } while (block != nullptr);
}

void MemHeap::BestFit(int size, int align, FreeBlockInfo &blockinfo) {
    FreeBlock *block = mFreeBlockChain;
    FreeBlock *prev = nullptr;
    if (block == nullptr) {
        return;
    }
    do {
        int blockSize = (int)block->mSizeWords;
        // Calculate the data start position (after FreeBlock header)
        intptr_t start = ((intptr_t)block >> 2) + 1;
        // Calculate padding needed to align data to (1 << align) bytes
        intptr_t pad = ((((uintptr_t)(1 << align) + start) - 1) >> align) << align;
        pad = pad - start;
        // Track the best fit: smallest block that satisfies size requirement
        if ((blockSize >= pad + size) && (blockSize < blockinfo.mSizeWords)) {
            blockinfo.mSizeWords = blockSize;
            blockinfo.mPadWords = pad;
            blockinfo.mBlock = block;
            blockinfo.mPrevBlock = prev;
        }
        prev = block;
        block = block->mNextBlock;
    } while (block != nullptr);
}

void MemHeap::LRUFit(int size, int align, FreeBlockInfo &blockinfo) {
    int bestTime = 0x7FFFFFFF;
    FreeBlock *prev = nullptr;
    for (FreeBlock *block = mFreeBlockChain; block != nullptr; ) {
        int ts = block->mTimeStamp;
        intptr_t start = ((intptr_t)block >> 2) + 1;
        intptr_t pad = ((((uintptr_t)(1 << align) + start) - 1) >> align) << align;
        pad = pad - start;
        if ((int)block->mSizeWords >= pad + size && ts < bestTime) {
            blockinfo.mSizeWords = block->mSizeWords;
            blockinfo.mPadWords = pad;
            blockinfo.mBlock = block;
            blockinfo.mPrevBlock = prev;
            bestTime = ts;
        }
        prev = block;
        block = block->mNextBlock;
    }
}

int MemHeap::GetAlignWords(int align) {
    if ((int)align == 0) return 1;
    int bits = 0;
    int extra = 0;
    while (align > 1) {
        if (align & 1) extra = 1;
        bits++;
        align >>= 1;
    }
    int result = bits + extra - 2;
    if (0 > result) result = 0;
    return result;
}

// Retail 0x827bca78 (called directly by MemAlloc @0x827bcf94) is one function:
// fit, split, and the allocation-failure report inline. MILO_FAIL compiles out
// of the matching build, so the failure path falls through into the split code.
int *MemHeap::Alloc(int sizeWords, int align, int &allocSize) {
    FreeBlockInfo info;
    info.mBlock = nullptr;
    info.mPrevBlock = nullptr;
    info.mSizeWords = 0x7FFFFFFF;
    info.mPadWords = 0x7FFFFFFF;

    switch (mStrategy) {
    case kFirstFit: FirstFit(sizeWords, align, info); break;
    case kBestFit:  BestFit(sizeWords, align, info); break;
    case kLRUFit:   LRUFit(sizeWords, align, info); break;
    case kLastFit:  LastFit(sizeWords, align, info); break;
    default:
        MILO_ASSERT(false, 0x151);
        break;
    }

    FreeBlock *block = info.mBlock;
    if (block == nullptr) {
        int lFrags, rFrags, freeBytes, biggest;
        FreeBlockStats(lFrags, rFrags, freeBytes, biggest);
        bool isMain = MainThread();
        if (!isMain) {
            extern bool gInsideMemFunc;
            extern CriticalSection *gMemLock;
            gInsideMemFunc = false;
            gMemLock->Abandon();
        }
#ifdef HX_NATIVE
        // DC3-era addition; retail RB3 has no alloc_fail.txt dump here.
        extern MemTracker *gMemTracker;
        if (gMemTracker != nullptr && !gMemTracker->GetHeapOnly()) {
            FILE *f = fopen("alloc_fail.txt", "w");
            if (f) {
                MemTracker::SpitAllocInfo((struct _iobuf *)f);
                fclose(f);
            }
        }
#endif
        String msg;
        msg << MakeString(
            "Allocation failure, heap \"%s\", want %d bytes\n"
            "   lFrags=  %8d\n"
            "   rFrags=  %8d\n"
            "   Biggest Block=%8d\n"
            "   Free Bytes=   %8d\n",
            mName, sizeWords * 4, lFrags, rFrags, biggest, freeBytes
        );
        MemPrintOverview(kNoHeap, msg);
        MILO_FAIL(msg.c_str());
#ifdef HX_NATIVE
        return nullptr;
#endif
    }

    if (info.mPadWords > 8) {
        FreeBlock *newBlock = (FreeBlock *)((int *)block + info.mPadWords);
        info.mBlock = newBlock;
        int newSize = info.mSizeWords - info.mPadWords;
        info.mSizeWords = newSize;
        unsigned int ts = block->mTimeStamp;
        FreeBlock *next = block->mNextBlock;
        newBlock->mSizeWords = newSize;
        newBlock->mNextBlock = next;
        newBlock->mTimeStamp = ts;
        InsertFreeBlock(block, info.mPadWords, info.mPrevBlock, newBlock, newBlock->mTimeStamp);
        info.mPadWords = 0;
        info.mPrevBlock = block;
    }

    int totalUsed = sizeWords + info.mPadWords;
    int remainder = info.mSizeWords - totalUsed;
    if (remainder > 8) {
        InsertFreeBlock(
            (FreeBlock *)((int *)info.mBlock + totalUsed), remainder, info.mPrevBlock,
            info.mBlock->mNextBlock, info.mBlock->mTimeStamp
        );
    } else {
        totalUsed = info.mSizeWords;
        if (info.mPrevBlock == nullptr) {
            mFreeBlockChain = info.mBlock->mNextBlock;
        } else {
            info.mPrevBlock->mNextBlock = info.mBlock->mNextBlock;
        }
    }

    int padWords = info.mPadWords;
    unsigned int *header = (unsigned int *)info.mBlock + padWords;
    *header = (totalUsed << 8) | ((padWords & 0xF) << 4) | (*header & 0xF);

    for (unsigned int *p = header - ((*header >> 4) & 0xF); p != header; p++) {
        *p = 0;
    }

    if (1 <= mDebugLevel) {
        unsigned int hdr = *header;
        int *end = (int *)header + ((hdr >> 8) - ((hdr >> 4) & 0xF));
        for (int *cur = (int *)header + 1; cur < end; cur++) {
            *cur = 0xABCDABCD;
        }
    }

    allocSize = *header >> 8;
    return (int *)(header + 1);
}

bool FreeBlock::AttemptMerge(FreeBlock *next, int debugLevel) {
    int thisSize = mSizeWords;
    if ((int *)this + thisSize == (int *)next) {
        unsigned int ts = Max<unsigned int>(mTimeStamp, next->mTimeStamp);
        int nextSize = next->mSizeWords;
        FreeBlock *nextNext = next->mNextBlock;
        mNextBlock = nextNext;
        mSizeWords = thisSize + nextSize;
        mTimeStamp = ts;
        if (1 <= debugLevel) {
            int *ptr = (int *)next;
            int *end = ptr + 3;
            if (ptr < end) {
                do {
                    *ptr = 0xDEADDEAD;
                    ptr++;
                } while (ptr < end);
            }
        }
        return true;
    }
    return false;
}

int *MemHeap::Truncate(int *ptr, int newSizeWords, int &allocSize) {
    if (ptr < mStart || ptr >= mStart + mSizeWords) {
        return nullptr;
    }

    unsigned int header = *(unsigned int *)(ptr - 1);
    unsigned int blockSizeWords = header >> 8;
    unsigned int padWords = (header >> 4) & 0xF;
    int truncWords = blockSizeWords - padWords - newSizeWords - 1;
    MILO_ASSERT(truncWords >= 0, 0x1A8);

    unsigned int *headerPtr = (unsigned int *)(ptr - 1);

    if (truncWords > 8) {
        FreeBlock *prev = nullptr;
        FreeBlock *next;
        for (next = mFreeBlockChain; next != nullptr && (int *)next < (int *)headerPtr; next = next->mNextBlock) {
            prev = next;
        }
        FreeBlock *newFree = (FreeBlock *)((int *)ptr + newSizeWords);
        InsertFreeBlock(newFree, truncWords, prev, next, gTimeStamp++);
        if (1 <= mDebugLevel) {
            int *end = (int *)newFree + newFree->mSizeWords;
            for (int *cur = (int *)newFree + 3; cur < end; cur++) {
                *cur = 0xDEADDEAD;
            }
        }
        if (next != nullptr) {
            newFree->AttemptMerge(next, mDebugLevel);
        }
        *headerPtr = (*headerPtr & 0xFF) | ((*headerPtr - (truncWords << 8)) & 0xFFFFFF00);
    }

    allocSize = *headerPtr >> 8;
    return ptr;
}

// Retail 0x827bbd88: returns a bool (MemFree masks the result to a byte at
// 0x827bc48c) and computes no byte count.
bool MemHeap::Free(int *ptr) {
    if (ptr < mStart || ptr >= mStart + mSizeWords) {
        return false;
    }

    unsigned int *headerAddr = (unsigned int *)(ptr - 1);
    FreeBlock *prev = nullptr;
    FreeBlock *next;
    for (next = mFreeBlockChain; next != nullptr && (int *)next < (int *)headerAddr; next = next->mNextBlock) {
        prev = next;
    }

    FreeBlock *newFree = (FreeBlock *)((char *)headerAddr - ((*headerAddr >> 2) & 0x3C));
    InsertFreeBlock(newFree, *headerAddr >> 8, prev, next, gTimeStamp++);

    if (1 <= mDebugLevel) {
        int *end = (int *)newFree + newFree->mSizeWords;
        for (int *cur = (int *)newFree + 3; cur < end; cur++) {
            *cur = 0xDEADDEAD;
        }
    }

    if (next != nullptr) {
        newFree->AttemptMerge(next, mDebugLevel);
    }
    if (prev != nullptr) {
        prev->AttemptMerge(newFree, mDebugLevel);
    }
    return true;
}

#ifndef HX_NATIVE
// --- retail TU-reunification (matching build only) ---
// In retail RB3 the free Mem* API and Str/FixedString glue below were compiled
// into the SAME translation unit as MemHeap (proven: their .text interleaves the
// MemHeap:: methods under /O1's no-cross-TU-reorder). DC3 later split them into
// MemMgr.cpp / Str.cpp / FixedString.cpp. We duplicate them here so MemHeap.obj
// emits+matches those bytes; the canonical definitions stay in their DC3-split
// files for the native (HX_NATIVE) link. No final link in the matching build, so
// the duplicate symbols never collide.
// Static (internal-linkage) so MSVC addresses them section-relative and folds
// gHeaps + gNumHeaps under one shared base, matching retail's MemFindAddrHeap
// anchor (lbl_82E06BA8: gHeaps at +0x8, gNumHeaps after the array). Reverse-decl
// layout puts gHeaps at the lower address. Canonical defs live in MemMgr.cpp;
// matching build has no final link (native excludes this reunification block).
static int gNumHeaps;
static MemHeap gHeaps[16];

int MemNumHeaps() { return gNumHeaps; }

int MemHeapSize(int heap) { return gHeaps[heap].SizeWords() * 4; }

int MemFindAddrHeap(void *addr) {
    for (int i = 0; i < gNumHeaps; i++) {
        if (addr >= gHeaps[i].Start() && addr < gHeaps[i].End()) {
            return i;
        }
    }
    return -2;
}

// The heap/temp stack push-pop quartet also lives in retail's MemHeap TU, in
// this order and contiguously: MemPushHeap 0x827BC1F8 (72 B), MemPopHeap
// 0x827BC240 (48 B), MemPushTemp 0x827BC270 (48 B), MemPopTemp 0x827BC2A0
// (48 B). Disassembled out of orig/45410914/band.exe, NONE of the four tests
// gInitted or gNumHeaps: each one is prologue, `ThreadMemStack(true)`, one
// load/add/store on mSize (0x40) or mTempRefs (0x44), epilogue. MemMgr.cpp's
// copies wrap the same bodies in a `gInitted && gNumHeaps > 0` guard, which is
// eight extra instructions (32 B) of prologue -- a real source divergence, not
// a codegen one. Those copies are kept as-is for the native (HX_NATIVE) link,
// where the guard is load-bearing before MemInit; the matching build takes
// these unguarded retail bodies.
void MemPushHeap(int iHeap) {
    MemHeapStack &s = ThreadMemStack(true);
    s.mStack[s.mSize] = iHeap;
    s.mSize++;
}

void MemPopHeap() {
    MemHeapStack &s = ThreadMemStack(true);
    s.mSize--;
}

void MemPushTemp() {
    MemHeapStack &s = ThreadMemStack(true);
    s.mTempRefs++;
}

void MemPopTemp() {
    MemHeapStack &s = ThreadMemStack(true);
    s.mTempRefs--;
}
#endif


// sw2 scatter-include (default/MemHeap <- utl/Str.cpp)
#define gRev gRev_Str
#define gAltRev gAltRev_Str
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "utl/Str.cpp"
#endif
#undef gRev
#undef gAltRev

// Retail 0x827BB718 (20 B), named in the map.  ODD BUT DELIBERATE HOME: this is
// a MemMgr class, yet splits.txt pins the address inside `default/MemHeap`
// (`.text 0x827BB718-0x827BBA20`) while its sibling Lock sits in `default/MemMgr`
// 0x20 earlier.  objdiff pairs target<->base by NAME WITHIN A UNIT, so the body
// has to be compiled into MemHeap.obj to pair at all; defining it in MemMgr.cpp
// would leave this row at 0% however correct the code is.  Whether retail really
// split the two across TUs or the pin boundary is wrong is a splits question,
// not a source one -- this follows the pins rather than re-homing them.
// DECLARED at utl/MemMgr.h:160, defined in NO translation unit before this.
// Retail body is the bare decrement: two MILO_ASSERTs
// there have pure conditions, so the
// evaluate-and-discard MILO_ASSERT leaves no trace -- which is exactly why this
// is 20 B and Lock, with its aliasing reload, is 28 B.
void MemHandle::Unlock() { --mAlloc->mLockCount; }

// sw2 scatter-include (default/MemHeap <- utl/MakeString.cpp)
#define gRev gRev_MakeString
#define gAltRev gAltRev_MakeString
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "utl/MakeString.cpp"
#endif
#undef gRev
#undef gAltRev
