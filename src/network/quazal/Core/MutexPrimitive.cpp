// Quazal NetZ - .\Core\MutexPrimitive.cpp
//
// Retail TU: .text 0x82A801F0..0x82A80378, compiled /Od /Oi- /Ob1 (see
// objects.json). It holds the four MutexPrimitive members followed by the
// two allocation helpers they instantiate for the RTL_CRITICAL_SECTION.
// The next object (CriticalSection's constructor at 0x82A80378) starts right
// after those helpers. Its .rdata is the __FILE__ string alone (no vtable, no
// EH tables).
//
// On Xbox 360 the mutex is an RTL_CRITICAL_SECTION (0x1C bytes) allocated
// from the default memory manager with instruction type 8.

#include "Platform/MutexPrimitive.h"
#include "Platform/MemoryManager.h"
#include "xdk/XBOXKRNL.h"

namespace Quazal {

    // Allocate/free one uninitialized T from the default memory manager. The
    // names are not retail-attested.
    template <class T>
    T *qNewPOD(const char *szFile, unsigned int uiLine) {
        void *pMem = MemoryManager::Allocate(
            MemoryManager::GetDefaultMemoryManager(),
            sizeof(T),
            szFile,
            uiLine,
            MemoryManager::_InstType8
        );
        T *pObject = (T *)pMem;
        return pObject;
    }

    template <class T>
    void qDeletePOD(T *pObject) {
        MemoryManager::Free(
            MemoryManager::GetDefaultMemoryManager(), pObject, MemoryManager::_InstType8
        );
    }

    bool MutexPrimitive::s_bNoOp;

    MutexPrimitive::MutexPrimitive() {
#line 76
        m_hMutex = qNewPOD<RTL_CRITICAL_SECTION>(__FILE__, __LINE__);
        RtlInitializeCriticalSection((RTL_CRITICAL_SECTION *)m_hMutex);
    }

    MutexPrimitive::~MutexPrimitive() {
        qDeletePOD((RTL_CRITICAL_SECTION *)m_hMutex);
        m_hMutex = 0;
    }

    void MutexPrimitive::EnterImpl() {
        RtlEnterCriticalSection((RTL_CRITICAL_SECTION *)m_hMutex);
    }

    void MutexPrimitive::LeaveImpl() {
        RtlLeaveCriticalSection((RTL_CRITICAL_SECTION *)m_hMutex);
    }

}
