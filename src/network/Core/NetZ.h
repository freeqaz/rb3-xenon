#pragma once
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class OperationManager;
    class SystemComponent;
    class SessionDiscoveryTable;
    class StationIdentification;

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

        // Session's reads (0x82A76B58..0x82A799F8).
        SessionDiscoveryTable *GetSessionDiscoveryTable() { return m_pSessionDiscoveryTable; }
        StationIdentification *GetStationIdentification() { return m_pStationIdentification; }
        void StartSessionServices();

        char m_unk4[0x28];
        SessionDiscoveryTable *m_pSessionDiscoveryTable; // 0x2c
        char m_unk30[0xc];
        StationIdentification *m_pStationIdentification; // 0x3c
        char m_unk40[0x4];
        OperationManager *m_pOperationManager; // 0x44
        SystemComponent *m_pComponent48; // 0x48
    };
}
