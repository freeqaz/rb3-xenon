#pragma once
#include "utl/Symbol.h"

class PhysMemTypeTracker {
public:
    PhysMemTypeTracker(Symbol);
    ~PhysMemTypeTracker();

    bool mActive; // 0x0
};

int PhysicalUsage();
void *PhysicalAlloc(int size);
void PhysicalFree(void *);
int ForceLinkXMemFuncs();

// Retail/match: THREE parameters (see Memory_Xbox.cpp). file/line are a
// dev-build tracking feature, kept for the native build only.
#ifdef HX_NATIVE
void *PhysicalAllocTracked(unsigned long size, unsigned long alignment, const char *file, int line, const char *name);
#else
void *PhysicalAllocTracked(unsigned long size, unsigned long alignment, const char *name);
#endif
// Retail/match: ONE parameter. Every retail call site of the tracked free
// (0x822733B8: DxRnd::AutoDelete, DxRnd::ReleaseAutoRelease,
// XMAReader::~XMAReader) sets only r3; the file/line/name arguments are a
// dev-build tracking feature, kept for the native build only.
#ifdef HX_NATIVE
void PhysicalFreeTracked(void *, const char *, int, const char *);
inline void PhysicalFreeTracked(void *p) { PhysicalFreeTracked(p, __FILE__, __LINE__, ""); }
#else
void PhysicalFreeTracked(void *);
#endif
