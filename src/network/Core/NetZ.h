#pragma once
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class OperationManager;
    class SystemComponent;

    // The instance-table type-4 object of the current context.
    class NetZ {
    public:
        virtual ~NetZ();

        static NetZ *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (NetZ *)InstanceTable::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        OperationManager *GetOperationManager() { return m_pOperationManager; }
        // The component Station::SetState initializes when a station reaches
        // state 3 (retail 0x82A7BF08).
        SystemComponent *GetComponent48() { return m_pComponent48; }

        char m_unk4[0x40];
        OperationManager *m_pOperationManager; // 0x44
        SystemComponent *m_pComponent48; // 0x48
    };
}
