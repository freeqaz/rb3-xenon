#ifndef PLATFORM_MUTEXPRIMITIVE_H
#define PLATFORM_MUTEXPRIMITIVE_H
#include "Platform/MemoryManager.h"
#include "Platform/RootObject.h"
// On Xbox 360, the mutex is backed by RTL_CRITICAL_SECTION (XBOXKRNL).
// Use void* for the pointer so the header compiles without pulling in
// revolution/OS.h. MutexPrimitive.cpp (not yet in this build) supplies
// the actual RTL_CRITICAL_SECTION allocation/init/lock.
namespace Quazal {
    class MutexPrimitive : public RootObject {
    public:
        MutexPrimitive();
        ~MutexPrimitive();
        void EnterImpl();
        void LeaveImpl();

        static bool s_bNoOp;

        void *m_hMutex; // Xbox: RTL_CRITICAL_SECTION * (was OSMutex * on Wii)
    };
}

#endif
