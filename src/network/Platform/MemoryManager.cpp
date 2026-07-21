#include "Platform/MemoryManager.h"
#include "ExceptionHandler.h"
#include "Platform/MutexPrimitive.h"
#include "WaterMark.h"
#include "decomp.h"
#include <stdlib.h>

namespace Quazal {
    AllocFunc *MemoryManager::s_fcnMalloc;
    FreeFunc *MemoryManager::s_fcnFree;
    unsigned int MemoryManager::s_uiHeaderSize;
    unsigned int MemoryManager::s_eShutDownState;
    MemoryManager *MemoryManager::s_poDefaultMemoryManager;

    MemoryManager::MemoryManager(char *name)
        : m_szName(name), unk8(0), unkc(0), unk10(0), m_pMemoryWaterMark(0), m_pMutex(0) {
        m_pMutex = new (__FILE__, 0x7E) MutexPrimitive();
        m_pMemoryWaterMark = new (__FILE__, 0x80) WaterMark(name, true, 60000);
    }

    MemoryManager::~MemoryManager() {}

    void *MemoryManager::Allocate(
        MemoryManager *mgr, u32 size, const char *file, uint line, _InstructionType
    ) {
        int mallocSize = size + (4 - (size & 3) & 3) + 4;
        void *malloced = GenericMalloc(mallocSize);
        ExceptionHandler::CheckCrtExceptions();
        if (mgr)
            mgr->BeginProtection();
        unsigned int offset = GetHeaderSize();
        *(FreeFunc **)malloced = s_fcnFree;
        void *ret = (char *)malloced + offset;
        if (mgr)
            mgr->EndProtection();
        return ret;
    }

    void MemoryManager::BeginProtection() {
        MutexPrimitive *mutex = m_pMutex;
        if (!MutexPrimitive::s_bNoOp) {
            mutex->EnterImpl();
        }
    }

    void MemoryManager::EndProtection() {
        MutexPrimitive *mutex = m_pMutex;
        if (!MutexPrimitive::s_bNoOp) {
            mutex->LeaveImpl();
        }
    }

    UNPOOL_DATA
    void MemoryManager::Free(MemoryManager *mgr, void *mem, _InstructionType ty) {
        if (mem) {
            bool state = s_eShutDownState == 1;
            if (mgr && !state) {
                mgr->BeginProtection();
            }
            void *toFree = (char *)mem - GetHeaderSize();
            FreeFunc *f = *(FreeFunc **)toFree;
            if (f)
                f(toFree);
            else
                free(toFree);
            ExceptionHandler::CheckCrtExceptions();
            if (mgr && !state) {
                mgr->EndProtection();
            }
        }
    }
END_UNPOOL_DATA

MemoryManager *MemoryManager::GetDefaultMemoryManager() {
    MemoryManager *mgr = s_poDefaultMemoryManager;
    if (mgr)
        return mgr;
    static bool s_bConstructionInProgress;
    if (s_bConstructionInProgress)
        return nullptr;
    s_bConstructionInProgress = true;
    mgr = new (__FILE__, 0x162) MemoryManager("Default memory manager");
    s_poDefaultMemoryManager = mgr;
    return mgr;
}
}
