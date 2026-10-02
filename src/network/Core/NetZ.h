#pragma once
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class OperationManager;
    class SystemComponent;
    class ConnectionManager;
    class Listener;

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
        // JobConnectStation's accessors (retail 0x82AD1BC8 reads 0x1c; the
        // connection job holds a SystemComponent::Use on 0x20).
        ConnectionManager *GetConnectionManager() { return m_pConnectionManager; }
        SystemComponent *GetSystemComponent() { return m_pSystemComponent; }
        Listener *GetListener() { return m_pListener; }

        char m_unk4[0x18];
        ConnectionManager *m_pConnectionManager; // 0x1c
        SystemComponent *m_pSystemComponent; // 0x20
        char m_unk24[0x1C];
        Listener *m_pListener; // 0x40
        OperationManager *m_pOperationManager; // 0x44
        SystemComponent *m_pComponent48; // 0x48
    };
}
