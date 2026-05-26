// DC3 Native Port - Memory System
// Replaces Memory_Xbox.cpp - redirects to standard malloc/free

#include <cstdlib>
#include <cstring>
#include "xdk/XAPILIB.h"

void PhysicalFree(void *address) {
    free(address);
}

void PhysicalFreeTracked(void *address, const char *, int, const char *) {
    free(address);
}

int PhysicalUsage() {
    return 0;
}

// PhysMemTypeTracker — used for Xbox physical memory tracking, no-op on native
#include "Memory.h"
PhysMemTypeTracker::PhysMemTypeTracker(Symbol) {}
PhysMemTypeTracker::~PhysMemTypeTracker() {}
