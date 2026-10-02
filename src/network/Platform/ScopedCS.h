#pragma once
#include "Platform/CriticalSection.h"
#include "Platform/RootObject.h"
#include "Platform/MutexPrimitive.h"

namespace Quazal {
    class ScopedCS : public RootObject {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), critSec(&cs) { critSec->Enter(); }

        ~ScopedCS() { EndScope(); }

        void EndScope() {
            if (m_bInScope) {
                critSec->Leave();
                m_bInScope = false;
            }
        }

        bool m_bInScope; // 0x0
        CriticalSection *critSec;
    };
}
