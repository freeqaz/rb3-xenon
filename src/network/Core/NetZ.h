#pragma once
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class OperationManager;
    class SystemComponent;
    class StationManager;
    class SessionDiscoveryTable;
    class StationIdentification;
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
        void CompleteJoin();
        OperationManager *GetOperationManager() { return m_pOperationManager; }
        // The component Station::SetState initializes when a station reaches
        // state 3 (retail 0x82A7BF08).
        SystemComponent *GetComponent48() { return m_pComponent48; }
        // JobConnectStation's accessors (retail 0x82AD1BC8 reads 0x1c; the
        // connection job holds a SystemComponent::Use on 0x20).
        ConnectionManager *GetConnectionManager() { return m_pConnectionManager; }
        // The StationManager at 0x20, read as its SystemComponent base (offset 0).
        SystemComponent *GetSystemComponent() { return (SystemComponent *)m_pStationManager; }
        Listener *GetListener() { return m_pListener; }

        StationManager *GetStationManager() { return m_pStationManager; }
        // Session's reads (0x82A76B58..0x82A799F8).
        SessionDiscoveryTable *GetSessionDiscoveryTable() { return m_pSessionDiscoveryTable; }
        StationIdentification *GetStationIdentification() { return m_pStationIdentification; }
        void StartSessionServices();

        char m_unk4[0x18];
        ConnectionManager *m_pConnectionManager; // 0x1c
        StationManager *m_pStationManager; // 0x20
        char m_unk24[0x8];
        SessionDiscoveryTable *m_pSessionDiscoveryTable; // 0x2c
        char m_unk30[0xc];
        StationIdentification *m_pStationIdentification; // 0x3c
        Listener *m_pListener; // 0x40
        OperationManager *m_pOperationManager; // 0x44
        SystemComponent *m_pComponent48; // 0x48
    };
}
