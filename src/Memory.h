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
void PhysicalFreeTracked(void *, const char *, int, const char *);
