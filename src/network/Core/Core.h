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
        // Three named locals, assigned through an if rather than a ?:. Retail
        // /Od callers never expand this (/Ob1 declines it) but reserve its
        // three locals, and both the local count and the if-form decide where
        // their own temporaries land.
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (pInstance != 0) {
                pCore = (Core *)pInstance->m_pDelegatorInstance;
            }
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