// Quazal NetZ - .\DOCore.cpp
//
// The retail TU is 0x82AC0470..0x82AC1808: the constructor, the
// ComponentState and UserDOFilter in-class virtuals its vtables need, the
// scalar deleting destructors, then the destructor and the state methods in
// source order, ending at HasStartedTermination. The code from 0x82AC1808 on
// belongs to the SessionDiscovery TUs (0x82AC1B70 is SessionDiscoveryTable's
// constructor; its EH tables sit in that TU's .rdata).
//
// Built /Od /Ob1 with EH off. The classes it uses are declared here only as
// far as this TU uses them. DOCore itself is declared here too rather than
// through ObjDup/DOCore.h: that header reaches Core/InstanceTable.h, whose
// GetInstanceFromVector is __declspec(noinline), and in this TU retail has it
// as a plain inline that /Ob1 declines, so its three argument slots stay
// reserved in the caller (SetToCorruptedState's frame).

#include "Core/InstantiationContext.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "Platform/qStd.h"

namespace Quazal {

    class InstanceControl;

    class InstanceTable : public RootObject {
    public:
        unsigned int GetInstanceFromVector(unsigned int ui, unsigned int idx) {
            if (idx == 0) {
                return m_oDefaultContext.GetInstance(ui);
            } else if (idx >= m_pvContextVector->size()) {
                SystemError::SignalError(0, 0, 0xe0000003, 0);
                return -1;
            } else {
                return (*m_pvContextVector)[idx]->GetInstance(ui);
            }
        }

        // The same lookup, declared out of line for Core::GetInstance below:
        // retail reserves only Core::GetInstance's own three locals in
        // TraceSystemState's frame, not a nested GetInstanceFromVector's.
        unsigned int LookupInstance(unsigned int, unsigned int);

        InstantiationContext m_oDefaultContext; // 0x0
        qVector<InstantiationContext *> *m_pvContextVector; // 0x30
    };

    class InstanceControl : public RootObject {
    public:
        virtual ~InstanceControl();

        static InstanceTable s_oInstanceTable;

        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
        bool m_bIsValid; // 0x10
    };

    class PseudoSingleton : public InstanceControl {
    public:
        PseudoSingleton(unsigned int);
        virtual ~PseudoSingleton();

        static unsigned int GetCurrentContext();
    };

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

    // The type-4 PseudoSingleton (as in ObjDup/DOCore.h).
    class DOCore : public PseudoSingleton {
    public:
        DOCore();
        virtual ~DOCore();

        static DOCore *GetInstance(unsigned int uiContext) {
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static DOCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }

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

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent; // 0x0
    };

    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}

        unsigned int mValue; // 0x0
    };

    struct qResult {
        int m_iCode;
        int m_iLine;
        const char *m_szFile;
    };

    class Network {
    public:
        static void AcquireInstance();
        static void ReleaseInstance();
    };

    class Operation {
    public:
        typedef bool (*TraceFilter)(unsigned int);
        static TraceFilter s_pfTraceFilter;
        static bool DefaultTraceFilter(unsigned int);
    };

    class Station {
    public:
        static void SetLocalStation(DOHandle);
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
            Terminating = 0x10,
            Terminated = 0x40,
            Faulty = 0x80
        };

        static const char *type() { return "SystemComponent"; }

        SystemComponent(const String &);
        virtual ~SystemComponent();
        virtual void *AcquireRef();
        virtual void ReleaseRef();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const { return type() == str; }
        virtual void EnforceDeclareSysComponentMacro() = 0;

        _State GetState() const { return mState; }
        bool SetState(_State, bool);
        _State Initialize();

        unsigned short m_ui16RefCount; // 0x4
        String mName; // 0x8
        _State mState; // 0xc
        unsigned int mRefs; // 0x10
        SystemComponent *mParent; // 0x14
    };

    class ComponentState : public SystemComponent {
    public:
        static const char *type() { return "ComponentState"; }

        ComponentState(const String &strName) : SystemComponent(strName) {}
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const {
            return type() == str || SystemComponent::IsAKindOf(str);
        }
        virtual void EnforceDeclareSysComponentMacro();
    };

    class SafetyExecutive : public RootObject {
    public:
        SafetyExecutive();
        ~SafetyExecutive();

        char m_pad[0xC];
    };

    class BundlingPolicy : public RootObject {
    public:
        BundlingPolicy();
        ~BundlingPolicy();

        char m_pad[0x10];
    };

    class StationManager : public SystemComponent {
    public:
        StationManager();
        virtual void EnforceDeclareSysComponentMacro();

        char m_pad[0x48];
    };

    class DOFilter;

    class DOSelections : public RootObject {
    public:
        DOSelections();
        ~DOSelections();

        static DOSelections *GetInstance();
        void Trace(unsigned int, DOFilter *);
        void ClearOnCorruption();

        char m_pad[0x6C];
    };

    class DOProtocol : public RootObject {
    public:
        DOProtocol();
        ~DOProtocol();
    };

    class RootTransport;

    class Stream {
    public:
        enum Type {
        };
    };

    class StreamBundling {
    public:
        void Enable(int);
    };

    class StreamSettings {
    public:
        void EnableBundling(int iEnable) { m_oBundling.Enable(iEnable); }

        char m_pad0[0x10];
        StreamBundling m_oBundling; // 0x10
    };

    StreamSettings *GetStreamSettingsForContext(int);

    class ConnectionOrientedStream : public RootObject {
    public:
        virtual ~ConnectionOrientedStream();
    };

    class PRUDPStream : public ConnectionOrientedStream {
    public:
        PRUDPStream(Stream::Type, RootTransport *);
        virtual ~PRUDPStream();
        virtual void _v01();
        virtual void _v02();
        virtual void _v03();
        virtual void _v04();
        virtual void _v05();
        virtual void _v06();
        virtual void _v07();
        virtual void _v08();
        virtual void _v09();
        virtual void _v10();
        virtual void _v11();
        virtual void _v12();
        virtual void _v13();
        virtual void _v14();
        virtual void _v15();
        virtual void _v16();
        virtual void _v17();
        virtual void _v18();
        virtual void _v19();
        virtual qResult Initialize();
        virtual qResult Teardown();

        char m_pad[0x114];
    };

    class NetZ {
    public:
        RootTransport *GetTransport() { return m_pTransport; }

        char m_pad0[0x4c];
        RootTransport *m_pTransport; // 0x4c
    };

    void *GetInstanceType1Delegator();

    inline RootTransport *GetTransport() {
        NetZ *pNetZ = (NetZ *)GetInstanceType1Delegator();
        if (pNetZ == 0) {
            return 0;
        } else {
            return pNetZ->GetTransport();
        }
    }

    class EndPointEventHandler : public RootObject {
    public:
        virtual ~EndPointEventHandler();
    };

    class ObjDupProtocol : public EndPointEventHandler {
    public:
        ObjDupProtocol();
        virtual ~ObjDupProtocol();
        void Trace(unsigned int);

        char m_pad[0x4C];
    };

    class ConnectionManager : public RootObject {
    public:
        virtual ~ConnectionManager();
        void AttachStream(ConnectionOrientedStream *, bool);

        unsigned int m_uiContext; // 0x4
    };

    class StationConnectionManager : public ConnectionManager {
    public:
        StationConnectionManager(EndPointEventHandler *);
        void SetContext(unsigned int uiContext) { m_uiContext = uiContext; }

        char m_pad[0xBC];
    };

    class SessionDiscoveryTable : public RootObject {
    public:
        SessionDiscoveryTable();
        ~SessionDiscoveryTable();

        char m_pad[0x88];
    };

    class DOProtocolServer : public RootObject {
    public:
        DOProtocolServer();
        virtual ~DOProtocolServer();

        char m_pad[0x154];
    };

    class ProtocolRequestBroker : public RootObject {
    public:
        ProtocolRequestBroker(ProtocolRequestBroker *);
        virtual ~ProtocolRequestBroker();

        char m_pad[0x68];
    };

    class OperationManager : public RootObject {
    public:
        OperationManager();
        ~OperationManager();

        char m_pad[0x10];
    };

    class DOFilter : public RootObject {
    public:
        DOFilter();
        virtual ~DOFilter();
    };

    class UserDOFilter : public DOFilter {
    public:
        UserDOFilter() {}
        virtual ~UserDOFilter() {}

        char m_pad[0x4];
    };

    class DOSubset : public RootObject {
    public:
        DOSubset(const char *, DOFilter *);
        virtual ~DOSubset();

        char m_pad[0x44];
    };

    class DataSet : public RootObject {
    public:
        ~DataSet();
    };

    class StationIdentification : public DataSet {
    public:
        StationIdentification();

        String m_strIdentificationToken; // 0x0
        String m_strProcessName; // 0x4
        unsigned int m_uiProcessType; // 0x8
        unsigned int m_uiProductVersion; // 0xc
    };

    class Scheduler;

    // Retail calls GetInstance out of line (0x823EA910, an /O1 copy): /Ob1 does
    // not expand it inside Scheduler::GetInstance, but its three locals stay
    // reserved in the caller's frame.
    class Core {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.LookupInstance(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : 0;
            return pCore;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    class Scheduler {
    public:
        static CriticalSection s_csGlobalSystemLock;
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (!inst)
                return 0;
            else
                return inst->GetScheduler();
        }
        void Trace(unsigned int) {}
    };

    class CallRegister : public RootObject {
    public:
        static CallRegister *GetInstance();
        void Start();
        void CancelPeriodicJobs();
        void Trace(unsigned int);
    };

    class BadEvents {
    public:
        enum _ID {
        };
        static void Signal(_ID);
    };

    // Retail's table is 0x82CA4280, its strings 0x82181858..0x82181B80; the
    // dynamic initializer is 0x82C420A0 and the atexit destructor 0x82C4AEF0.
    char *g_szObjDupErrorDescriptions[] = {
        "Operation invalid on a duplica",
        "Operation invalid on a duplication master",
        "Invalid property transition",
        "Callback not defined",
        "Duplicated object not found",
        "Method called with an invalid parameter",
        "Duplicated object cannot emmigrate",
        "Invalid duplicated object construction",
        "Duplicated object already published",
        "Missing dataset callback",
        "Some well known handles are not initialized ",
        "Invalid well known handle creation",
        "Mismatched duplicated object types",
        "Well known handle already created",
        "Duplicated object (generic) cannot publish",
        "Station not found",
        "Cluster redefinition",
        "Cluster join denied",
        "Cluster join failed (server did not respond)",
        "Mismatched barrier ID",
        "Barrier timeout",
        "Cluster not initialized",
        "Not a cluster master",
        "Invalid state transition",
        "Cluster already initialized",
        "Transport not found"
    };

    unsigned int DOCore::s_uiDOCoreCount;
    ErrorDescriptionTable DOCore::s_oErrorTable(g_szObjDupErrorDescriptions, 3);

    DOCore::DOCore() : PseudoSingleton(4) {
        Network::AcquireInstance();
        Operation::s_pfTraceFilter = Operation::DefaultTraceFilter;
        {
            ScopedCS oCS(Scheduler::s_csGlobalSystemLock);
            s_uiDOCoreCount++;
            if (s_uiDOCoreCount == 1) {
                RegisterStatics();
            }
        }
        m_pSafetyExecutive = new (__FILE__, 0x55) SafetyExecutive();
        m_pBundlingPolicy = new (__FILE__, 0x56) BundlingPolicy();
        Station::SetLocalStation(DOHandle());
        m_pStationManager = new (__FILE__, 0x60) StationManager();
        m_pDOSelections = new (__FILE__, 0x61) DOSelections();
        m_pDOProtocol = new (__FILE__, 0x62) DOProtocol();
        m_pStream = new (__FILE__, 0x63) PRUDPStream((Stream::Type)1, GetTransport());
        GetStreamSettingsForContext(1)->EnableBundling(0);
        m_pStream->Initialize();
        m_pObjDupProtocol = new (__FILE__, 0x68) ObjDupProtocol();
        m_pStationConnectionManager =
            new (__FILE__, 0x69) StationConnectionManager(m_pObjDupProtocol);
        m_pStationConnectionManager->AttachStream(m_pStream, false);
        m_pSessionDiscoveryTable = new (__FILE__, 0x6D) SessionDiscoveryTable();
        m_pDOProtocolServer = new (__FILE__, 0x6E) DOProtocolServer();
        m_pProtocolRequestBroker = new (__FILE__, 0x6F) ProtocolRequestBroker(0);
        m_pStationConnectionManager->SetContext(PseudoSingleton::GetCurrentContext());
        m_pOperationManager = new (__FILE__, 0x73) OperationManager();
        m_pJoinProcessing = new (__FILE__, 0x74) ComponentState("Join processing");
        m_pFaultProcessing = new (__FILE__, 0x75) ComponentState("Fault processing");
        m_pDOCoreState = new (__FILE__, 0x76) ComponentState("DOCore state");
        m_pUserDOs = new (__FILE__, 0x77) DOSubset("UserDOs", new (__FILE__, 0x77) UserDOFilter());
        m_pStationIdentification = new (__FILE__, 0x78) StationIdentification();
    }

    DOCore::~DOCore() {
        if (IsCorrupted()) {
            m_pDOSelections->ClearOnCorruption();
            GetStationManager()->SetState(SystemComponent::Faulty, false);
        }
        delete m_pStationIdentification;
        if (!IsCorrupted()) {
            delete m_pJoinProcessing;
            delete m_pUserDOs;
        }
        delete m_pDOCoreState;
        delete m_pFaultProcessing;
        delete m_pDOProtocolServer;
        delete m_pSessionDiscoveryTable;
        delete m_pObjDupProtocol;
        delete m_pDOSelections;
        delete m_pStationManager;
        delete m_pStationConnectionManager;
        delete m_pOperationManager;
        delete m_pProtocolRequestBroker;
        delete m_pBundlingPolicy;
        delete m_pSafetyExecutive;
        m_pStream->Teardown();
        delete m_pStream;
        delete m_pDOProtocol;
        {
            ScopedCS oCS(Scheduler::s_csGlobalSystemLock);
            s_uiDOCoreCount--;
            if (s_uiDOCoreCount == 0) {
                UnregisterStatics();
            }
        }
        Network::ReleaseInstance();
    }

    void DOCore::TraceSystemState(unsigned int uiLevel) {
        if (DOSelections::GetInstance() != 0) {
            DOSelections::GetInstance()->Trace(uiLevel, 0);
        }
        if (Scheduler::GetInstance() != 0) {
            Scheduler::GetInstance()->Trace(uiLevel);
        }
        if (m_pObjDupProtocol != 0) {
            m_pObjDupProtocol->Trace(uiLevel);
        }
        CallRegister::GetInstance()->Trace(uiLevel);
    }

    void DOCore::QueuePeriodicJobs() { CallRegister::GetInstance()->Start(); }

    void DOCore::CancelPeriodicJobs() { CallRegister::GetInstance()->CancelPeriodicJobs(); }

    bool DOCore::IsReadyToLeave() {
        // Retail emits two `return true` paths, the first branching over the
        // second, which is what a constant-true test leaves at /Od.
        if (true) {
            return true;
        }
        return true;
    }

    void DOCore::SetToReadyState() { GetDOCoreState()->Initialize(); }

    void DOCore::SetToTerminatingState() {
        GetDOCoreState()->SetState(SystemComponent::Terminating, false);
    }

    void DOCore::SetToTerminatedState() {
        GetDOCoreState()->SetState(SystemComponent::Terminated, false);
    }

    void DOCore::SetToCorruptedState() {
        GetDOCoreState()->SetState(SystemComponent::Faulty, false);
        DOCore::GetInstance()->TraceSystemState(1);
        BadEvents::Signal((BadEvents::_ID)4);
    }

    bool DOCore::IsTerminating() const {
        return GetDOCoreState()->GetState() == SystemComponent::Terminating;
    }

    bool DOCore::IsTerminated() const {
        return GetDOCoreState()->GetState() == SystemComponent::Terminated;
    }

    bool DOCore::IsCorrupted() const {
        return GetDOCoreState()->GetState() == SystemComponent::Faulty;
    }

    bool DOCore::HasStartedTermination() const {
        return IsTerminating() || IsTerminated() || IsCorrupted();
    }
}
