#pragma once
#include "MemHeap.h"
#ifdef HX_NATIVE
#include <cstddef> // for size_t
#endif

extern const char *gStlAllocName;
extern bool gStlAllocNameLookup;
extern class CriticalSection *gMemLock;
extern class CriticalSection *gMemStackLock;

void PhysDelta(const char *);
bool MemUseLowestMip();
bool MemUseLowestMipException(const char *);

/** Get the largest block of physical memory we can successfully allocate. */
int _GetFreePhysicalMemory();
/** Get the largest block of system memory we can successfully allocate. */
int _GetFreeSystemMemory();

const char *MemHeapName(int heapNum);
int MemFindAddrHeap(void *addr);
int GetCurrentHeapNum();
int MemNumHeaps();
int MemFindHeap(const char *);
void MemPushHeap(int heapNum);
void MemPopHeap();
void MemForceNewOperatorAlign(int align);
// Retail/match: SIX parameters, not eight. Both of retail's call sites of
// fn_827C43E0 -- XMemAlloc (fn_822735B0) and PhysicalAllocTracked
// (fn_82273350) -- set up r3..r8 ONLY: no r9, no r10, and no stores to the
// outgoing argument area. The X360 EABI passes integer arguments 7 and 8 in
// r9/r10, so their absence is decisive rather than suggestive, and the two
// sites agree. Our tree's trailing (const char *file, int line) are a
// dev-build tracking feature retail did not compile; they are kept for the
// native build only.
#ifdef HX_NATIVE
void MemTrackAlloc(int, int, const char *, void *, bool, unsigned char, const char *, int);
#else
void MemTrackAlloc(int, int, const char *, void *, bool, unsigned char);
#endif
void MemTrackFree(void *);
void MemTrackRealloc(void *, int, int, void *);
int MemHeapSize(int heapNum);
void MemPrint(int heapIdx, class TextStream &stream, bool freeOnly);
void MemInit();
void MemDelta(const char *msg, int heapNum);
int MemAllocSize(void *mem);
void *MemResizeElem(
    void *&mem,
    int &totalSize,
    void *cutPoint,
    int cutLength,
    int insertLength,
    const char *file,
    int line,
    const char *name
);
void MemFreeBlockStats(int, int &, int &, int &, int &, int &);
// Retail/match 4-ref overload — see MemMgr.cpp.
void MemFreeBlockStats(int, int &, int &, int &, int &);
void MemPrintOverview(int, char *const);
MemHeapStack &ThreadMemStack(bool);

#define kNoHeap -3
#define kSystemHeap -1

void MemPushTemp();
void MemPopTemp();

#ifdef HX_NATIVE
struct MemTemp {
    MemTemp() { MemPushTemp(); }
    ~MemTemp() { MemPopTemp(); }
};

class MemDoTempAllocations {
public:
    // Args defaulted so the no-arg call sites (rewritten for the X360 match,
    // where retail folds these to the no-arg MemTemp guard) still compile on the
    // native host, which keeps the push/pop temp-allocation mechanism.
    MemDoTempAllocations(bool = true, bool = false);
    ~MemDoTempAllocations();

    int mOld;
};
#else
// ⛔ THE COMMENT THAT USED TO SIT HERE WAS STALE AND CONTRADICTED THE CODE
// BELOW IT.  It claimed retail folds BOTH spellings into ONE out-of-line no-arg
// guard, that `MemDoTempAllocations` is "a typedef alias of the same guard",
// and that "there is NO MemPushTemp/MemTempRefs mechanism (that is a DC3-newer
// divergence)".  All three are refuted by this repo's own target:
//
//   * `?MemPushTemp@@YAXXZ` IS a named retail function — 0x827BC270, 48 bytes,
//     100.0% matched in report.json — and 0x827BC2A0 is its pop (identical body,
//     `subi` where push has `addi` on ThreadMemStack(true)+0x44).  The mechanism
//     exists in RB3 retail; it is not a DC3-era addition.
//   * 59 retail functions call 0x827BC270 directly, every one of them with a
//     BARE `bl` and no `addi r3, <frame>` this-setup — the empty-class shape
//     modelled by `MemDoTempAllocations` below, NOT the out-of-line shape.
//   * the ctor/dtor addresses it cited, fn_82797500 / fn_827975C8, no longer
//     denote functions at all: both fall INSIDE fn_82797298 (TrackWatcherImpl,
//     size 0x438) in the current config/45410914/symbols.txt.  They are left
//     over from an older split and must not be quoted as evidence.
//
// That stale text is also what made a dc3 lane report this header as carrying
// dc3's `MemTemp`/`MemDoTempAllocations` fabrication.  It does not: the class
// below IS already the real inline header guard (dc3's end state), and its
// 4-byte `??1MemDoTempAllocations@@QAA@XZ` COMDAT is emitted and paired today.
//
// What is NOT settled, and is deliberately left alone: whether the out-of-line
// `MemTemp` guard below exists in RB3 retail at all.  No retail address is
// identified for its ctor or dtor, and none of its remaining call sites
// (ObjectDir::Reserve, RndDir::HarvestPollables, HamSongData::Load,
// MidiParser::ParseNote, RndShaderMgr::PreInit, MidiParserMgr::OnNewTrack) is
// named in scripts/target_symbol_map.json, so none can be adjudicated on bytes.
// Collapsing it into the guard below measures NET POSITIVE whole-binary
// (+11 matched / +80 B / +1 honest), so it is a live suspect — but it needs a
// per-site retail adjudication, not a metric-led deletion.  See the commit that
// added this comment.  MemHeap.h befriends it for mStrategy access.
struct MemTemp {
    MemTemp();
    ~MemTemp();

    int mOld; // 0x0 — saved MemHeap::Strategy
};

// The game-wide `MemDoTempAllocations m;` guard is a DISTINCT retail construct
// from MemTemp. Its ctor/dtor are INLINE one-liners that each call a single
// out-of-line temp-alloc helper (retail fn_827BC270 / fn_827BC2A0: bump the
// per-thread temp-alloc refcount at +0x44). Because the class is EMPTY and the
// trivial ctor/dtor inline away, MSVC fully elides the object — every call site
// is just a bare `bl <helper>` at scope-entry and scope-exit, with NO
// `addi r3, <frame>` this-setup and NO reserved stack slot. Modeling it as a
// typedef of the out-of-line MemTemp (which homes an `int mOld` to the frame)
// added a spurious `addi r3` + a 4-byte slot that shifted every following local
// by +4 — the whole SongSort family B_STRUCT_OFFSET loss.
struct MemDoTempAllocations {
    MemDoTempAllocations() { MemPushTemp(); }
    ~MemDoTempAllocations() { MemPopTemp(); }
};
#endif

struct MemHeapTracker {
    MemHeapTracker(int x) { MemPushHeap(x); }
    ~MemHeapTracker() { MemPopHeap(); }
};

// rb3-Wii locked-heap handle API (used by MetaMusic's mRndHeap streaming path).
// Declarations only — additive, no codegen impact on other TUs.
struct MemHandleAlloc {
    class MemHandle *mBack; // 0x0
    int mLockCount;         // 0x4
};

class MemHandle {
public:
    MemHandle(void *);
    void *Lock();
    void Unlock();

    MemHandleAlloc *mAlloc;
};

MemHandle *_MemAllocH(int);
void MemFreeH(MemHandle *);

class MemTempHeap {
public:
    MemTempHeap(int x) { MemPushHeap(x); }
    ~MemTempHeap() { MemPopHeap(); }
};

void *MemTruncate(
    void *mem,
    int size,
    const char *file = "unknown",
    int line = 0,
    const char *name = "unknown"
);
void *_MemAllocTemp(int size, const char *file, int line, const char *name, int align);
// ⛔ PHANTOMS -- `_MemAlloc` / `_MemFree` are rb3-Wii (MWCC) spellings that
// RETAIL RB3-360 DOES NOT HAVE, and that we never defined either.  Adjudicated
// on retail bytes (lane W1b-GAME): across all 396 pinned target objs the
// mangled `?_MemAlloc@@` and `?_MemFree@@` appear ZERO times, while their real
// siblings `?MemAlloc@@`, `?MemFree@@`, `?_MemAllocTemp@@`, `?MemOrPoolFreeSTL@@`
// all appear -- so the absence is a measurement, not a lookup that silently
// failed.  Retail's whole global free family is MemFree / MemOrPoolFreeSTL /
// MemFreeH / MemTrackFree.  Every one of the 12 `_MemFree` call sites we had
// was freeing a buffer that had been allocated with `MemAlloc`, and objdiff
// showed retail calling `?MemFree@@YAXPAX@Z` at those very sites (e.g.
// UIStats::Terminate idx 10; the `_Temporary_buffer` dtor is an 8-byte thunk
// `lwz r3,0x8(r3); b ?MemFree@@`), so they were wrong callees, not a style
// choice.  Lane W0-ALLOC rewrote the 8 `_MemAlloc` sites; W1b-GAME rewrote the
// 12 `_MemFree` sites.
//
// The MIRROR-IMAGE case W0-ALLOC warned about (retail spelling X where we
// spell Y) is STRUCTURALLY EMPTY here rather than merely unobserved: retail
// never references either phantom anywhere, so there is no site that could
// want one.  ⚠ Note the per-unit relocation-count comparison is NOT a usable
// instrument for that question -- it "disagrees" on 361 of 396 units because
// our objs carry template COMDATs and inlined dtors that retail's pinned
// extents do not.  A gate that fires on 91% of units cannot discriminate.
//
// Kept ONLY for HX_NATIVE: the shared native engine (SHA-pinned sibling repo
// milo-native-engine, src/platform/RB3TexSharpen.cpp) still calls both, and by
// house rule only the coordinator bumps that pin -- so removing these outright
// would break a repo this lane does not own.  Under the match build they are
// gone, which makes the phantoms UNCALLABLE (a reintroduced call is now a
// compile error instead of a silent wrong relocation).
#ifdef HX_NATIVE
void *_MemAlloc(int size, int align); // rb3-Wii two-arg allocator (native only)
void _MemFree(void *mem);             // rb3-Wii free (native only)
#endif
void *
MemRealloc(void *mem, int size, const char *file, int line, const char *name, int align);
void *MemAlloc(int size, const char *file, int line, const char *name, int align = 0);
#ifndef HX_NATIVE
// Retail/match 2-arg heap allocator (size, align) — no debug strings. See the
// retail ABI note above; mirrors rb3-Wii _MemAlloc(int, int).
void *MemAlloc(int size, int align);
// Retail/match LEAF alloc: rewrite the inherited debug call sites
// `MemAlloc(size, __FILE__, line, name[, align])` down to the retail 2-arg
// `(MemAlloc)(size, align)`. The retail XEX strips MemTrack, so the call passes
// only size + align (verified in Ghidra: Function_827977D0(size, align)). The
// common debug form omits align (defaulted 0), so the macro forces align 0 and
// swallows the trailing file/line/name. The handful of sites that pass a real
// non-zero align (BlockMgr 4, Mic/BinkReader 0x80, operator new
// gNewOperatorAlign) bypass the macro with the parenthesized `(MemAlloc)(size,
// align)` form under their own HX_NATIVE guard. The 5-arg debug stub and the
// definition likewise parenthesize the name. HX_NATIVE keeps the 5-arg debug
// form above. NOTE: this macro must follow ALL MemAlloc declarations.
#define MemAlloc(size, file, line, name, ...) (MemAlloc)((size), 0)
// Retail/match 2-arg temp allocator (size, align) — no debug strings, mirroring
// the MemAlloc lever above. The retail RB3-360 XEX strips MemTrack from
// _MemAllocTemp too: SongMgr::SaveWrite calls it as fn_827979D8(size, align=0)
// with NO __FILE__/__LINE__/name string loads (verified via objdiff: target
// `li r4, 0x0; bl fn_827979D8` vs our 5-arg debug form with the "SongMgr" /
// "src/system/meta/SongMgr.cpp" / line-733 literals). Unlike the MemAlloc macro
// (which forces align 0), this PRESERVES the trailing align arg (StorePacked-
// Metadata passes 0x20) and swallows the file/line/name. The definition and the
// internal MemMgr plumbing parenthesize the name `(_MemAllocTemp)` to bypass the
// macro. HX_NATIVE keeps the 5-arg debug form above.
void *_MemAllocTemp(int size, int align);
#define _MemAllocTemp(size, file, line, name, align) (_MemAllocTemp)((size), (align))
#endif
#ifdef HX_NATIVE
void MemFree(
    void *mem, const char *file = "unknown", int line = 0, const char *name = "unknown"
);
#else
// Retail/match LEAF free: the retail RB3-360 XEX strips MemTrack debug
// instrumentation, so every MemFree call site passes ONLY the pointer — no
// __FILE__/line/name (verified in Ghidra: callers like the RndBitmap dtor pass
// a single reg; the debug literals are absent from the XEX). Mirrors rb3-Wii's
// 1-arg _MemFree(void*) and follows the MemOrPool/STL +52 + POOL_OVERLOAD
// precedent. The function is declared/defined 1-arg; a function-like macro
// rewrites the dozens of inherited 4-arg debug call sites
// `MemFree(p, __FILE__, line, name)` down to `(MemFree)(p)` so no debug regs
// are materialized. The definition (and any address-taken use) parenthesizes
// the name `(MemFree)` to suppress the macro. HX_NATIVE keeps the 4-arg debug
// form above (host tracking strings stay live).
void MemFree(void *mem);
#define MemFree(ptr, ...) (MemFree)(ptr)
#endif
// Retail/match allocation ABI. The retail RB3-360 XEX strips all MemTrack /
// MILO_ASSERT debug instrumentation, so its alloc call sites pass NO __FILE__/
// __LINE__/name args (verified in Ghidra: callers like fn_82798360 invoke the
// heap allocator as MemAlloc(size, align) — exactly 2 regs, no string loads;
// the "StringBuf"/__FILE__ literals are absent from the binary). This mirrors
// rb3-Wii's _MemOrPoolAlloc(int)/_MemAlloc(int,int) form and follows the same
// precedent as PoolAlloc.h's 2-arg POOL_OVERLOAD win. On HX_NATIVE we keep the
// debug form (default args carry real host tracking strings); on the X360 match
// build the entry points take only the size/idx the retail call site passes.
#ifdef HX_NATIVE
void *MemOrPoolAlloc(int size, const char *file, int line, const char *name);
void *MemOrPoolAllocSTL(int size, const char *file, int line, const char *name);
void MemOrPoolFree(
    int,
    void *mem,
    const char *file = "unknown",
    int line = 0,
    const char *name = "unknown"
);
void MemOrPoolFreeSTL(int, void *mem, const char *file, int line, const char *name);
#else
void *MemOrPoolAlloc(int size);
void *MemOrPoolAllocSTL(int size);
void MemOrPoolFree(int idx, void *mem);
void MemOrPoolFreeSTL(int idx, void *mem);
#endif

#ifdef HX_NATIVE
// On 64-bit native, operator new takes size_t (unsigned long), not unsigned int
void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *mem) noexcept;
void operator delete[](void *mem) noexcept;


#define OBJ_MEM_OVERLOAD(line_num)                                                       \
    static void *operator new(size_t s) {                                                \
        return MemAlloc(s, __FILE__, line_num, StaticClassName().Str(), 0);              \
    }                                                                                    \
    static void *operator new(size_t s, void *place) { return place; }                   \
    static void operator delete(void *v) {                                               \
        MemFree(v, __FILE__, line_num, StaticClassName().Str());                         \
    }

#define MEM_OVERLOAD(class_name, line_num)                                               \
    static void *operator new(size_t s) {                                                \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new(size_t s, void *place) { return place; }                   \
    static void operator delete(void *v) { MemFree(v, __FILE__, line_num, #class_name); }

#define MEM_ARRAY_OVERLOAD(class_name, line_num)                                         \
    static void *operator new[](size_t s) {                                              \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new[](size_t s, void *place) { return place; }                 \
    static void operator delete[](void *v) {                                             \
        MemFree(v, __FILE__, line_num, #class_name);                                     \
    }
#else
void *operator new(unsigned int size);
void *operator new[](unsigned int size);
void operator delete(void *mem);
void operator delete[](void *mem);

// Retail/match: keep class operator new/delete OUT OF LINE. After the
// debug-arg-stripping `MemAlloc` macro reduces these bodies to the trivial
// `return MemAlloc(s, 0)`, MSVC /Ob2 would otherwise INLINE operator new into
// every `new Foo` call site (emitting a direct 2-arg `MemAlloc(size, 0)` with an
// extra `li r4, 0`). The retail XEX instead keeps operator new out-of-line and
// ICF-folds every identical `{ return MemAlloc(size, 0); }` into a single thunk
// (fn_82709EE0), so each `new` site is just `li r3, size; bl <operator new>`
// (verified on CacheMgr::CreateCacheMgr: target `bl fn_82709EE0` vs our inlined
// 2-arg MemAlloc). __declspec(noinline) reproduces that: the body still folds to
// fn_82709EE0, but the call site no longer inlines it. operator delete likewise.
// OBJ_MEM_OVERLOAD is the EXCEPTION to the out-of-line rule above. Retail's
// `MemAlloc` strip happened inside an inline *function*, not a macro, so the
// `StaticClassName()` name argument was still EVALUATED (and its result thrown
// away) before the 2-arg `MemAlloc(size, 0)` call. That makes the body
// class-specific -> it does NOT ICF-fold, and MSVC /Ob2 inlines it into the
// single `new Foo` call site. Measured in the retail XEX at every
// `Foo::NewObject` of an OBJ_MEM_OVERLOAD class (e.g. RndDir::NewObject
// fn_8240F380): `addi r3,r31,0x50 ; bl ?StaticClassName@RndDir@@ ;
// li r4,0 ; li r3,0x214 ; bl <MemAlloc> ; stw r3,0x54(r31)`. The discarded
// Symbol temp is what pushes the EH-cleanup raw-pointer temp from +0x50 to
// +0x54 in every such NewObject's unwind funclet.
// This is the GENERALISATION of `FXSEND360_NEW` (src/system/synth_xbox/FxSend.h,
// 2026-07-18), which already carries the measured form for the 10 FxSend*360
// classes: the `.Str()` call is load-bearing -- a bare `StaticClassName();`
// discard homes the Symbol temp into the SAME slot as `mem`, while calling
// `.Str()` on it reproduces retail's separate-slot placement. Keep both the
// `.Str()` and the named `mem` local.
// MEM_OVERLOAD (string-literal name) keeps `noinline`: its name argument is a
// literal, the body really does reduce to `MemAlloc(size, 0)`, it ICF-folds,
// and retail calls it out of line.
#define OBJ_MEM_OVERLOAD(line_num)                                                       \
    static void *operator new(unsigned int s) {                                          \
        (void)StaticClassName().Str();                                                   \
        void *mem = (MemAlloc)(s, 0);                                                    \
        return mem;                                                                      \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    __declspec(noinline) static void operator delete(void *v) {                          \
        MemFree(v, __FILE__, line_num, StaticClassName().Str());                         \
    }

#define MEM_OVERLOAD(class_name, line_num)                                               \
    __declspec(noinline) static void *operator new(unsigned int s) {                     \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    __declspec(noinline) static void operator delete(void *v) {                          \
        MemFree(v, __FILE__, line_num, #class_name);                                      \
    }

#define MEM_ARRAY_OVERLOAD(class_name, line_num)                                         \
    __declspec(noinline) static void *operator new[](unsigned int s) {                   \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new[](unsigned int s, void *place) { return place; }           \
    __declspec(noinline) static void operator delete[](void *v) {                        \
        MemFree(v, __FILE__, line_num, #class_name);                                     \
    }
#endif

// rb3-Wii style NEW_OVERLOAD/DELETE_OVERLOAD (no class name / line tracking).
// dc3 only exposes MEM_OVERLOAD/OBJ_MEM_OVERLOAD; the rb3-Wii Fader.h uses the
// terser spelling, so provide it for header compatibility.
#ifdef HX_NATIVE
#define NEW_OVERLOAD                                                                     \
    static void *operator new(size_t s) {                                                \
        return MemAlloc(s, __FILE__, 0, "unknown", 0);                                   \
    }                                                                                    \
    static void *operator new(size_t s, void *place) { return place; }

#define DELETE_OVERLOAD                                                                  \
    static void operator delete(void *v) { MemFree(v, __FILE__, 0, "unknown"); }
#else
#define NEW_OVERLOAD                                                                     \
    __declspec(noinline) static void *operator new(unsigned int s) {                     \
        return MemAlloc(s, __FILE__, 0, "unknown", 0);                                   \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }

#define DELETE_OVERLOAD                                                                  \
    __declspec(noinline) static void operator delete(void *v) {                          \
        MemFree(v, __FILE__, 0, "unknown");                                              \
    }
#endif

// #define NEW_ARRAY_OVERLOAD \
//     void *operator new[](size_t t) { return _MemAlloc(t, 0); } \ void *operator
//     new[](size_t, void *place) { return place; }

// #define DELETE_ARRAY_OVERLOAD \
//     void operator delete[](void *v) { _MemFree(v); }
