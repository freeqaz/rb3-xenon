#pragma once
#include "Core/CallContextRegister.h"
#include "Core/InstanceControl.h"
#include "Core/PseudoSingleton.h"
#include "Core/SystemComponents.h"
#include "Platform/RefCountedObject.h"
#include "SecurityContextManager.h"

namespace Quazal {
    class Scheduler;

    class Core : public RefCountedObject {
    public:
        Core();
        virtual ~Core();

        void AcquireInstance();
        void ReleaseInstance();

        static bool s_bUsesThreads;
        static bool s_bIsThreadSafe;
        static unsigned int s_uiCoreCount;
        // lol, gotta love unsafe static casts
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : nullptr;
            return pCore;
        }

        Scheduler *GetScheduler() { return m_pScheduler; }

        Scheduler *m_pScheduler; // 0x8
        CallContextRegister *m_pCallContextRegister; // 0xc
        SystemComponents *m_pSystemComponents; // 0x10
        SecurityContextManager *m_pSecurityContextManager; // 0x14
        PseudoSingleton m_psInstance; // 0x18
    };
}