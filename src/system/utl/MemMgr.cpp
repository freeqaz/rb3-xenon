#include "utl/MemMgr.h"
#include "MemHeap.h"
#include "MemStats.h"
#include "MemTracker.h"
#include "Memory.h"
#include "obj/Data.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "utl/Option.h"
#include "utl/MakeString.h"
#include "utl/PoolAlloc.h"
#include "utl/Std.h"
#include "utl/TextStream.h"
#include "xdk/XAPILIB.h"
#include <cstdlib>
#include <cstring>

extern MemTracker *gMemTracker;
CriticalSection *gMemLock;

#define MAX_HEAPS 16
#define MAX_BUF_THREADS 32

const char *gStlAllocName = "StlAlloc";
bool gStlAllocNameLookup = false;

bool gbUseLowestMip = false;
bool gInsideMemFunc = false;
extern bool gMemoryUsageTest;
int gCheckConsistency;
int gNewOperatorAlign;
int gSingleHeap;
extern String gMemLogType;
std::vector<String> gUseLowestMipExceptions;
MemHeapStack gNullMemStack;
int gNumThreads;
int gThreadIds[MAX_BUF_THREADS];

bool gInitted;

MemHeap gHeaps[MAX_HEAPS];
int gNumHeaps;

#ifdef HX_NATIVE
// On native, do NOT override global operator new/delete.
// The CRT's default new/delete use malloc/free which is what we want.
// Overriding them here caused heap corruption during static initialization
// because some code paths still went through custom MemHeap logic.
#else
void *operator new(unsigned int size) {
    return MemAlloc(size, __FILE__, 0x5CF, "new", gNewOperatorAlign);
}

void *operator new[](unsigned int size) {
    return MemAlloc(size, __FILE__, 0x5E6, "new[]");
}

void operator delete(void *v) { MemFree(v, "unknown", 0, "unknown"); }
void operator delete[](void *v) { MemFree(v, "unknown", 0, "unknown"); }
#endif

void PhysDelta(const char *name) {
    static int gPhysicalUsage = -1;
    if (gPhysicalUsage == -1) {
        gPhysicalUsage = PhysicalUsage();
    }
    MEMORYSTATUS status;
    GlobalMemoryStatus(&status);
    TheDebug << name << " free:" << status.dwAvailPhys << " usage:" << PhysicalUsage()
             << " delta usage:" << PhysicalUsage() - gPhysicalUsage << "\n";
    gPhysicalUsage = PhysicalUsage();
}

bool MemUseLowestMip() { return gbUseLowestMip; }

bool MemUseLowestMipException(const char *exc) {
    String str(exc);
    str.ToLower();
    FOREACH (it, gUseLowestMipExceptions) {
        if (strstr(str.c_str(), it->c_str()) != nullptr) {
            return true;
        }
    }
    return false;
}

int _GetFreePhysicalMemory() {
    int low = 0;
    int high = 0x40000000;
    int mid;
    do {
        mid = (high + low) / 2;
        void *ptr = XPhysicalAlloc(mid, -1, 0, 4);
        if (ptr) {
            low = mid;
            XPhysicalFree(ptr);
        } else {
            high = mid;
        }
    } while (low + 1 < high);
    return low;
}

int _GetFreeSystemMemory() {
    int low = 0;
    int high = 0x40000000;
    int mid;
    do {
        mid = (high + low) / 2;
        void *ptr = malloc(mid);
        if (ptr) {
            low = mid;
            free(ptr);
        } else {
            high = mid;
        }
    } while (low + 1 < high);
    return low;
}

int MemNumHeaps() { return gNumHeaps; }

void MemFree(void *mem, const char *file, int line, const char *name) {
    if (mem) {
#ifdef HX_NATIVE
        free(mem);
#else
        CritSecTracker tracker(gMemLock);
        int i;
        int freed = 0;
        MemHeap *heap = gHeaps;
        for (i = 0; i < gNumHeaps; i++, heap++) {
            freed = heap->Free((int *)mem);
            if (freed)
                break;
        }
        if (i == gNumHeaps) {
            if (mem >= (void *)0xA0000000) {
                PhysicalFree(mem);
            } else {
                free(mem);
            }
        }
        if (gMemTracker) {
            MemTrackFree(mem);
#ifdef HX_NATIVE
            // On LP64, mHeapOnly is at a different offset and mHeapStats shifts
            // Skip heap stats tracking on native — not critical for functionality
#else
            if (((char *)gMemTracker)[0x18195]) {
                ((HeapStats *)((char *)gMemTracker + 0xC))[(signed char)i].Free(freed, freed);
            }
#endif
        }
#endif
    }
}

void MemForceNewOperatorAlign(int align) { gNewOperatorAlign = align; }

void *MemTruncate(void *mem, int size, const char *file, int line, const char *name) {
    CritSecTracker tracker(gMemLock);
    if (!mem)
        return nullptr;
    else if (size == 0) {
        MemFree(mem);
        return nullptr;
    } else {
        int i60;
        int allocSize = (size + 3) >> 2;
        void *truncated = nullptr;
        int i;
        for (i = 0; i < gNumHeaps; i++) {
            if (gHeaps[i].Truncate((int *)mem, allocSize, i60))
                break;
        }
        if (i == gNumHeaps) {
            truncated = realloc(mem, size);
            i60 = allocSize;
        }
        MemTrackRealloc(mem, size, i60 << 2, truncated);
        return truncated;
    }
}

const char *MemHeapName(int heap) {
    if (heap == -2)
        return "physical";
    else if (heap < 0)
        return "system";
    else
        return gHeaps[heap].Name();
}

int MemHeapSize(int heap) { return gHeaps[heap].SizeWords() * 4; }

int MemFindAddrHeap(void *addr) {
    for (int i = 0; i < gNumHeaps; i++) {
        if (addr >= gHeaps[i].Start() && addr < gHeaps[i].End()) {
            return i;
        }
    }
    return -2;
}

void MemPrint(int heapIdx, TextStream &stream, bool freeOnly) {
    CritSecTracker tracker(gMemLock);
    gHeaps[heapIdx].Print(stream, freeOnly);
}

#ifdef HX_NATIVE
void *MemOrPoolAlloc(int size, const char *file, int line, const char *name) {
    if (size == 0) {
        return nullptr;
    } else if (size > 0x80) {
        return MemAlloc(size, file, line, name);
    } else {
        return PoolAlloc(size, size, file, line, name);
    }
}

void MemOrPoolFree(int poolIdx, void *mem, const char *file, int line, const char *name) {
    if (mem) {
        if (poolIdx > 0x80) {
            MemFree(mem, file, line, name);
        } else {
            PoolFree(poolIdx, mem, file, line, name);
        }
    }
}

void MemOrPoolFreeSTL(
    int poolIdx, void *mem, const char *file, int line, const char *name
) {
    if (mem) {
        if (poolIdx > 0x80) {
            MemFree(mem, file, line, name);
        } else {
            PoolFree(poolIdx, mem, file, line, name);
        }
    }
}
#else
// Retail/match dispatchers: no __FILE__/line/name — the retail XEX's MemTrack
// instrumentation is compiled out, so the pool/heap entry points take only the
// byte size (and the heap fast path's MemAlloc takes (size, align)). Mirrors
// rb3-Wii _MemOrPoolAlloc(int)/_MemAlloc(int,int) and the 2-arg POOL_OVERLOAD.
void *MemOrPoolAlloc(int size) {
    if (size == 0) {
        return nullptr;
    } else if (size > 0x80) {
        return MemAlloc(size, 0);
    } else {
        return PoolAlloc(size, size);
    }
}

void MemOrPoolFree(int poolIdx, void *mem) {
    if (mem) {
        if (poolIdx > 0x80) {
            MemFree(mem);
        } else {
            PoolFree(poolIdx, mem);
        }
    }
}

void MemOrPoolFreeSTL(int poolIdx, void *mem) {
    if (mem) {
        if (poolIdx > 0x80) {
            MemFree(mem);
        } else {
            PoolFree(poolIdx, mem);
        }
    }
}
#endif

void AddHeap(
    int heapNum,
    int size,
    const char *c3,
    bool b4,
    int i5,
    MemHeap::Strategy strat,
    int i7,
    bool b8
) {
    void *raw_mem = malloc(size);
    if (raw_mem == 0) {
        int max = 0x40000000;
        raw_mem = malloc(max);
        MILO_ASSERT(raw_mem, 0x32C);
        if (size > max) {
            MILO_LOG(
                "not enough memory for heap \"%s\". Requested: %d. Available: %d\n",
                c3,
                size,
                max
            );
        }
        size = max;
    }
    gHeaps[heapNum].Init(c3, gNumHeaps, (int *)raw_mem, size >> 2, b4, strat, i7, b8);
}

void AddHeap(int i1, int i2, DataArray *arr) {
    Symbol handle("handle");
    Symbol region("region");
    Symbol debug("debug");
    Symbol strategy("strategy");
    Symbol allow_temp("allow_temp");
    const char *name = arr->Str(0);
    bool iHandle = false;
    arr->FindData(handle, iHandle, false);
    int iRegion = 0;
    arr->FindData(region, iRegion, false);
    bool iAllowTemp = true;
    arr->FindData(allow_temp, iAllowTemp, false);
    int iDebug = 0;
    arr->FindData(debug, iDebug, false);
    int iStrategy = 0;
    arr->FindData(strategy, iStrategy, false);
    AddHeap(
        i1, i2, name, iHandle, iRegion, (MemHeap::Strategy)iStrategy, iDebug, iAllowTemp
    );
}

// Stub: MemHeap::Alloc and ThreadMemStack are not yet decompiled, so route
// through malloc() which uses the CRT heap (NtAllocateVirtualMemory in Xenia).
__declspec(noinline) void *
MemAlloc(int size, const char *file, int line, const char *name, int align) {
    if (size <= 0)
        return nullptr;
    return malloc(size);
}

#ifndef HX_NATIVE
// Retail/match 2-arg heap allocator: MemAlloc(size, align). The retail XEX's
// heap fast path (verified in Ghidra: Function_827977D0(size, align)) takes no
// __FILE__/line/name — MemTrack is compiled out. Mirrors rb3-Wii _MemAlloc(int,
// int). Routes through the existing stub for now (MemHeap::Alloc TBD).
__declspec(noinline) void *MemAlloc(int size, int align) {
    if (size <= 0)
        return nullptr;
    return malloc(size);
}
#endif

void *_MemAllocTemp(int size, const char *file, int line, const char *name, int align) {
    MemTemp tmp;
    return MemAlloc(size, file, line, name, align);
}

void *MemOrPoolAllocSTL(int size, const char *file, int line, const char *name) {
    if (size == 0)
        return nullptr;
#ifdef HX_NATIVE
    return malloc(size);
#else
    else if (size > 0x80) {
        MemTemp tmp;
        return MemAlloc(size, file, line, name, 0);
    } else {
        return PoolAlloc(size, size, file, line, name);
    }
#endif
}

void MemInit() {
    gMemLock = new CriticalSection();
    gMemStackLock = new CriticalSection();
    CritSecTracker tracker(gMemLock);
    bool disableMgr = false;
    bool enableTracking = false;
    bool noTrackImmediate = true;
    DataArray *cfg = SystemConfig("mem");
    cfg->FindData("check_consistency", gCheckConsistency);
    cfg->FindData("enable_tracking", enableTracking);
    cfg->FindData("disable_mgr", disableMgr);
    cfg->FindData("single_heap", gSingleHeap);
    cfg->FindData("no_track_immediate", noTrackImmediate, false);
    cfg->FindData("log_type", gMemLogType, false);
    cfg->FindData("use_lowest_mip", gbUseLowestMip, false);
    if (gbUseLowestMip) {
        DataArray *mipArr = cfg->FindArray("lowest_mip_exceptions", false);
        if (mipArr) {
            for (int i = 1; i < mipArr->Size(); i++) {
                String str(mipArr->Str(i));
                str.ToLower();
                gUseLowestMipExceptions.push_back(str);
            }
        }
    }
    int trackHeap = -1;
    cfg->FindData("track_heap", trackHeap, false);
    bool enableDejaReport;
    cfg->FindData("enable_deja_report", enableDejaReport, false);
    int trackedAllocs = -1;
    cfg->FindData("tracked_allocs", trackedAllocs, false);
    bool heapOnly = false;
    cfg->FindData("heap_only", heapOnly, false);
    bool spew = false;
    cfg->FindData("spew", spew, false);
    if (enableTracking) {
        MILO_LOG("MemTrack: free Memory %d\n", _GetFreeSystemMemory());
        MILO_LOG("MemTrack: free physical Memory %d\n", _GetFreePhysicalMemory());
    }
    DataArray *poolArr = cfg->FindArray("pool");
    if (UsingCD()) {
        if (cfg->FindArray("discReleaseHeaps")) {
            poolArr = cfg->FindArray("discReleasePool");
        }
    }
    PoolAllocInit(poolArr);
    if (!disableMgr) {
        void *mem = malloc(0x10000);
        DataArray *heapArr = cfg->FindArray("heaps");
        if (UsingCD()) {
            if (cfg->FindArray("discReleaseHeaps")) {
                heapArr = cfg->FindArray("discReleaseHeaps");
            }
        }
        if (!(gSingleHeap == 0)) {
            gNumHeaps = 1;
        } else {
            gNumHeaps = heapArr->Size();
            MILO_ASSERT(gNumHeaps < MAX_HEAPS, 0x295);
        }
        Symbol size("size");
        int totalBytes = 0;
        AddHeap(heapArr->Size() - 1, 0x2500000, "tiny", false, 0, (MemHeap::Strategy)0, 0, false);
        gInitted = true;
        int i = heapArr->Size() - 1;
        if (i > 0) {
            do {
                DataArray *heap = heapArr->Array(i);
                MILO_ASSERT(heap, 0x2b2);
                int bytes = 0;
                heap->FindData(size, bytes, true);
                if (gSingleHeap != 0) {
                    totalBytes += bytes;
                    if (i != 1) {
                        continue;
                    }
                    AddHeap(0, totalBytes, heap);
                } else {
                    AddHeap(i - 1, bytes, heap);
                }
            } while (--i > 0);
        }
        free(mem);
    }
    MemHeapStack::sDefaultHeap = 0;
    if (enableTracking) {
        MemTrackInit(trackHeap, trackedAllocs, heapOnly);
        MemTrackEnable(!noTrackImmediate);
        MemTrackSpew(spew);
        cfg->FindData("track_stl", gStlAllocNameLookup);
        MemDelta("-- MemTrackInit -- ", 0);
        if (OptionBool("memory_usage_test", false)) {
            gMemoryUsageTest = true;
            MILO_LOG("--- Executing Game in Memory Usage Test Mode ---\n");
            MemTrackSetReportName(OptionStr("budget_log", "mem_usage_test_x360.0000.csv"));
        }
        if (OptionBool("memory_alloc_test", false)) {
            MILO_LOG("--- Executing Game in Memory Alloc Test Mode ---\n");
            MemTrackSetReportName(OptionStr("budget_log", "alloc_test"));
        }
    }
    gInitted = true;
}

int MemAllocSize(void *mem) {
    CritSecTracker tracker(gMemLock);
    if (!mem)
        return 0;
    else {
        for (int i = 0; i < gNumHeaps; i++) {
            int size = gHeaps[i].AllocSize((int *)mem);
            if (size != 0) {
                return size;
            }
        }
        MILO_FAIL("Can't determine size of allocation.");
        return 0;
    }
}

void *MemResizeElem(
    void *&mem,
    int &totalSize,
    void *cutPoint,
    int cutLength,
    int insertLength,
    const char *file,
    int line,
    const char *name
) {
    void *old = mem;
    int suffixSize = 0;
    ptrdiff_t prefixSize = (char *)cutPoint - (char *)mem;
    int newTotalSize = prefixSize;
    if (insertLength > -1) {
        suffixSize = (totalSize - newTotalSize) - cutLength;
        int delta = insertLength + suffixSize;
        newTotalSize = delta + prefixSize;
    }
    if (newTotalSize != totalSize) {
        mem = MemAlloc(newTotalSize, file, line, name);
        totalSize = newTotalSize;
        if (prefixSize != 0) {
            memcpy(mem, old, prefixSize);
        }
        if (suffixSize != 0) {
            memcpy(
                (char *)mem + prefixSize + insertLength,
                (char *)cutPoint + cutLength,
                suffixSize
            );
        }
        MemFree(old, file, line, name);
    }
    return (char *)mem + prefixSize;
}

void *
MemRealloc(void *mem, int size, const char *file, int line, const char *name, int align) {
    CritSecTracker tracker(gMemLock);
    if (gNumHeaps != 0) {
        int memSize = MemAllocSize(mem);
        void *dst = MemAlloc(size, file, line, name, align);
        memcpy(dst, mem, size < memSize ? size : memSize);
        MemFree(mem);
        return dst;
    } else {
        void *dst = realloc(mem, size);
        int sizeInWords = (size + 3) >> 2;
        MemTrackRealloc(mem, size, sizeInWords << 2, dst);
        return dst;
    }
}

MemHeapStack &ThreadMemStack(bool);

void MemPushHeap(int iHeap) {
    bool proceed = false;
    if (gInitted) {
        if (gNumHeaps > 0) {
            proceed = true;
        }
    }
    if (proceed) {
        MemHeapStack &s = ThreadMemStack(true);
        MILO_ASSERT_FMT(
            iHeap > kNoHeap && iHeap < gNumHeaps,
            "iHeap = %d, gNumHeaps=%d",
            iHeap,
            gNumHeaps
        );
        MILO_ASSERT(s.mSize + 1 < DIM(s.mStack), 0x1EA);
        s.mStack[s.mSize] = iHeap;
        s.mSize++;
    }
}

void MemPopHeap() {
    bool proceed = false;
    if (gInitted) {
        if (gNumHeaps > 0) {
            proceed = true;
        }
    }
    if (proceed) {
        MemHeapStack &s = ThreadMemStack(true);
        MILO_ASSERT(s.mSize > 0, 0x1f6);
        s.mSize--;
    }
}

void MemPushTemp() {
    bool proceed = gNumHeaps != 0 && gNumHeaps > 0;
    if (proceed) {
        MemHeapStack &s = ThreadMemStack(true);
        s.mTempRefs++;
    }
}

void MemPopTemp() {
    bool proceed = gNumHeaps != 0 && gNumHeaps > 0;
    if (proceed) {
        MemHeapStack &s = ThreadMemStack(true);
        MILO_ASSERT(s.mTempRefs > 0, 0x209);
        s.mTempRefs--;
    }
}

MemDoTempAllocations::MemDoTempAllocations(bool b1, bool b2) {
    mOld = gNumHeaps;
    if (b1 && gNumHeaps > 0) {
        MemPushTemp();
    }
}

MemDoTempAllocations::~MemDoTempAllocations() {
    if (gNumHeaps > mOld) {
        MemPopTemp();
    }
}

void MemFreeBlockStats(
    int heapNum, int &i2, int &i3, int &numFreeBytes, int &i5, int &biggestFreeBlock
) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x154);
    gHeaps[heapNum].FreeBlockStats(i2, i3, numFreeBytes, i5, biggestFreeBlock);
}

static MemHeapStack gThreadBuf[MAX_BUF_THREADS];
static int gThreadBufCurrentIndex;

MemHeapStack &ThreadMemStack(bool createIfMissing) {
    int idx;
    CriticalSection *lock = gMemStackLock;
    if (lock) {
        lock->Enter();
    }
    if (gNumThreads == 0) {
        gNumThreads = 1;
        gThreadIds[0] = GetCurrentThreadId();
        idx = gThreadBufCurrentIndex;
    } else {
        DWORD currentThreadId = GetCurrentThreadId();
        idx = gThreadBufCurrentIndex;
        if ((unsigned long)gThreadIds[gThreadBufCurrentIndex] != currentThreadId) {
            unsigned long *threadIdSlot = (unsigned long *)&gThreadIds[0];
            idx = 0;
            if (gNumThreads > 0) {
                unsigned long *slot = threadIdSlot;
                do {
                    unsigned long tid = GetCurrentThreadId();
                    if (*slot == tid)
                        break;
                    idx++;
                    slot++;
                } while (idx < gNumThreads);
            }
            if (!createIfMissing) {
                if (lock) {
                    lock->Exit();
                }
                return gNullMemStack;
            }
            if (idx == gNumThreads) {
                int cur = 0;
                int activeCount = gNumThreads;
                if (gNumThreads > 0) {
                    do {
                        if (!ValidateThreadId(*threadIdSlot)) {
                            MILO_ASSERT(gThreadBuf[cur].mSize == 0, 0x12e);
                            MILO_ASSERT(gThreadBuf[cur].mTempRefs == 0, 0x12f);
                            gThreadIds[cur] = GetCurrentThreadId();
                            activeCount = gNumThreads;
                            break;
                        }
                        cur++;
                        threadIdSlot++;
                        activeCount = gNumThreads;
                    } while (cur < gNumThreads);
                }
                idx = cur;
                if (cur == activeCount) {
                    MILO_ASSERT(gNumThreads < MAX_BUF_THREADS, 0x138);
                    gThreadIds[cur] = GetCurrentThreadId();
                    gNumThreads++;
                }
            }
        }
    }
    gThreadBufCurrentIndex = idx;
    MemHeapStack &result = gThreadBuf[gThreadBufCurrentIndex];
    if (lock) {
        lock->Exit();
    }
    return result;
}
int GetCurrentHeapNum() {
    MemHeapStack &stack = ThreadMemStack(false);
    if (stack.mSize != 0) {
        return stack.mStack[stack.mSize - 1];
    }
    return MemHeapStack::sDefaultHeap;
}
static int gPrevFree[MAX_HEAPS] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

void MemDelta(const char *name, int heapNum) {
    int numLargeFrags = 0;
    int numRightFrags = 0;
    int numFreeBytes = 0;
    int biggestFreeBlock = 0;
    int minFreeBytes = 0;
    MemFreeBlockStats(heapNum, numLargeFrags, numRightFrags, numFreeBytes, biggestFreeBlock, minFreeBytes);
    if (gPrevFree[heapNum] == -1) {
        gPrevFree[heapNum] = numFreeBytes;
    }
    int delta = gPrevFree[heapNum] - numFreeBytes;
    int fragmentation = numFreeBytes - minFreeBytes;
    TheDebug << name << " lfrag:" << numLargeFrags << " rfrag:" << numRightFrags
             << " largest:" << minFreeBytes << " free:" << numFreeBytes
             << " fragmentation:" << fragmentation << " delta:" << delta << "\n";
    gPrevFree[heapNum] = numFreeBytes;
}
int MemFindHeap(const char *name) {
    for (int i = 0; i < gNumHeaps; i++) {
        if (gHeaps[i].Name() && strcmp(gHeaps[i].Name(), name) == 0) {
            return i;
        }
    }
    if (strcmp(name, "char") == 0) {
        return 0;
    }
    if (strcmp(name, "physical") == 0) {
        return -2;
    }
    if (gSingleHeap) {
        return 0;
    }
    if (gInitted && gNumHeaps > 0) {
        MILO_FAIL("could not find heap \"%s\"", name);
    }
    return -1;
}
static SIZE_T sMinPhysFree = (SIZE_T)-1;

void MemPrintOverview(int heapId, char *const buf) {
    char *p = buf;
    if ((int)-2 == heapId || heapId == -3) {
        MEMORYSTATUS status;
        GlobalMemoryStatus(&status);
        if (sMinPhysFree >= status.dwAvailPhys) {
            sMinPhysFree = status.dwAvailPhys;
        }
        int usage = PhysicalUsage();
        unsigned long minFreeKB = sMinPhysFree >> 10;
        unsigned long availKB = status.dwAvailPhys >> 10;
        int usageKB = usage >> 10;
        const char *str = MakeString(
            " [%5s] KB free:%7u(%7u) usage:%5i\n",
            "physical", availKB, minFreeKB, usageKB
        );
        strcpy(p, str);
        auto _tmp0 = strlen(p);
        p += _tmp0;
    }
    for (int i = 0; i < gNumHeaps; i++) {
        if (heapId == -3 || heapId == i) {
            int leftFrag, rightFrag, numFreeBytes, biggestFree, minFreeBytes;
            MemFreeBlockStats(i, leftFrag, rightFrag, numFreeBytes, biggestFree, minFreeBytes);
            int wasteKB = (numFreeBytes - minFreeBytes) >> 10;
            int bigKB = minFreeBytes >> 10;
            int freeKB = biggestFree >> 10;
            int totalFreeKB = numFreeBytes >> 10;
            const char *name = MemHeapName(i);
            const char *str = MakeString(
                " [%5s] KB free:%7d(%7d) big:%7d lfrag:%5d rfrag:%5d waste:%5d\n",
                name, totalFreeKB, freeKB, bigKB, leftFrag, rightFrag, wasteKB
            );
            strcpy(p, str);
            auto _tmp1 = strlen(p);
            p += _tmp1;
        }
    }
}
