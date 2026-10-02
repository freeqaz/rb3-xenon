#pragma once
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class SafetyExecutive;
    class DOProtocol;
    class StationConnectionManager;
    class StationManager;
    class DOSelections;
    class ObjDupProtocol;
    class SessionDiscoveryTable;
    class DOProtocolServer;
    class ProtocolRequestBroker;
    class BundlingPolicy;
    class StationIdentification;
    class PRUDPStream;
    class OperationManager;
    class ComponentState;
    class DOSubset;
    class ErrorDescriptionTable;

    // .\DOCore.cpp (0x82AC0470..0x82AC1808). The type-4 PseudoSingleton: it
    // owns the ObjDup components, created in this order by the constructor.
    // DOCore.cpp declares the class itself (with the same layout) because it
    // needs InstanceTable::GetInstanceFromVector as a plain inline, which
    // Core/InstanceTable.h marks __declspec(noinline).
    class DOCore : public PseudoSingleton {
    public:
        DOCore();
        virtual ~DOCore();

        static DOCore *GetInstance(unsigned int uiContext) {
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static DOCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        // The spelling DuplicatedObject::ValidOperation inlines (the context
        // in a named local, the result in a temp).
        static DOCore *GetCurrentInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }

        void TraceSystemState(unsigned int);
        void QueuePeriodicJobs();
        void CancelPeriodicJobs();
        bool IsReadyToLeave();
        void SetToReadyState();
        void SetToTerminatingState();
        void SetToTerminatedState();
        void SetToCorruptedState();
        bool IsTerminating() const;
        bool IsTerminated() const;
        bool IsCorrupted() const;
        bool HasStartedTermination() const;

        ComponentState *GetDOCoreState() const { return m_pDOCoreState; }
        StationManager *GetStationManager() const { return m_pStationManager; }
        // Called on the first construction and the last destruction; empty
        // in this build (retail calls a shared empty body).
        void RegisterStatics();
        void UnregisterStatics();

        static unsigned int s_uiDOCoreCount;
        static ErrorDescriptionTable s_oErrorTable;

        SafetyExecutive *m_pSafetyExecutive; // 0x14
        DOProtocol *m_pDOProtocol; // 0x18
        StationConnectionManager *m_pStationConnectionManager; // 0x1c
        StationManager *m_pStationManager; // 0x20
        DOSelections *m_pDOSelections; // 0x24
        ObjDupProtocol *m_pObjDupProtocol; // 0x28
        SessionDiscoveryTable *m_pSessionDiscoveryTable; // 0x2c
        DOProtocolServer *m_pDOProtocolServer; // 0x30
        ProtocolRequestBroker *m_pProtocolRequestBroker; // 0x34
        BundlingPolicy *m_pBundlingPolicy; // 0x38
        StationIdentification *m_pStationIdentification; // 0x3c
        PRUDPStream *m_pStream; // 0x40
        OperationManager *m_pOperationManager; // 0x44
        ComponentState *m_pJoinProcessing; // 0x48
        ComponentState *m_pDOCoreState; // 0x4c
        DOSubset *m_pUserDOs; // 0x50
        ComponentState *m_pFaultProcessing; // 0x54
    };
}
