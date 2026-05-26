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
void *
MemRealloc(void *mem, int size, const char *file, int line, const char *name, int align);
void *MemAlloc(int size, const char *file, int line, const char *name, int align = 0);
void MemFree(
    void *mem, const char *file = "unknown", int line = 0, const char *name = "unknown"
);
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

// #define NEW_ARRAY_OVERLOAD \
//     void *operator new[](size_t t) { return _MemAlloc(t, 0); } \ void *operator
//     new[](size_t, void *place) { return place; }

// #define DELETE_ARRAY_OVERLOAD \
//     void operator delete[](void *v) { _MemFree(v); }
