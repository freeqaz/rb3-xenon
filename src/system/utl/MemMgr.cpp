#include "utl/MemMgr.h"
#include "MemHeap.h"
#include "MemStats.h"
#include "MemTracker.h"
#include "../../Memory.h"
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
    // Retail/match: 2-arg (size, align) with a LITERAL 0 align.
    //
    // This used to pass `gNewOperatorAlign`, inherited from dc3-decomp — which
    // is NEWER than RB3 and knows an align global RB3 does not.  Both oracles
    // refute it: rb3-Wii's MemMgr.cpp reads `operator new(size) { return
    // _MemAlloc(size, 0); }`, and retail's own body at 0x827bd2f0 is
    // `li r4,0 ; b <alloc>` -- an immediate 0, not a load of a global.  The
    // global-load form compiled to 12 bytes (lis/lwz/b) instead of retail's 8,
    // so it could not participate in the /OPT:ICF fold that puts every
    // `MemAlloc(size, 0)` operator-new spelling at ONE address, and 510 call
    // sites across 367 functions named a callee retail does not have.
    return (MemAlloc)(size, 0);
}

void *operator new[](unsigned int size) { return (MemAlloc)(size, 0); }

void operator delete(void *v) { MemFree(v); }
void operator delete[](void *v) { MemFree(v); }
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

#ifdef HX_NATIVE
void MemFree(void *mem, const char *file, int line, const char *name) {
#else
// Retail/match: 1-arg leaf free (see MemMgr.h ABI note). Parenthesized name
// suppresses the call-site MemFree(...) arg-stripping macro at the definition.
void(MemFree)(void *mem) {
#endif
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
(MemAlloc)(int size, const char *file, int line, const char *name, int align) {
    if (size <= 0)
        return nullptr;
    return malloc(size);
}

#ifndef HX_NATIVE
// Retail/match 2-arg heap allocator: MemAlloc(size, align). The retail XEX's
// heap fast path takes no __FILE__/line/name — MemTrack is compiled out.
// Mirrors rb3-Wii _MemAlloc(int, int). STILL A STUB (MemHeap::Alloc TBD).
//
// ─── HANDOFF (lane SRCPORT-1): the retail body is fn_827BCD38, 644 B, and its
// semantics are FULLY reconstructed below from retail bytes. Everything it
// needs already exists in-tree; what is left is writing it and matching it.
//
// Layouts, all verified against the retail encoding (do not re-derive):
//   MemHeapStack  mStack[16]@0x0, mSize@0x40, mTempRefs@0x44  (0x48 stride —
//                 matches ThreadMemStack's `mulli r10,r29,0x48`)
//   MemHeap       mStrategy@0x1c, mAllowTemp@0x20, stride 0x24 (`mulli *,0x24`;
//                 DC3's extra member widens this to 0x28 and is already
//                 correctly excluded from the match build — see MemHeap.h)
//   MemHeap::Alloc(sizeWords, alignShift, int&out) is fn_827BCA78 — 4 args,
//                 r6 = &local. It is NOT a rival allocator: fan-in 1, sole
//                 caller fn_827BCD38 (lane ALLOCGATE-1).
//
// Reconstruction:
//   CritSecTracker tracker(gMemLock);            // gMemLock @ lbl_82E06E08
//   MemHeapStack &s = ThreadMemStack(false);     // ONE call; see note below
//   int heapNum = s.mSize ? s.mStack[s.mSize-1] : MemHeapStack::sDefaultHeap;
//   MemHeap *heap = heapNum > -1 ? &gHeaps[heapNum] : NULL;
//   bool temp = s.mTempRefs != 0
//            || (heap != NULL && heap->mStrategy == MemHeap::kLastFit);
//   if (temp && heap != NULL && !heap->mAllowTemp) {
//       MemHeapStack &s2 = ThreadMemStack(true);
//       int savedSize = s2.mSize;
//       while (s2.mSize > 0) {                   // pop heaps until one allows temp
//           s2.mSize--;
//           int h = s2.mSize ? s2.mStack[s2.mSize-1] : MemHeapStack::sDefaultHeap;
//           MemHeap *hp = h > -1 ? &gHeaps[h] : NULL;
//           if (hp->mAllowTemp) break;           // retail derefs without a null check
//       }
//       s2.mTempRefs++;
//       void *r = MemAlloc(size, align);         // recurses into itself
//       s2.mSize = savedSize;                    // restore INSIDE the guard
//       s2.mTempRefs--;
//       return r;                                // (crit-sec exit is common tail)
//   }
//   if (heap == NULL)
//       return heapNum == -2 ? <fn_82273420>(size) : malloc(size);
//   // GetSizeWords/GetAlignWords are INLINED by retail. They live in
//   // MemHeap.cpp so they cannot inline into this TU — write the expressions
//   // out here rather than moving them to the header (that would perturb
//   // MemHeap.cpp's own emitted copies). Bodies are already correct there:
//   int sizeWords  = ((size + 3) >> 2) + 1; if ((unsigned)sizeWords < 3) sizeWords = 3;
//   int alignShift = MemHeap::GetAlignWords(align);   // align==0 => 1
//   MemHeap::Strategy saved = heap->mStrategy;
//   if (temp) heap->mStrategy = MemHeap::kLastFit;
//   int out; void *r = heap->Alloc(sizeWords, alignShift, out);
//   heap->mStrategy = saved;
//   return r;
//
// ⚠ ONE ThreadMemStack(false) call, not two. Retail calls it once and
// RE-MATERIALISES the heap-num select at the null-heap branch off the still-live
// r3/r9/r8. Writing `GetCurrentHeapNum()` (which calls ThreadMemStack itself)
// plus a separate `ThreadMemStack(false)` for mTempRefs emits TWO calls, and
// MSVC cannot CSE a call away.
//
// ⚠ IT CANNOT SCORE UNTIL 0x827bcd38 IS NAMED. The address is unnamed in
// target_symbol_map.json, so the row is unpaired and worth 0 B no matter how
// good the body is. ALLOCGATE-1 declined the name for a sound reason: naming it
// converts ~104 objs' worth of currently-FORGIVEN placeholder call sites into
// checked ones. The right order is therefore: port the body, verify it offline
// by relocation-normalized byte identity (the L3_EXACT comparator in
// tools/alias_forgiveness_audit.py already does exactly this), and only then add
// the map name — at which point ab_measure prices the +644 B against the
// newly-checked sites as a single net number. Do NOT name it first.
__declspec(noinline) void *(MemAlloc)(int size, int align) {
    if (size <= 0)
        return nullptr;
    return malloc(size);
}
#endif

// Parenthesized name bypasses the X360 call-site macro that rewrites 5-arg
// `_MemAllocTemp(size, file, line, name, align)` down to the retail 2-arg form.
void *(_MemAllocTemp)(int size, const char *file, int line, const char *name, int align) {
    MemTemp tmp;
    // Parenthesized: route to the 5-arg debug stub, bypassing the X360 call-site
    // macro (internal MemMgr plumbing, not an external retail call site).
    return (MemAlloc)(size, file, line, name, align);
}

#ifndef HX_NATIVE
// Retail/match 2-arg temp allocator the X360 XEX actually calls (fn_827979D8):
// a temp-REFCOUNT scope guard around a 2-arg heap MemAlloc(size, align), no
// debug strings. Parenthesized name so the call-site macro doesn't recurse.
//
// ⚠ The guard is MemDoTempAllocations, NOT MemTemp — these are two DISTINCT
// retail constructs (see MemMgr.h) and this function had the wrong one. Retail
// inlines the refcount bump here rather than calling the out-of-line guard:
//
//     li r3,1 ; bl <ThreadMemStack> ; lwz r11,0x44(r3) ; addi r11,r11,1 ; stw ...
//     ... bl <MemAlloc> ...
//     li r3,1 ; bl <ThreadMemStack> ; lwz r11,0x44(r3) ; subi r11,r11,1 ; stw ...
//
// i.e. ThreadMemStack(true).mTempRefs++/-- with NO gNumHeaps guard and no frame
// slot. MemTemp is the OTHER guard (out-of-line ctor/dtor, homes an `int mOld`,
// swaps the heap's Strategy to kLastFit); using it here emitted an `addi r3,
// <frame>` this-pointer and two out-of-line calls, and cost the whole body.
void *(_MemAllocTemp)(int size, int align) {
    MemDoTempAllocations tmp;
    return (MemAlloc)(size, align);
}
#endif

#ifdef HX_NATIVE
void *MemOrPoolAllocSTL(int size, const char *file, int line, const char *name) {
    if (size == 0)
        return nullptr;
    return malloc(size);
}
#else
// Retail/match STL pool dispatcher. ADJUDICATED ON RETAIL BYTES, lane STLALLOC-1
// (2026-08-14). Retail RB3-360 has exactly ONE pool/heap alloc dispatcher, the
// 36-byte leaf at 0x827bd208, and every STL container `allocate()` reaches it:
// 159 of 176 map-named `_M_create_node`/`_M_insert_overflow*` functions call it
// directly and 2 more via an outlined helper; of the remainder, 2 use different
// allocator templates entirely (TransformListAlloc -> ReclaimableAlloc::CustAlloc,
// XboxAllocator -> MemAlloc/MemFree direct) and the rest are scan-window
// artifacts. NO rival dispatcher exists anywhere in .text.
//
// ⚠ TWO ORACLE FORMS ARE WRONG FOR RB3-360 — do not "fix" this back to either:
//
//   * dc3-decomp spells `{ MemTemp tmp; MemAlloc(...); }` — a temp guard. Retail
//     has NO guard here; 0x827bd208 is a frameless leaf that tail-branches
//     (`b`, not `bl`) to both callees, which a guard's dtor makes impossible.
//     DC3 is the NEWER engine; this is a DC3-side divergence.
//   * rb3-Wii spells the STL variant with threshold 0x100 vs 0x80 for the plain
//     variant. RB3-360 does NOT carry that split: a whole-.text scan finds ZERO
//     0x100-vs-r3 compares near any allocator branch (the same probe finds the
//     0x80 one, so it discriminates), and retail's surviving
//     ?MemOrPoolFreeSTL@@YAXHPAX@Z at 0x827bca50 compares against 0x80 too.
//
// ⇒ the STL and plain variants are byte- and relocation-identical here, so
// /OPT:ICF folds them. That is confirmed by retail's own internal inconsistency,
// needing no fold model: the FREE pair collapsed the same way and kept the *STL*
// survivor name (0x827bca50, the only MemOrPoolFree-family body in the image),
// while the ALLOC pair kept the *non-STL* name (0x827bd208). That survivor-name
// asymmetry is arbitrary ICF selection — two genuinely distinct functions could
// not produce it.
//
// Keeping this body identical to MemOrPoolAlloc(int) above is load-bearing: it
// is what lets tools/alloc_fold_gate.py prove the alias membership on our own
// two COMDATs (L4_OURSIDE) instead of asserting it.
void *MemOrPoolAllocSTL(int size) {
    if (size == 0) {
        return nullptr;
    } else if (size > 0x80) {
        return MemAlloc(size, 0);
    } else {
        return PoolAlloc(size, size);
    }
}
#endif

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
        mem = (MemAlloc)(newTotalSize, file, line, name);
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
        // The 2-arg retail allocator `MemAlloc(int size, int align)` is declared
        // ONLY `#ifndef HX_NATIVE` (utl/MemMgr.h:154-157), so the X360 spelling
        // below does not compile natively ("too few arguments to function call,
        // expected at least 4"). Natively, route to the 5-arg debug allocator --
        // the same call this line carried before wave4 f278d4d7.
#ifdef HX_NATIVE
        void *dst = (MemAlloc)(size, file, line, name, align);
#else
        void *dst = (MemAlloc)(size, (int)file);
#endif
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

// Retail/match: UNGUARDED. The retail out-of-line copies (fn_827BC270 push /
// fn_827BC2A0 pop, both in the MemHeap unit) are each exactly twelve
// instructions with no gNumHeaps load and no branch:
//
//     mflr r12 ; stw r12,-8(r1) ; stwu r1,-0x60(r1)
//     li r3,1 ; bl <ThreadMemStack> ; lwz r11,0x44(r3)
//     addi/subi r11,r11,1 ; stw r11,0x44(r3)
//     addi r1,r1,0x60 ; lwz r12,-8(r1) ; mtlr r12 ; blr
//
// The `gNumHeaps` guard was a Wii/DC3-side divergence; retail has no such test
// (and no MILO_ASSERT — that family is a no-op in this build anyway). Dropping
// it matters because these inline into same-TU callers such as _MemAllocTemp,
// where the guard's load+branch is exactly what kept the body from matching.
// ⚠ Keep the NAMED reference. `ThreadMemStack(true).mTempRefs++` as a single
// expression makes MSVC materialise the member address (`addi rN,r3,0x44` then
// `lwz r11,0(rN)`); binding the object to a reference first keeps it in a
// register and addresses the member by displacement, `lwz r11,0x44(r3)`, which
// is what retail emits. Same semantics, different addressing mode.
// The gNumHeaps guard is kept for HX_NATIVE only: natively these can be reached
// before any heap exists, and the guard is the pre-existing behaviour this lane
// has no evidence to change. The match build follows retail's bytes.
void MemPushTemp() {
#ifdef HX_NATIVE
    if (gNumHeaps > 0)
#endif
    {
        MemHeapStack &s = ThreadMemStack(true);
        s.mTempRefs++;
    }
}

void MemPopTemp() {
#ifdef HX_NATIVE
    if (gNumHeaps > 0)
#endif
    {
        MemHeapStack &s = ThreadMemStack(true);
        s.mTempRefs--;
    }
}

#ifdef HX_NATIVE
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
#else
// Retail/match no-arg temp-allocation scope guard (fn_82797500 / fn_827975C8).
// Locks gMemStackLock, captures the current heap's strategy into mOld, forces
// MemHeap::kLastFit for the scope; the dtor restores mOld. Matches the rb3-Wii
// MemDoTempAllocations ctor/dtor shape (CritSecTracker + GetCurrentHeapNum +
// gHeaps[]) but with the retail unconditional kLastFit strategy and no
// `enabled` static (verified byte-for-byte against the retail XEX).
MemTemp::MemTemp() {
    CritSecTracker tracker(gMemStackLock);
    int heapNum = GetCurrentHeapNum();
    MemHeap *heap = heapNum > -1 ? &gHeaps[heapNum] : nullptr;
    if (heap) {
        mOld = heap->mStrategy;
        heap->mStrategy = MemHeap::kLastFit;
    } else {
        mOld = -1;
    }
}

MemTemp::~MemTemp() {
    CritSecTracker tracker(gMemStackLock);
    if (mOld != -1) {
        int heapNum = GetCurrentHeapNum();
        MemHeap *heap = heapNum > -1 ? &gHeaps[heapNum] : nullptr;
        heap->mStrategy = (MemHeap::Strategy)mOld;
    }
}
#endif

// Retail/match 4-arg FreeBlockStats (fn_827963D8). Per the rb3-Wii oracle this
// is Heap::FreeBlockStats(int&,int&,int&,int&), grouped into the MemMgr.o TU.
// Walks the free-block chain: totalFree = sum of block bytes, biggest = largest
// block bytes, maxIdx = index of the largest block, rFrags = (count-maxIdx-1).
// No member-variable writes (unlike the 5-arg overload in MemHeap.cpp).
void MemHeap::FreeBlockStats(int &maxIdx, int &rFrags, int &totalFree, int &biggest) {
    int idx = 0;
    int maxBytes = 0;
    int sumBytes = 0;
    int maxBlock = -1;
    for (FreeBlock *it = mFreeBlockChain; it != nullptr; it = it->mNextBlock, idx++) {
        int bytes = it->mSizeWords * 4;
        if (maxBytes < bytes) {
            maxBytes = bytes;
            maxBlock = idx;
        }
        sumBytes += bytes;
    }
    totalFree = sumBytes;
    biggest = maxBytes;
    maxIdx = maxBlock;
    rFrags = (idx - maxBlock) - 1;
}

// Retail/match MemHandle::Lock (fn_827966E8, ExactInstructions, TU=MemMgr.o).
// mAlloc points at a MemHandleAlloc header (mBack@0, mLockCount@4) followed by
// the relocatable user data at +0x10. Lock bumps the lock count and hands back
// the data pointer.
void *MemHandle::Lock() {
    ++mAlloc->mLockCount;
    return (char *)mAlloc + 0x10;
}

void MemFreeBlockStats(
    int heapNum, int &i2, int &i3, int &numFreeBytes, int &i5, int &biggestFreeBlock
) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x154);
    gHeaps[heapNum].FreeBlockStats(i2, i3, numFreeBytes, i5, biggestFreeBlock);
}

// Retail/match 4-ref overload (rb3-Wii oracle
// MemFreeBlockStats(int, int&, int&, int&, int&)). DC3 grew a fifth out-param
// (minFreeBytes); RB3 retail predates it, so call sites that must match retail
// codegen (e.g. MetaMusic::Poll) use this arity.
void MemFreeBlockStats(int heapNum, int &a, int &b, int &c, int &d) {
    CritSecTracker tracker(gMemLock);
    MILO_ASSERT(heapNum < MAX_HEAPS, 0x154);
    gHeaps[heapNum].FreeBlockStats(a, b, c, d);
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
