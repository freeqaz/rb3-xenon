#pragma once
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class OperationManager;

    // The instance-table type-4 object of the current context.
    class NetZ {
    public:
        virtual ~NetZ();

        static NetZ *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (NetZ *)InstanceTable::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        OperationManager *GetOperationManager() { return m_pOperationManager; }

        char m_unk4[0x40];
        OperationManager *m_pOperationManager; // 0x44
    };
}
