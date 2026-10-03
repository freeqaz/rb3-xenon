// Quazal NetZ - .\Foundation\ProductFacade.cpp
//
// ProductFacade brings a NetZ product up and down: the constructor acquires
// the Core, the utility subsystem and the DOCore (all reference-counted across
// facades under s_csGlobalLock), registers the DDLs and system components and
// starts session discovery; Terminate queues a JobTerminateFacade; the
// destructor releases what the constructor acquired.
//
// The retail TU is 0x82A8FDF0..0x82A90C88, plus the dynamic initialiser and
// atexit destructor of s_csGlobalLock (0x82C417E8, 0x82C4A8F8). It starts at
// the constructor: its .rdata (the file string at 0x8217E874, then the vtable
// at 0x8217E894) follows MatchMakingClient's, and the functions before it are
// called from other TUs and never from this one. It ends at
// DeleteUtilitySubsystem; the code from 0x82A90C88 on is called only from
// other TUs. Its statics are the .bss run 0x82E1043C..0x82E1045C.
//
// Built /Od /Ob1 with EH and RTTI off (no EH records in the TU, and no RTTI
// locator before the vtable). The NetZ classes are declared here only as far
// as this TU uses them; their members are defined in other TUs. At /Od the
// local names set the stack layout.

#include "Core/InstantiationContext.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "Platform/Result.h"
#include "Platform/qStd.h"

namespace Quazal {

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent; // 0x0
    };

    // GetInstanceFromVector is a plain inline that /Ob1 declines: each call is
    // out of line (retail 0x823EA8A0), but the caller still reserves its
    // argument slots (the 12-byte holes before each DOCore::GetInstance's
    // context temp in Terminate(CallContext *)).
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

        InstantiationContext m_oDefaultContext; // 0x0
        qVector<InstantiationContext *> *m_pvContextVector; // 0x30
    };

    class InstanceControl : public RootObject {
    public:
        virtual ~InstanceControl();

        bool IsValid() const { return m_bIsValid; }

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

    class DOHandle : public RootObject {
    public:
        DOHandle() : mValue(0) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}

        unsigned int mValue; // 0x0
    };

    class Scheduler;
    class Job;
    class SystemComponents;

    class Core : public RootObject {
    public:
        static void AcquireInstance();
        static void ReleaseInstance();

        // Called out of line (retail 0x823EA910); its three locals stay reserved
        // in the caller. Of the spellings tried, only this one gives both the
        // constructor's and the destructor's frames.
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (inst)
                pCore = (Core *)inst->m_pDelegatorInstance;
            return pCore;
        }

        Scheduler *GetScheduler() { return m_pScheduler; }
        SystemComponents *GetSystemComponents() { return m_pSystemComponents; }

        char m_pad0[0x8];
        Scheduler *m_pScheduler; // 0x8
        void *m_pCallContextRegister; // 0xc
        SystemComponents *m_pSystemComponents; // 0x10
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
        };

        virtual ~SystemComponent();
        _State Initialize();

        char m_pad4[0x14];
    };

    class SystemComponentGroup : public SystemComponent {
    public:
        SystemComponentGroup(const String &);
        bool RegisterComponent(SystemComponent *);
        bool UnregisterComponent(SystemComponent *);

        char m_pad18[0x8];
    };

    class SystemComponents : public SystemComponentGroup {
    public:
        // Expanded in place at every use. /Ob1 declines the ternary spelling of
        // the same test, which leaves an out-of-line call instead.
        static SystemComponents *GetInstance() {
            if (Core::GetInstance() == 0)
                return 0;
            return Core::GetInstance()->GetSystemComponents();
        }

        // Defined in ../Core/SystemComponents.h, at lines 47 and 48.
        void CreateSessionGroup() {
            m_pSessionGroup = new ("../Core/SystemComponents.h", 47) SystemComponentGroup("Session");
            RegisterComponent(m_pSessionGroup);
        }
        void CreateDOCoreGroup() {
            m_pDOCoreGroup = new ("../Core/SystemComponents.h", 48) SystemComponentGroup("DOCore");
            RegisterComponent(m_pDOCoreGroup);
        }

        SystemComponentGroup *GetSessionGroup() { return m_pSessionGroup; }
        SystemComponentGroup *GetDOCoreGroup() { return m_pDOCoreGroup; }

        int m_unk20; // 0x20
        SystemComponentGroup *m_pSessionGroup; // 0x24
        SystemComponentGroup *m_pDOCoreGroup; // 0x28
    };

    class Scheduler : public RootObject {
    public:
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (!inst)
                return 0;
            else
                return inst->GetScheduler();
        }
        void Queue(Job *, bool);

        char m_pad0[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
    };

    class StationConnectionManager {
    public:
        void DenyIncomingConnectionsFromNewStation();
    };

    class DOCore : public PseudoSingleton {
    public:
        DOCore();
        virtual ~DOCore();

        static DOCore *GetInstance(unsigned int uiContext) {
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static DOCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }

        void QueuePeriodicJobs();
        void CancelPeriodicJobs();
        StationConnectionManager *GetStationConnectionManager() { return m_pStationConnectionManager; }

        char m_pad14[0x8];
        StationConnectionManager *m_pStationConnectionManager; // 0x1c
        char m_pad20[0x38];
    };

    class CallContext : public RootObject {
    public:
        enum _State {
            CallInit = 0,
            CallPending = 1,
            CallSuccess = 2,
            CallError = 3,
            CallCancelled = 4,
        };
        CallContext();
        virtual ~CallContext();

        bool Wait(unsigned int);
        bool InitiateCall();
        void SetStateImpl(_State, qResult, bool);
        // Inline: retail builds the qResult and its int temporary among the
        // inline-expansion temps of Terminate(CallContext *). The name is ours.
        void SignalSuccess() { SetStateImpl(CallSuccess, qResult(0x10001), true); }
        _State GetState() const { return m_eState; }
        unsigned int GetID() const { return m_uiID; }

        unsigned int m_uiRefCount; // 0x4
        unsigned int m_uiFlags; // 0x8
        _State m_eState; // 0xc
        char m_pad10[0x24];
        unsigned int m_uiID; // 0x34
        char m_pad38[0x18];
    };

    class ProductSpecifics : public RootObject {
    public:
        virtual ~ProductSpecifics();
        virtual unsigned int GetProductID() = 0;
        virtual void RegisterSpecificDDLs() {}
        virtual void RegisterSpecificComponents() {}
    };

    class ProductInfo {
    public:
        static ProductInfo *FindAnyInstance();
    };

    class SessionDiscoveryProtocol : public RootObject {
    public:
        virtual ~SessionDiscoveryProtocol();
    };

    class LANSessionDiscovery : public SessionDiscoveryProtocol {
    public:
        LANSessionDiscovery();
        virtual ~LANSessionDiscovery();

        char m_pad4[0x1c];
    };

    class Session {
    public:
        static void InitStaticSessionDescription(ProductInfo *);
        static void RegisterSessionDiscovery(SessionDiscoveryProtocol *, bool);
        static void UnregisterSessionDiscovery(SessionDiscoveryProtocol *);
    };

    class Network {
    public:
        static void AcquireInstance();
        static void ReleaseInstance();
    };

    class Station {
    public:
        static void SetLocalStation(DOHandle);
    };

    class BundlingPolicy {
    public:
        static BundlingPolicy *GetInstance();
        void Disable();
    };

    class DDLDeclarations {
    public:
        static void LoadAll();
    };

    class DOCoreDDLDeclarations {
    public:
        static void Register();
    };

    class Job : public RootObject {
    public:
        virtual ~Job();
    };

    class ProductFacade;

    class JobTerminateFacade : public Job {
    public:
        JobTerminateFacade(ProductFacade *, unsigned int);

        char m_pad4[0x8c];
    };

    class UtilitySubsystem : public RootObject {
    public:
        UtilitySubsystem();
        ~UtilitySubsystem();
    };

    class OutputFormat {
    public:
        void ShowLocalStationHandle(bool);

        static unsigned int (*s_pfStationHandleResolver)();
        static const char *(*s_pfThreadNameResolver)();
        static unsigned int (*s_pfCurrentContextResolver)();
    };

    class TraceLog {
    public:
        static TraceLog *GetInstance();
        OutputFormat *GetOutputFormat();
    };

    unsigned int GetLocalStationHandle();
    // 0x82AAC608 is ObjectThreadRoot::GetCurrentThreadName (the name ObjectThreadRoot.cpp maps).
    class ObjectThreadRoot {
    public:
        static const char *GetCurrentThreadName();
    };
    void InitDOClasses();
    // 0x82A8F3D0 (declared the same way in NetSession_Xbox.cpp).
    bool FlushPendingCalls();

    class ProductFacade : public PseudoSingleton {
    public:
        ProductFacade(ProductSpecifics *);
        virtual ~ProductFacade();

        // Terminate() (0x82A90718), CreateUtilitySubsystem (0x82A90B60) and
        // DeleteUtilitySubsystem (0x82A90C08) are our names for these bodies.
        bool Terminate();
        bool Terminate(CallContext *);
        static bool DecrementDOCoreRefCount();

        bool IsInitialized() const { return m_bInitialized; }

        void CreateUtilitySubsystem();
        void DeleteUtilitySubsystem();

        static UtilitySubsystem *s_poUtilitySubsystem;
        static unsigned int s_uiSubsystemRefCount;
        static unsigned int s_uiDOCoreRefCount;
        static CriticalSection s_csGlobalLock;

        ProductSpecifics *m_pProductSpecifics; // 0x14
        bool m_bInitialized; // 0x18
        bool m_bTerminated; // 0x19
        SessionDiscoveryProtocol *m_pSessionDiscovery; // 0x1c
    };

    UtilitySubsystem *ProductFacade::s_poUtilitySubsystem;
    unsigned int ProductFacade::s_uiSubsystemRefCount;
    unsigned int ProductFacade::s_uiDOCoreRefCount;
    CriticalSection ProductFacade::s_csGlobalLock(0);

    ProductFacade::ProductFacade(ProductSpecifics *pSpecifics) : PseudoSingleton(0) {
        Core::AcquireInstance();
        m_bTerminated = false;
        m_pSessionDiscovery = 0;
        m_pProductSpecifics = pSpecifics;
        m_bInitialized = true;
        if (!IsValid()) {
            m_bInitialized = false;
            return;
        }
        {
            ScopedCS oCS(s_csGlobalLock);
            if (++s_uiSubsystemRefCount == 1)
                CreateUtilitySubsystem();
        }
        {
            ScopedCS oCS(s_csGlobalLock);
            if (++s_uiDOCoreRefCount == 1)
                InitDOClasses();
            pSpecifics->RegisterSpecificDDLs();
            DOCoreDDLDeclarations::Register();
            DDLDeclarations::LoadAll();
        }
        Network::AcquireInstance();
        SystemComponents::GetInstance()->CreateDOCoreGroup();
        SystemComponents::GetInstance()->CreateSessionGroup();
        // Retail stores the new DOCore to the local and then stores it again.
        DOCore *pDOCore = new (__FILE__, 0x90) DOCore();
        pDOCore = pDOCore;
        pSpecifics->RegisterSpecificComponents();
        DOCore::GetInstance()->QueuePeriodicJobs();
        SystemComponents::GetInstance()->GetDOCoreGroup()->Initialize();
        Session::InitStaticSessionDescription(ProductInfo::FindAnyInstance());
        m_pSessionDiscovery = new (__FILE__, 0xad) LANSessionDiscovery();
        Session::RegisterSessionDiscovery(m_pSessionDiscovery, false);
    }

    ProductFacade::~ProductFacade() {
        if (!m_bTerminated)
            Terminate();
        if (IsInitialized()) {
            Network::ReleaseInstance();
            ScopedCS oCS(s_csGlobalLock);
            if (--s_uiSubsystemRefCount == 0)
                DeleteUtilitySubsystem();
        }
        FlushPendingCalls();
        delete m_pProductSpecifics;
        Core::ReleaseInstance();
        if (IsInitialized())
            Station::SetLocalStation(DOHandle());
        if (IsInitialized() && Core::GetInstance()) {
            SystemComponents::GetInstance()->UnregisterComponent(SystemComponents::GetInstance()->GetDOCoreGroup());
            SystemComponents::GetInstance()->UnregisterComponent(SystemComponents::GetInstance()->GetSessionGroup());
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            SystemComponents::GetInstance()->Initialize();
        }
    }

    bool ProductFacade::Terminate() {
        CallContext oContext;
        Terminate(&oContext);
        oContext.Wait(-1);
        return oContext.GetState() == CallContext::CallSuccess;
    }

    bool ProductFacade::Terminate(CallContext *pContext) {
        if (m_bTerminated)
            return false;
        {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            if (!pContext->InitiateCall())
                return false;
        }
        m_bTerminated = true;
        if (!IsInitialized()) {
            pContext->SignalSuccess();
            return true;
        }
        DOCore::GetInstance()->GetStationConnectionManager()->DenyIncomingConnectionsFromNewStation();
        if (m_pSessionDiscovery != 0) {
            Session::UnregisterSessionDiscovery(m_pSessionDiscovery);
            delete m_pSessionDiscovery;
        }
        BundlingPolicy::GetInstance()->Disable();
        DOCore::GetInstance()->CancelPeriodicJobs();
        JobTerminateFacade *pJob = new (__FILE__, 0x133) JobTerminateFacade(this, pContext->GetID());
        Scheduler::GetInstance()->Queue(pJob, false);
        return true;
    }

    bool ProductFacade::DecrementDOCoreRefCount() {
        ScopedCS oCS(s_csGlobalLock);
        return --s_uiDOCoreRefCount == 0;
    }

    void ProductFacade::CreateUtilitySubsystem() {
        s_poUtilitySubsystem = new (__FILE__, 0x145) UtilitySubsystem();
        OutputFormat::s_pfStationHandleResolver = GetLocalStationHandle;
        OutputFormat::s_pfThreadNameResolver = ObjectThreadRoot::GetCurrentThreadName;
        OutputFormat::s_pfCurrentContextResolver = PseudoSingleton::GetCurrentContext;
        TraceLog::GetInstance()->GetOutputFormat()->ShowLocalStationHandle(true);
    }

    void ProductFacade::DeleteUtilitySubsystem() {
        delete s_poUtilitySubsystem;
        s_poUtilitySubsystem = 0;
    }
}
