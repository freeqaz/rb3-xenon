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
                for (int i = 0; i < 0x10 && info->mStackTrace[i] != 0; i++) {
                    ts << *info;
                }
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
    auto& _ref0 = mStart;
    _ref0 = start;
    mName = name;
    mNum = num;
    mIsHandleHeap = handle;
    int *i7 = (int *)(((uintptr_t)start - 4 & ~(uintptr_t)0xFU) + 0x10);
    mStrategy = strat;
    _ref0 = i7;
    mAllowTemp = allowTemp;
#ifdef HX_NATIVE
    mMinFreeBytes = -1;
#endif
    mDebugLevel = debugLevel;
    gTimeStamp++;
        int time = gTimeStamp;
    InsertFreeBlock((FreeBlock *)_ref0, mSizeWords = size - (i7 - start), nullptr, nullptr, time);
    if (1 <= mDebugLevel) {
        FreeBlock *blockStart = mFreeBlockChain;
        int *blockStartInt = (int *)blockStart;
        int *start3 = blockStartInt + 3;
        int *blockEnd = blockStartInt + blockStart->mSizeWords;
        if (start3 < blockEnd) {
            int *ptr = start3 - 1;
            for (unsigned int count = (((unsigned int)blockEnd - (unsigned int)start3) - 1) / 4 + 1; count != 0; count--) {
                ptr++;
                *ptr = 0xDEADDEAD;
            }
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

int *MemHeap::TryAlloc(int sizeWords, int align, int &allocSize) {
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
        return nullptr;
    }

    if (info.mBlock == nullptr) return nullptr;

    FreeBlock *prevBlock = info.mPrevBlock;
    int blockSize = info.mSizeWords;
    int padWords = info.mPadWords;

    if (padWords > 8) {
        FreeBlock *newBlock = (FreeBlock *)((int *)info.mBlock + padWords);
        int remaining = blockSize - padWords;
        newBlock->mSizeWords = remaining;
        newBlock->mNextBlock = info.mBlock->mNextBlock;
        newBlock->mTimeStamp = info.mBlock->mTimeStamp;
        InsertFreeBlock(info.mBlock, padWords, prevBlock, newBlock, info.mBlock->mTimeStamp);
        prevBlock = info.mBlock;
        info.mBlock = newBlock;
        blockSize = remaining;
        padWords = 0;
    }

    int totalUsed = padWords + sizeWords;
    int remainder = blockSize - totalUsed;

    if (remainder > 8) {
        InsertFreeBlock(
            (FreeBlock *)((int *)info.mBlock + totalUsed), remainder,
            prevBlock, info.mBlock->mNextBlock, info.mBlock->mTimeStamp
        );
    } else {
        if (prevBlock == nullptr) {
            mFreeBlockChain = info.mBlock->mNextBlock;
        } else {
            prevBlock->mNextBlock = info.mBlock->mNextBlock;
        }
        totalUsed = blockSize;
    }

    unsigned int *header = (unsigned int *)info.mBlock + padWords;
    *header = (totalUsed << 8) | (padWords << 4) | (*header & 0xF);

    int *ptr = (int *)info.mBlock;
    int *headerPtr = (int *)header;
    for (; ptr != headerPtr; ptr++) {
        *ptr = 0;
    }

    if (1 <= mDebugLevel) {
        unsigned int hdr = *header;
        unsigned int dataWords = (hdr >> 8) - ((hdr >> 4) & 0xF);
        int *end = (int *)header + dataWords;
        int *cur = (int *)header + 1;
        if (cur < end) {
            for (int count = ((end - cur - 1) >> 2) + 1; count != 0; count--) {
                cur++;
                *cur = 0xABCDABCD;
            }
        }
    }

    allocSize = *header >> 8;
    return (int *)(header + 1);
}

int *MemHeap::Alloc(int sizeWords, int align, int &allocSize) {
    int *result = TryAlloc(sizeWords, align, allocSize);
    if (result == nullptr) {
        int lFrags, rFrags, freeBytes, minFreeBytes, maxFreeBlock;
        FreeBlockStats(lFrags, rFrags, freeBytes, minFreeBytes, maxFreeBlock);
        bool isMain = MainThread();
        if (!isMain) {
            extern bool gInsideMemFunc;
            extern CriticalSection *gMemLock;
            gInsideMemFunc = false;
            gMemLock->Abandon();
        }
        extern MemTracker *gMemTracker;
        if (gMemTracker != nullptr && !gMemTracker->GetHeapOnly()) {
            FILE *f = fopen("alloc_fail.txt", "w");
            if (f) {
                MemTracker::SpitAllocInfo((struct _iobuf *)f);
                fclose(f);
            }
        }
        int wantBytes = sizeWords * 4;
        char buf[2048];
        const char *msg = MakeString(
            "Allocation failure, heap \"%s\", want %d bytes\n"
            "   lFrags=  %8d\n"
            "   rFrags=  %8d\n"
            "   Biggest Block=%8d\n"
            "   Free Bytes=   %8d\n",
            mName, wantBytes, lFrags, rFrags, maxFreeBlock, freeBytes
        );
        strcpy(buf, msg);
        int len = strlen(buf);
        MemPrintOverview(-3, buf + len);
        MILO_FAIL(buf);
    }
    return result;
}

bool FreeBlock::AttemptMerge(FreeBlock *next, int debugLevel) {
    int thisSize = mSizeWords;
    if ((int *)this + thisSize == (int *)next) {
        unsigned int ts = mTimeStamp;
        if (ts < next->mTimeStamp) {
            ts = next->mTimeStamp;
        }
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
        int ts = gTimeStamp;
        FreeBlock *newFree = (FreeBlock *)((int *)ptr + newSizeWords);
        gTimeStamp++;
        InsertFreeBlock(newFree, truncWords, prev, next, ts);
        if (1 <= mDebugLevel) {
            int *end = (int *)newFree + newFree->mSizeWords;
            if ((int *)newFree + 3 < end) {
                int *cur = (int *)newFree + 2;
                for (unsigned int count = (((unsigned int)end - (unsigned int)((int *)newFree + 3)) - 1) / 4 + 1; count != 0; count--) {
                    cur++;
                    *cur = 0xDEADDEAD;
                }
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

int MemHeap::Free(int *ptr) {
    if (ptr < mStart || ptr >= mStart + mSizeWords) {
        return 0;
    }

    unsigned int *headerAddr = (unsigned int *)(ptr - 1);
    unsigned int header = *headerAddr;
    int blockSizeBytes = (header >> 6) & 0x3FFFFFC;

    FreeBlock *prev = nullptr;
    FreeBlock *next;
    for (next = mFreeBlockChain; next != nullptr && (int *)next < (int *)headerAddr; next = next->mNextBlock) {
        prev = next;
    }

    unsigned int padBytes = (header >> 2) & 0x3C;
    int *blockStart = (int *)((char *)headerAddr - padBytes);

    int ts = gTimeStamp++;
    FreeBlock *newFree = (FreeBlock *)blockStart;
    InsertFreeBlock(newFree, *headerAddr >> 8, prev, next, ts);

    if (1 <= mDebugLevel) {
        int *end = (int *)newFree + newFree->mSizeWords;
        if ((int *)newFree + 3 < end) {
            int *cur = (int *)newFree + 2;
            for (unsigned int count = (((unsigned int)end - (unsigned int)((int *)newFree + 3)) - 1) / 4 + 1; count != 0; count--) {
                cur++;
                *cur = 0xDEADDEAD;
            }
        }
    }

    if (next != nullptr) {
        newFree->AttemptMerge(next, mDebugLevel);
    }
    if (prev != nullptr) {
        prev->AttemptMerge(newFree, mDebugLevel);
    }

    return blockSizeBytes;
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

// sw2 scatter-include (default/MemHeap <- utl/MakeString.cpp)
#define gRev gRev_MakeString
#define gAltRev gAltRev_MakeString
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "utl/MakeString.cpp"
#endif
#undef gRev
#undef gAltRev
