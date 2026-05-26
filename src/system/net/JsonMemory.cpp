#include "JsonMemory.h"
#include "utl/MemMgr.h"
#include <cstring>

void *JsonMalloc(int size) { return MemAlloc(size, "Json", 0, "unknown"); }

void *JsonCalloc(int i1, int i2) {
    void *dst = MemAlloc(i1 * i2, "Json", 0, "unknown");
    memset(dst, 0, i1 * i2);

    return dst;
}

void *JsonRealloc(void *mem, int size) {
    void *dst = MemRealloc(mem, size, "Json", 0, "unknown", 0);
    return dst;
}
