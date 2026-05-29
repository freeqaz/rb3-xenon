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
void MemTrackAlloc(int, int, const char *, void *, bool, unsigned char, const char *, int);
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
void MemPrintOverview(int, char *const);
MemHeapStack &ThreadMemStack(bool);

#define kNoHeap -3
#define kSystemHeap -1

void MemPushTemp();
void MemPopTemp();

struct MemTemp {
    MemTemp() { MemPushTemp(); }
    ~MemTemp() { MemPopTemp(); }
};

class MemDoTempAllocations {
public:
    MemDoTempAllocations(bool, bool);
    ~MemDoTempAllocations();

    int mOld;
};

struct MemHeapTracker {
    MemHeapTracker(int x) { MemPushHeap(x); }
    ~MemHeapTracker() { MemPopHeap(); }
};

void *MemTruncate(
    void *mem,
    int size,
    const char *file = "unknown",
    int line = 0,
    const char *name = "unknown"
);
void *_MemAllocTemp(int size, const char *file, int line, const char *name, int align);
void *_MemAlloc(int size, int align); // rb3-Wii two-arg allocator used in STLPort specializations
void _MemFree(void *mem);             // rb3-Wii free used in STLPort specializations
void *
MemRealloc(void *mem, int size, const char *file, int line, const char *name, int align);
void *MemAlloc(int size, const char *file, int line, const char *name, int align = 0);
#ifndef HX_NATIVE
// Retail/match 2-arg heap allocator (size, align) — no debug strings. See the
// retail ABI note above; mirrors rb3-Wii _MemAlloc(int, int).
void *MemAlloc(int size, int align);
#endif
void MemFree(
    void *mem, const char *file = "unknown", int line = 0, const char *name = "unknown"
);
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

#define OBJ_MEM_OVERLOAD(line_num)                                                       \
    static void *operator new(unsigned int s) {                                          \
        return MemAlloc(s, __FILE__, line_num, StaticClassName().Str(), 0);              \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    static void operator delete(void *v) {                                               \
        MemFree(v, __FILE__, line_num, StaticClassName().Str());                         \
    }

#define MEM_OVERLOAD(class_name, line_num)                                               \
    static void *operator new(unsigned int s) {                                          \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }             \
    static void operator delete(void *v) { MemFree(v, __FILE__, line_num, #class_name); }

#define MEM_ARRAY_OVERLOAD(class_name, line_num)                                         \
    static void *operator new[](unsigned int s) {                                        \
        return MemAlloc(s, __FILE__, line_num, #class_name, 0);                          \
    }                                                                                    \
    static void *operator new[](unsigned int s, void *place) { return place; }           \
    static void operator delete[](void *v) {                                             \
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
    static void *operator new(unsigned int s) {                                          \
        return MemAlloc(s, __FILE__, 0, "unknown", 0);                                   \
    }                                                                                    \
    static void *operator new(unsigned int s, void *place) { return place; }

#define DELETE_OVERLOAD                                                                  \
    static void operator delete(void *v) { MemFree(v, __FILE__, 0, "unknown"); }
#endif

// #define NEW_ARRAY_OVERLOAD \
//     void *operator new[](size_t t) { return _MemAlloc(t, 0); } \ void *operator
//     new[](size_t, void *place) { return place; }

// #define DELETE_ARRAY_OVERLOAD \
//     void operator delete[](void *v) { _MemFree(v); }
