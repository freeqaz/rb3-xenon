// rb3-app: force-included into src/network/net/Net.cpp only (lane A5,
// 2026-11-22; native/CMakeLists.txt sets `-include` on that one TU). Native only.
//
// Net.cpp's Quazal allocator hooks (retail 0x823E0358 QuazalMemAlloc,
// 0x823E0398 QuazalMemFree) call the retail-ABI allocator entry points
// MemOrPoolAlloc(int) and MemOrPoolFreeSTL(int, void*). On HX_NATIVE,
// utl/MemMgr.h declares only the debug-argument forms (file, line, name), so
// those two calls did not compile. These overloads forward to the native forms
// with a tracking name; they add no behaviour. Other native TUs that need the
// same call spell it with an `#ifdef HX_NATIVE` arm (DataArray.cpp NodesAlloc).
//
// LP64 note [read, not exercised]: QuazalMemAlloc keeps the block size in a
// leading `unsigned long`, 8 bytes on LP64, but reserves only 4 (`size += 4`).
// Natively nothing calls the hooks: they are reached only through
// Quazal::MemoryManager::s_fcnMalloc/s_fcnFree, which the Quazal stack reads,
// and there is no Quazal stack natively. The rb3-app Net::Init gate does not
// install them either (rb3_app_gates.cpp).
#pragma once
#include "utl/MemMgr.h"

#ifdef HX_NATIVE
inline void *MemOrPoolAlloc(int size) {
    return MemOrPoolAlloc(size, "network/net/Net.cpp", 0, "QuazalMemAlloc");
}
inline void MemOrPoolFreeSTL(int idx, void *mem) {
    MemOrPoolFreeSTL(idx, mem, "network/net/Net.cpp", 0, "QuazalMemFree");
}
#endif
