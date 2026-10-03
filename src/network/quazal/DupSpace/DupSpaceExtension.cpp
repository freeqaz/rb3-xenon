// Quazal NetZ - .\DupSpace\DupSpaceExtension.cpp
//
// The retail TU is 0x82B26B20..0x82B270E8: the DupSpaceExtension constructor,
// the GetType/IsAKindOf/deleting-destructor virtuals its vtable needs, the
// destructor, DupSpaceOperationCallback's constructor, deleting destructor and
// CallMethod, then BeginInitialization, BeginTermination and Register in source
// order, and last the MethodCallJob<DuplicationSpaceTable, int, PeriodicJob>
// GetTraceInfo and Execute that BeginTermination's job needs (that job's
// deleting destructor is folded into CallRegister's identical copy, and so is
// its vtable). 0x82B270E8 is DuplicationSpace's constructor, the next TU.
//
// Built /Od /Oi- /EHs-c- /Ob1 /GR-: no EH prefixes or funclets, and the vtable
// at 0x8218B438 carries no locator slot. The classes it uses are declared here
// only as far as this TU uses them; their members are defined in other TUs.

#include "Platform/RootObject.h"

#define DUPSPACEEXTENSION_FILE ".\\DupSpace\\DupSpaceExtension.cpp"

namespace Quazal {

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent; // 0x0
    };

    class DebugString {
    public:
        DebugString() {}
    };

    class StringStream : public RootObject {
    public:
        StringStream();
        ~StringStream();

        StringStream &operator<<(const char *);
        StringStream &operator<<(int);

        const char *m_szBuffer; // 0x0
        unsigned int m_uiSize; // 0x4
        const char *m_szCurrentPosition; // 0x8
        char m_szInitialBuffer[256]; // 0xc
        bool m_bHex; // 0x10c
        bool m_bShowBase; // 0x10d
        bool m_bBoolAlpha; // 0x10e
    };

    StringStream &operator<<(StringStream &, const String &);

    class InstanceControl : public RootObject {
    public:
        virtual ~InstanceControl();

        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
        };

        static const char *type() { return "SystemComponent"; }

        SystemComponent(const String &);
        virtual ~SystemComponent();
        virtual void *AcquireRef();
        virtual void ReleaseRef();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const { return type() == str; }
        virtual void EnforceDeclareSysComponentMacro() = 0;
        virtual void TraceImpl(unsigned int) const;
        virtual _State StateTransition(_State);
        virtual void OnInitialize();
        virtual void OnTerminate();
        virtual bool BeginInitialization();
        virtual bool EndInitialization();
        virtual bool BeginTermination();
        virtual bool EndTermination();
        virtual bool ValidTransition(_State);
        virtual bool UseIsAllowed();
        virtual _State TestState();
        virtual void DoWork();

        unsigned short m_ui16RefCount; // 0x4
        String mName; // 0x8
        _State mState; // 0xc
        unsigned int mRefs; // 0x10
        SystemComponent *mParent; // 0x14
    };

    class SystemComponentGroup : public SystemComponent {
    public:
        bool RegisterComponent(SystemComponent *);
    };

    class SystemComponents : public SystemComponentGroup {
    public:
        SystemComponentGroup *GetExtensions() { return m_pExtensions; }

        char m_pad18[0x10];
        SystemComponentGroup *m_pExtensions; // 0x28
    };

    class Scheduler;
    class Job;

    // Retail calls GetInstance out of line (0x823EA910, an /O1 copy).
    class Core : public RootObject {
    public:
        static Core *GetInstance();
        Scheduler *GetScheduler() { return m_pScheduler; }
        SystemComponents *GetSystemComponents() { return m_pSystemComponents; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
        char m_padc[4];
        SystemComponents *m_pSystemComponents; // 0x10
    };

    class Scheduler {
    public:
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (inst == 0) {
                return 0;
            } else {
                return inst->GetScheduler();
            }
        }
        void Queue(Job *, bool);
    };

    inline SystemComponents *GetSystemComponents() {
        if (Core::GetInstance() == 0) {
            return 0;
        } else {
            return Core::GetInstance()->GetSystemComponents();
        }
    }

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void *AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class Job : public RefCountedObject {
    public:
        Job(const DebugString &);
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute() = 0;
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual String GetTraceInfo() const;
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        char m_pad8[0x28];
        bool m_bDeleteAfterExecution; // 0x30
        char m_pad31[7];
    };

    class PeriodicJob : public Job {
    public:
        PeriodicJob(const DebugString &);
        virtual ~PeriodicJob() {}
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        int m_tiPeriod; // 0x38
        char m_pad3c[4];
    };

    template <class T1, class T2, class T3>
    class MethodCallJob : public T3 {
    public:
        typedef void (T1::*JobFunc)(T2);

        MethodCallJob(const String &strName, T1 *pTarget, JobFunc pMethod, T2 arg)
            : T3(DebugString()) {
            m_pTargetObject = pTarget;
            m_pMethod = pMethod;
            m_arg = arg;
        }
        virtual ~MethodCallJob() {}
        virtual void Execute() {
            T1 *target = m_pTargetObject;
            JobFunc func = m_pMethod;
            (target->*func)(m_arg);
        }
        virtual String GetTraceInfo() const {
            StringStream ss;
            ss << T3::GetTraceInfo();
            ss << "\tArgument: " << m_arg;
            return String(ss.m_szBuffer);
        }

        JobFunc m_pMethod; // 0x40
        T1 *m_pTargetObject; // 0x48
        T2 m_arg; // 0x4c
    };

    class Operation;
    class DOOperation;

    class OperationCallback : public RootObject {
    public:
        OperationCallback() : m_uiPriority(500) {}
        virtual ~OperationCallback() {}
        virtual void CallMethod(Operation *) = 0;

        unsigned int m_uiPriority; // 0x4
    };

    class OperationManager : public RootObject {
    public:
        void RegisterCallback(OperationCallback *);
    };

    class DuplicatedObject : public RootObject {
    public:
        static OperationManager *GetOperationManager();
    };

    class DuplicationSpaceTable : public RootObject {
    public:
        DuplicationSpaceTable();
        ~DuplicationSpaceTable();

        static DuplicationSpaceTable *GetInstance();
        void RegisterDuplicationSpaces();
        void UnregisterDuplicationSpaces(int);
        void StartPeriodicMatch();
        void StopPeriodicMatch();
        void OperationEndMatchTrigger(DOOperation *);

        char m_pad0[0x38];
    };

    class DuplicationSpace : public RootObject {
    public:
        static void ResetIDGenerator();

        static bool s_bUseSessionSpace;
    };

    class SessionSpace : public RootObject {
    public:
        static void InitializeSpecialRelations();
    };

    class DupSpaceExtension : public SystemComponent {
    public:
        class DupSpaceOperationCallback : public OperationCallback {
        public:
            DupSpaceOperationCallback();
            virtual void CallMethod(Operation *);
        };

        static const char *type() { return "DupSpaceExtension"; }

        DupSpaceExtension();
        virtual ~DupSpaceExtension();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const {
            return type() == str || SystemComponent::IsAKindOf(str);
        }
        virtual void EnforceDeclareSysComponentMacro();
        virtual bool BeginInitialization();
        virtual bool BeginTermination();

        static bool Register();

        DupSpaceOperationCallback m_oCallback; // 0x18
        DuplicationSpaceTable m_oDupSpaceTable; // 0x20
    };

    DupSpaceExtension::DupSpaceExtension() : SystemComponent(String("DupSpace extension")) {}

    DupSpaceExtension::~DupSpaceExtension() {}

    DupSpaceExtension::DupSpaceOperationCallback::DupSpaceOperationCallback() {}

    void DupSpaceExtension::DupSpaceOperationCallback::CallMethod(Operation *pOperation) {
        DuplicationSpaceTable::GetInstance()->OperationEndMatchTrigger((DOOperation *)pOperation);
    }

    bool DupSpaceExtension::BeginInitialization() {
        DuplicatedObject::GetOperationManager()->RegisterCallback(&m_oCallback);
        DuplicationSpaceTable::GetInstance()->RegisterDuplicationSpaces();
        DuplicationSpaceTable::GetInstance()->StartPeriodicMatch();
        if (DuplicationSpace::s_bUseSessionSpace) {
            SessionSpace::InitializeSpecialRelations();
        }
        return true;
    }

    bool DupSpaceExtension::BeginTermination() {
        DuplicationSpaceTable::GetInstance()->StopPeriodicMatch();
        MethodCallJob<DuplicationSpaceTable, int, PeriodicJob> *pJob =
            new (DUPSPACEEXTENSION_FILE, 0x31) MethodCallJob<DuplicationSpaceTable, int, PeriodicJob>(
                String("UnregisterDuplicationSpaces"),
                DuplicationSpaceTable::GetInstance(),
                &DuplicationSpaceTable::UnregisterDuplicationSpaces,
                0
            );
        pJob->m_bDeleteAfterExecution = true;
        Scheduler::GetInstance()->Queue(pJob, false);
        return true;
    }

    bool DupSpaceExtension::Register() {
        DuplicationSpace::ResetIDGenerator();
        DupSpaceExtension *pExtension = new (DUPSPACEEXTENSION_FILE, 0x42) DupSpaceExtension();
        GetSystemComponents()->GetExtensions()->RegisterComponent(pExtension);
        return true;
    }

}
