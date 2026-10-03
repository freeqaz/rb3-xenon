// Quazal NetZ - .\WorkerThreads.cpp
//
// Retail TU: .text 0x82B000E8..0x82B007F0, compiled /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). It opens with the constructor (no EH prefix in front of
// it) and ends with the out-of-line vector clear() that Stop calls; the next
// object's first function is at 0x82B007F0. No .pdata record in the extent has
// the EH bit although Start and Stop hold a String and a ScopedCS, and the
// WorkerThreads vtable (0x821873DC: scalar deleting dtor, Initialize, Teardown,
// _purecall for Work) has no complete-object locator in front of it.
//
// The ObjectThread<WorkerThreads, int> methods are not in this object: retail
// uses the identical ObjectThread<UDPTransport, void *> code (vtable
// 0x82189D58).

#include "network/Core/WorkerThreads.h"
#include "Platform/ObjectThread.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"

namespace Quazal {
    WorkerThreads::WorkerThreads() : m_csState(0x40000000) { m_eState = Stopped; }

    WorkerThreads::~WorkerThreads() { Stop(); }

#line 32
    bool WorkerThreads::Start(unsigned int uiNbThreads) {
        ScopedCS oCS(m_csState);
        if (m_eState != Stopped)
            return false;
        m_eState = Running;
        for (unsigned int i = 0; i < uiNbThreads; i++) {
            String strName;
            strName.Format("WorkerThread ID %d", i + 1);
            ObjectThread<WorkerThreads, int> *pThread =
                new (__FILE__, __LINE__) ObjectThread<WorkerThreads, int>(strName);
            pThread->Update(this, &WorkerThreads::Run, 0, true);
            m_vecThreads.push_back(pThread);
        }
        return true;
    }

    bool WorkerThreads::Stop() {
        {
            ScopedCS oCS(m_csState);
            if (m_eState != Running)
                return false;
            m_eState = Stopping;
        }
        for (unsigned int i = 0; i < m_vecThreads.size(); i++) {
            m_vecThreads[i]->Wait(-1);
            delete m_vecThreads[i];
        }
        m_vecThreads.clear();
        {
            ScopedCS oCS(m_csState);
            m_eState = Stopped;
        }
        return true;
    }

    unsigned int WorkerThreads::GetNbWorkers() const { return m_vecThreads.size(); }

    void WorkerThreads::Run(int) {
        Initialize();
        while (m_eState == Running) {
            Work();
        }
        Teardown();
    }
}
