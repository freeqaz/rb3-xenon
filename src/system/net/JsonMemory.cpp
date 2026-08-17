#include "JsonMemory.h"
#include "utl/MemMgr.h"
#include <cstring>

void *JsonMalloc(int size) { return MemAlloc(size, "Json", 0, "unknown"); }

void *JsonCalloc(int i1, int i2) {
    // Retail allocates the calloc'd block from the TEMP heap: the retail JsonCalloc
    // body is word-identical to ours except `bl ?_MemAllocTemp@@YAPAXHH@Z` here.
    // (JsonMalloc above is absent from the retail objs, so it is left as-is rather
    // than guessed.) Lane W0-ALLOC.
    void *dst = _MemAllocTemp(i1 * i2, "Json", 0, "unknown", 0);
    memset(dst, 0, i1 * i2);

    return dst;
}

void *JsonRealloc(void *mem, int size) {
    void *dst = MemRealloc(mem, size, "Json", 0, "unknown", 0);
    return dst;
}
