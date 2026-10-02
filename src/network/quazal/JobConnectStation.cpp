// Quazal NetZ - .\JobConnectStation.cpp
//
// The retail TU is .text 0x82AB4CA0..0x82AB7EF0: JobConnectStation's methods,
// the anonymous ConnectCallback, and the qList<StepSequenceJob::Step> insert /
// erase / node-creation helpers it instantiates. It is built
// /Od /Oi- /EHs-c- /Ob1 /GR- (objects.json): no function carries EH state, and
// the vtable at 0x82181368 has no RTTI locator in front of it.
//
// The job tries a sequence of connection techniques (the step list built by the
// constructor) until one reaches the station, then creates or restores the
// Station duplica. Step names are the strings retail passes with each step.
//
// The surrounding NetZ classes are declared here only as far as this TU uses
// them; their members are defined in other TUs.

#include "Core/InstantiationContext.h"
#include "Platform/SystemError.h"
#include "Platform/qStd.h"

namespace Quazal {

    // GetInstanceFromVector is an inline candidate here: /Ob1 declines it at
    // every call site, which still reserves its this/ui/idx slots in the caller.
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
        static InstanceTable s_oInstanceTable;

        void *m_vtable; // 0x0
        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}

        unsigned int GetValue() const { return mValue; }
        unsigned int GetDOClassID() const { return (mValue & 0xFFC00000) >> 22; }
        bool operator==(const DOHandle &h) const { return mValue == h.mValue; }
        bool operator<(const DOHandle &h) const { return mValue < h.mValue; }

        unsigned int mValue; // 0x0
    };

    class DuplicatedObject;

    class DORef : public RootObject {
    public:
        DORef(DOHandle);
        ~DORef();
        void Acquire();

        unsigned int GetReferencedHandle() const { return m_hReferencedDO.mValue; }
        DOHandle GetHandle() const { return DOHandle(GetReferencedHandle()); }
        DuplicatedObject *GetDOPtr() const { return m_poReferencedDO; }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        ~DORefTemplate() {}

        // Retail calls the one out-of-line copy (DuplicatedObject's TU emits it
        // first); /Ob1 declines it here but still reserves its frame.
        bool IsValid() const {
            if (GetDOPtr() == NULL) {
                SystemError::SignalError(0, 0, 0xA0030004, 0);
                return false;
            } else {
                T *pDO = (T *)m_poReferencedDO;
                if (!T::GetDOClass(pDO->m_dohMyself.GetDOClassID())->IsAKindOf(T::GetStaticClassID())) {
                    SystemError::SignalError(0, 0, 0xE003000C, 0);
                    return false;
                }
                return true;
            }
        }
        T *operator->() const {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)GetDOPtr();
            }
        }
    };

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent;
    };

    class qResult {
    public:
        qResult();
        qResult(const int &);
        bool Equals(const bool &) const;
        qResult &operator=(const qResult &);

        unsigned int m_iReturnCode;
        const char *m_cszFilename;
        int m_iLineNumber;
    };

    class Time {
    public:
        Time(unsigned long long ullValue) : m_ullValue(ullValue) {}
        unsigned long long m_ullValue;
    };

    class Stream {
    public:
        enum Type {
            DO = 1
        };
    };

    class StationURL {
    public:
        ~StationURL();
        void SetStreamType(Stream::Type);
        void SetStreamID(unsigned char);
        void Trace(unsigned int) const;

        char m_data[0x64];
    };

    class UserContext : public RootObject {
    public:
        UserContext(void *pPointer) { m_pPointer = pPointer; }
        ~UserContext() {}
        void *GetPointer() const { return m_pPointer; }

        void *m_pPointer; // 0x0
    };

    class Buffer;
    class CallContext;
    class ObjDupProtocol {
    public:
        static ObjDupProtocol *GetInstance();
    };

    class EndPoint {
    public:
        virtual void _v00();
        virtual void _v01();
        virtual void _v02();
        virtual void _v03();
        virtual bool IsConnected();
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
        virtual qResult RegisterProtocol(ObjDupProtocol *);
        virtual void _v17();
        virtual void _v18();
        virtual void _v19();
        virtual qResult Connect(
            Buffer *, unsigned int, void (*)(EndPoint *, qResult, const UserContext *),
            const UserContext &, unsigned int
        );

        void Release();
        void SetStationHandle(unsigned int);
    };

    class Listener {
    public:
        virtual void _v00();
        virtual void _v01();
        virtual void _v02();
        virtual void _v03();
        virtual void _v04();
        virtual void _v05();
        virtual void _v06();
        virtual void _v07();
        virtual EndPoint *FindEndPoint(unsigned int);
    };

    class ByteStream : public RootObject {
    public:
        void Append(const void *, unsigned int, bool);
    };

    class Message : public ByteStream {
    public:
        Message();
        ~Message();
        Buffer *GetBuffer();

        char m_pad[0x40];
    };

    class SystemComponent {
    public:
        enum _State {
            TerminatingInUse = 0x10,
            Terminating = 0x20,
        };

        class Use : public RootObject {
        public:
            Use(SystemComponent *, const char *);
            ~Use();

            SystemComponent *m_pComponent; // 0x0
            const char *m_szName; // 0x4
            bool m_bInUse; // 0x8
        };

        _State GetState() const { return m_eState; }
        bool IsTerminating() const {
            return GetState() == TerminatingInUse || GetState() == Terminating;
        }

        char m_pad0[0xC];
        _State m_eState; // 0xc
    };

    // The type-4 component: the NetZ core object of the current context.
    class NetZ {
    public:
        static NetZ *GetInstance(unsigned int uiContext) {
            return (NetZ *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static NetZ *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        SystemComponent *GetSystemComponent() { return m_pSystemComponent; }
        Listener *GetListener() { return m_pListener; }

        char m_pad0[0x20];
        SystemComponent *m_pSystemComponent; // 0x20
        char m_pad24[0x1C];
        Listener *m_pListener; // 0x40
    };

    class Network {
    public:
        static Network *GetInstance();
        bool IsShuttingDown();
        void RegisterEndPoint(EndPoint *);
        bool ConnectToURLs(
            CallContext *, Buffer *, unsigned int, qList<StationURL> *, EndPoint **, unsigned int
        );
        bool RegistersEndPoints() const { return m_bRegistersEndPoints; }

        char m_pad0[0xBD];
        bool m_bRegistersEndPoints; // 0xbd
    };

    class ScopedCS {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), m_pCS(&cs) {
            CriticalSection *pCS = m_pCS;
            if (!MutexPrimitive::s_bNoOp) {
                pCS->EnterImpl();
            }
        }
        ~ScopedCS() { EndScope(); }
        void EndScope() {
            if (m_bInScope) {
                CriticalSection *pCS = m_pCS;
                if (!MutexPrimitive::s_bNoOp) {
                    pCS->LeaveImpl();
                }
                m_bInScope = false;
            }
        }

        bool m_bInScope; // 0x0
        CriticalSection *m_pCS; // 0x4
    };

    class Job;

    class Scheduler {
    public:
        void Queue(Job *, bool);

        char m_pad0[0x3C];
        CriticalSection m_csSystemLock; // 0x3C
    };

    // The type-3 component; it owns the scheduler.
    class Core {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : 0;
            return pCore;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    inline Scheduler *GetScheduler() {
        Core *inst = Core::GetInstance();
        if (!inst) {
            return 0;
        } else {
            return inst->GetScheduler();
        }
    }

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class CallContext : public RefCountedObject {
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
        virtual void _v3();
        virtual void _v4();
        virtual void _v5();
        virtual void _v6();
        virtual void _v7();
        virtual void Trace(unsigned int);

        void Reset();
        void SetFlag(unsigned int);
        void ClearFlag(unsigned int);
        _State GetState() const { return m_eState; }

        unsigned int m_uiFlags; // 0x8
        _State m_eState; // 0xc
        unsigned int m_unk10[0xE];
        Time m_tTimeout; // 0x48
    };

    class DOCallContext : public CallContext {
    public:
        DOCallContext(DOHandle, bool);
        virtual ~DOCallContext();
        bool Cancel(unsigned int);

        unsigned char unk50[0x50];
    };

    // Carries the result of Session::RetrieveURLs.
    class RetrieveURLsContext : public DOCallContext {
    public:
        RetrieveURLsContext(DOHandle, bool);
        virtual ~RetrieveURLsContext();

        unsigned char unkA0[0x20];
    };

    class DOOperation;
    class DOClass {
    public:
        virtual void _v00();
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
        virtual bool IsAKindOf(unsigned int) const;
    };

    class MasterStationRef {
    public:
        MasterStationRef(DOHandle, bool);
        ~MasterStationRef();

        char m_pad[0x10];
    };

    class DuplicatedObject : public RootObject {
    public:
        virtual void _v00();
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
        virtual void Trace(unsigned int) const;

        static DuplicatedObject *CreateDuplica(DOHandle, const MasterStationRef &);
        bool AddToStoreAsDuplica(DOHandle, Message *);
        bool DeleteDuplicaMainRef();
        bool UndeleteMainRef();
        DOHandle GetHandle() const;

        static DOClass *GetDOClass(unsigned int);
        bool FlagIsSet(unsigned short f) const { return (m_uiFlags & f) == f; }
        bool IsDeleted() const { return !FlagIsSet(1); }

        char m_pad4[0x1C];
        unsigned short m_uiFlags; // 0x20
        char m_pad22[0x26];
        DOHandle m_dohMyself; // 0x48
    };

    class Station : public DuplicatedObject {
    public:
        static DOHandle GetLocalStationHandle();
        bool IsAPeer();
        bool IsLocal();
        bool IsNotConnected();
        void ClearAtEOS();
        void SetConnection(EndPoint *);
        EndPoint *ReleaseConnection();
        EndPoint *GetEndPoint();
        bool Disconnect(bool);
        unsigned short GetState() const { return m_usState; }

        static unsigned int s_uiClassID;
        static unsigned int GetStaticClassID() { return s_uiClassID; }

        char m_pad4c[0x64];
        unsigned short m_usState; // 0xb0
    };

    class Session : public DuplicatedObject {
    public:
        static unsigned int s_uiClassID;
        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static DOHandle GetInstanceHandle();
        bool RetrieveURLs(RetrieveURLsContext *, const DOHandle &, qList<StationURL> *);
    };

    class CallRegister {
    public:
        static CallRegister *GetInstance();
        DOCallContext *FindCall(DOHandle, DOHandle);
    };

    class SelectionCursor : public RootObject {
    public:
        SelectionCursor(const SelectionCursor &);

        unsigned int m_uiValue; // 0x0
    };

    inline bool operator==(SelectionCursor a, unsigned int b) { return a.m_uiValue == b; }

    class SelectionPosition : public RootObject {
    public:
        bool EndReached() const { return m_oCursor == 0; }

        unsigned int unk0; // 0x0
        SelectionCursor m_oCursor; // 0x4
    };

    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(bool, bool);
        ~SelectionIterator();
        void Next(bool);
        bool EndReached() const { return m_oPosition.EndReached(); }

        void *m_pSelection; // 0x0
        SelectionPosition m_oPosition; // 0x4
        unsigned char unkC[0x18];
    };

    template <class T>
    class SelectionIteratorTemplate : public SelectionIterator {
    public:
        SelectionIteratorTemplate(bool b1, bool b2) : SelectionIterator(b1, b2) {
            SetFilter();
            GotoStart();
        }
        void SetFilter();
        void GotoStart();
        T *GetDOPtr();
        T *operator->() { return GetDOPtr(); }
    };

    class DebugString {
    public:
        DebugString() {}
    };

    class Job : public RefCountedObject {
    public:
        enum State {
            Initial = 0,
            Waiting = 1,
            Suspended = 2,
            Ready = 3,
            Running = 4,
            Complete = 5
        };
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute();
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual void GetTraceInfo();
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        void PerformExecution(const Time &);
        void SetToWaiting(int);
        void SetToSuspended();
        void SetToReady();
        void SetToRunning();
        void SetToComplete();
        State GetState() const { return m_eState; }

        unsigned int m_unk8[5];
        State m_eState; // 0x1c
        unsigned int m_unk20[2];
        Time m_tDeadline; // 0x28
        unsigned int m_unk30[2];
    };

    class JobDOOperation : public Job {
    public:
        JobDOOperation(DOOperation *);

        DOOperation *m_pOperation; // 0x38
    };

    class __multiple_inheritance StepSequenceJob;

    class StepSequenceJob : public Job {
    public:
        typedef void (StepSequenceJob::*JobStateFunc)(void);

        class Step : public RootObject {
        public:
            Step(JobStateFunc func, const char *name) : m_pfState(func), m_szName(name) {}
            ~Step() {}

            JobStateFunc m_pfState; // 0x0
            const char *m_szName; // 0x8
            unsigned int m_unkc;
        };

        StepSequenceJob(const DebugString &);
        virtual ~StepSequenceJob();
        virtual void CheckExceptions();

        void SetStep(const Step &);
        void ResumeOnCallCompletion(CallContext *, Step *);

        Time m_tStepStart; // 0x38
        unsigned int m_unk40[2];
        Step m_oCurrentStep; // 0x48
        unsigned int m_unk58; // 0x58
        unsigned int m_unk5c; // 0x5c
    };

    // A job that changes the connection state of one station; StationManager
    // tracks it until it completes.
    class JobChangeConnection : public StepSequenceJob {
    public:
        JobChangeConnection(const String &, DOHandle);
        virtual ~JobChangeConnection();
        virtual int GetTargetConnectionState() const = 0;
        virtual int GetType() const = 0;
        virtual void Trace(unsigned int);

        void Complete();
        bool IsCancelled() const { return m_bCancelled; }

        bool m_bCancelled; // 0x60
        DORefTemplate<Station> m_refStation; // 0x64
        bool m_bCompleted; // 0x70
    };

    class JobConnectStation : public JobChangeConnection {
    public:
        JobConnectStation(DOHandle);
        virtual ~JobConnectStation();
        virtual void SetDefaultPostExecutionState() {}
        virtual void CheckExceptions();
        virtual void TestSuspendedJobState();
        virtual int GetTargetConnectionState() const { return 1; }
        virtual int GetType() const { return 2; }
        virtual void Trace(unsigned int);

        EndPoint *FindIncomingEndPoint(DOHandle);
        void SetResultAndResume(qResult);
        void SelectConnectionTechnique();
        void TryConnectViaUndelete();
        void TryConnectViaInitialEndPoint();
        void TryConnectViaIncomingEndPoint();
        void TryConnectViaIncomingEndPointImpl();
        void ProcessIncomingConnectionResult();
        void TryConnectViaURLs();
        void RetrieveURLs();
        void WaitForURLs();
        void PrepareURLs();
        void DirectConnectViaURLs();
        void ProcessConnectionResult();
        void TryWaitingForIncomingEndPoint();
        void QueueOperation(DOOperation *);
        void QueueJob(Job *);
        void ExecuteQueuedJobs();
        void CancelQueuedJobs();
        void CompleteConnection();
        void ConnectOrphanStation();
        void ProcessConnectOrphanResult();
        void ConnectionCancelled();
        void ConnectionFailed();
        void ConnectionSucceeded() {}

        static bool s_bDisconnectOnError;
        static unsigned int s_uiConnectionTimeout;

        SystemComponent::Use m_oUse; // 0x78
        EndPoint *m_pEndPoint; // 0x84
        qResult m_rResult; // 0x88
        qList<Job *> m_lstQueuedJobs; // 0x94
        bool m_bCompleting; // 0x9c
        qList<Step> m_lstTechniques; // 0xa0
        RetrieveURLsContext m_oURLsContext; // 0xa8
        qList<StationURL> m_lstURLs; // 0x168
        unsigned int m_uiAttempts; // 0x170
        CallContext m_oCallContext; // 0x178
        bool m_bConnectingOrphan; // 0x1c8
    };

    class StationManager {
    public:
        static StationManager *GetInstance();
        bool StationIsDead(DOHandle);
        bool RetrieveStationURLs(DOHandle, qList<StationURL> *);
        void DisconnectStation(Station *);
        DOHandle GetInitialStation() const { return m_hInitialStation; }
        EndPoint *GetInitialEndPoint() const { return m_pInitialEndPoint; }

        char m_pad0[0x50];
        DOHandle m_hInitialStation; // 0x50
        EndPoint *m_pInitialEndPoint; // 0x54
    };

    inline DuplicatedObject *AcquireDO(DORef &ref) {
        if (ref.m_poReferencedDO == NULL) {
            ref.Acquire();
        }
        return ref.m_poReferencedDO;
    }

#define JCS_FILE ".\\JobConnectStation.cpp"
#define JCS_STEP(name) Step((JobStateFunc)&JobConnectStation::name, "JobConnectStation::" #name)

    JobConnectStation::JobConnectStation(DOHandle hStation)
        : JobChangeConnection(String("JobConnectStation"), hStation),
          m_oUse(NetZ::GetInstance()->GetSystemComponent(), NULL),
          m_oURLsContext(DOHandle(), true) {
        m_pEndPoint = NULL;
        m_rResult = qResult(0x10001);
        m_bCompleting = false;
        m_lstTechniques.push_back(JCS_STEP(TryConnectViaUndelete));
        m_lstTechniques.push_back(JCS_STEP(TryConnectViaInitialEndPoint));
        m_lstTechniques.push_back(JCS_STEP(TryConnectViaIncomingEndPoint));
        m_lstTechniques.push_back(JCS_STEP(TryConnectViaURLs));
        m_lstTechniques.push_back(JCS_STEP(TryWaitingForIncomingEndPoint));
        m_uiAttempts = 0;
        if (StationManager::GetInstance()->StationIsDead(hStation)) {
            SetStep(JCS_STEP(ConnectionFailed));
        } else {
            SetStep(JCS_STEP(SelectConnectionTechnique));
        }
        m_bConnectingOrphan = false;
    }

    JobConnectStation::~JobConnectStation() {}

    void JobConnectStation::CheckExceptions() {
        if (m_bCompleting) {
            return;
        }
        if (NetZ::GetInstance()->GetSystemComponent()->IsTerminating()) {
            SetStep(JCS_STEP(ConnectionCancelled));
        }
        if (IsCancelled()) {
            SetStep(JCS_STEP(ConnectionCancelled));
        }
    }

    void JobConnectStation::TestSuspendedJobState() {
        if (m_bConnectingOrphan) {
            if (m_refStation.IsValid() && m_refStation->IsDeleted()) {
                SetToReady();
                SetStep(JCS_STEP(ConnectionCancelled));
            } else if (NetZ::GetInstance()->GetSystemComponent()->IsTerminating()) {
                SetToReady();
                SetStep(JCS_STEP(ConnectionCancelled));
            } else if (IsCancelled()) {
                SetToReady();
                SetStep(JCS_STEP(ConnectionCancelled));
            }
        }
    }

    EndPoint *JobConnectStation::FindIncomingEndPoint(DOHandle hStation) {
        return NetZ::GetInstance()->GetListener()->FindEndPoint(hStation.GetValue());
    }

}

namespace {
    void ConnectCallback(Quazal::EndPoint *pEndPoint, Quazal::qResult r, const Quazal::UserContext *pContext) {
        using namespace Quazal;
        ScopedCS oCS(GetScheduler()->m_csSystemLock);
        JobConnectStation *pJob = (JobConnectStation *)pContext->GetPointer();
        if (pJob->GetState() == Job::Suspended) {
            pJob->SetResultAndResume(r);
        }
        pJob->ReleaseRef();
    }
}

namespace Quazal {

    void JobConnectStation::SetResultAndResume(qResult r) {
        m_rResult = r;
        SetToReady();
    }

    void JobConnectStation::SelectConnectionTechnique() {
        if (m_lstTechniques.empty()) {
            SetStep(JCS_STEP(ConnectionFailed));
        } else {
            SetStep(m_lstTechniques.front());
            m_lstTechniques.pop_front();
        }
    }

    void JobConnectStation::TryConnectViaUndelete() {
        AcquireDO(m_refStation);
        if (m_refStation.IsValid() && !m_refStation->IsLocal()) {
            if (!m_refStation->IsDeleted()) {
                m_refStation->Trace(1);
            }
            switch (m_refStation->GetState()) {
            case 3:
                if (m_refStation->IsAPeer()) {
                    SetStep(JCS_STEP(CompleteConnection));
                } else {
                    SetStep(JCS_STEP(SelectConnectionTechnique));
                }
                break;
            case 1:
                if (m_refStation->IsAPeer()) {
                    SetStep(JCS_STEP(CompleteConnection));
                } else {
                    SetStep(JCS_STEP(SelectConnectionTechnique));
                }
                break;
            case 4:
            case 5:
                SetStep(JCS_STEP(ConnectionFailed));
                break;
            case 2:
                break;
            case 0:
                SetStep(JCS_STEP(ConnectionFailed));
                break;
            default:
                SetStep(JCS_STEP(ConnectionFailed));
            }
        } else {
            SetStep(JCS_STEP(SelectConnectionTechnique));
        }
    }

    void JobConnectStation::TryConnectViaInitialEndPoint() {
        bool bUseInitial = StationManager::GetInstance()->GetInitialStation() == m_refStation.GetHandle()
            && StationManager::GetInstance()->GetInitialEndPoint() != NULL
            && StationManager::GetInstance()->GetInitialEndPoint()->IsConnected();
        if (bUseInitial) {
            m_pEndPoint = StationManager::GetInstance()->GetInitialEndPoint();
            if (!m_pEndPoint->IsConnected()) {
                SetStep(JCS_STEP(SelectConnectionTechnique));
            } else {
                SetStep(JCS_STEP(CompleteConnection));
            }
        } else {
            SetStep(JCS_STEP(SelectConnectionTechnique));
        }
    }

    void JobConnectStation::TryConnectViaIncomingEndPoint() {
        SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
        m_uiAttempts = 1;
    }

    void JobConnectStation::TryConnectViaIncomingEndPointImpl() {
        m_uiAttempts = m_uiAttempts - 1;
        if (Network::GetInstance()->IsShuttingDown()) {
            SetStep(JCS_STEP(ConnectionFailed));
            return;
        }
        EndPoint *pEndPoint = FindIncomingEndPoint(m_refStation.GetHandle());
        if (pEndPoint == NULL) {
            if (m_uiAttempts > 0) {
                SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
                SetToWaiting(100);
            } else {
                SetStep(JCS_STEP(SelectConnectionTechnique));
            }
            return;
        }
        m_pEndPoint = pEndPoint;
        Message oMsg;
        unsigned int uiLocal = Station::GetLocalStationHandle().mValue;
        oMsg.Append(&uiLocal, 4, true);
        unsigned int uiTarget = m_refStation.GetReferencedHandle();
        oMsg.Append(&uiTarget, 4, true);
        if (Network::GetInstance()->RegistersEndPoints()) {
            Network::GetInstance()->RegisterEndPoint(m_pEndPoint);
        }
        m_rResult = m_pEndPoint->Connect(
            oMsg.GetBuffer(), 0, ConnectCallback, UserContext(this), s_uiConnectionTimeout
        );
        if (m_rResult.Equals(false)) {
            m_pEndPoint->Release();
            m_pEndPoint = NULL;
            if (m_uiAttempts > 0) {
                SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
                SetToWaiting(100);
            } else {
                SetStep(JCS_STEP(SelectConnectionTechnique));
            }
            return;
        }
        AcquireRef();
        SetToSuspended();
        SetStep(JCS_STEP(ProcessIncomingConnectionResult));
    }

    void JobConnectStation::ProcessIncomingConnectionResult() {
        if (m_rResult.Equals(true)) {
            SetStep(JCS_STEP(CompleteConnection));
        } else {
            m_pEndPoint->Release();
            m_pEndPoint = NULL;
            if (m_uiAttempts > 0) {
                SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
                SetToWaiting(100);
            } else {
                SetStep(JCS_STEP(SelectConnectionTechnique));
            }
        }
    }

    void JobConnectStation::TryConnectViaURLs() {
        if (StationManager::GetInstance()->RetrieveStationURLs(m_refStation.GetHandle(), &m_lstURLs)) {
            SetStep(JCS_STEP(PrepareURLs));
        } else {
            SetStep(JCS_STEP(RetrieveURLs));
        }
    }

    void JobConnectStation::RetrieveURLs() {
        m_oURLsContext.ClearFlag(0x20);
        m_oURLsContext.SetFlag(0x80);
        m_oURLsContext.SetFlag(0x400);
        DORefTemplate<Session> refSession(Session::GetInstanceHandle());
        if (!refSession.IsValid()) {
            SetStep(JCS_STEP(SelectConnectionTechnique));
            return;
        }
        DOHandle hStation = m_refStation.GetReferencedHandle();
        bool bFailed = !refSession->RetrieveURLs(&m_oURLsContext, hStation, &m_lstURLs);
        if (bFailed) {
            SetStep(JCS_STEP(ConnectionFailed));
        } else {
            SetStep(JCS_STEP(WaitForURLs));
        }
    }

    void JobConnectStation::WaitForURLs() {
        if (m_oURLsContext.GetState() == CallContext::CallPending) {
            SetToWaiting(50);
            return;
        }
        m_oURLsContext.Trace(0x200000);
        for (qList<StationURL>::iterator it = m_lstURLs.begin(); it != m_lstURLs.end(); it++) {
            it->Trace(0x200000);
        }
        SetStep(JCS_STEP(PrepareURLs));
    }

    void JobConnectStation::PrepareURLs() {
        for (qList<StationURL>::iterator it = m_lstURLs.begin(); it != m_lstURLs.end(); it++) {
            StationURL &url = *it;
            url.SetStreamType(Stream::DO);
            url.SetStreamID(1);
        }
        SetStep(JCS_STEP(DirectConnectViaURLs));
    }

    void JobConnectStation::DirectConnectViaURLs() {
        m_oCallContext.Reset();
        Message oMsg;
        unsigned int uiLocal = Station::GetLocalStationHandle().mValue;
        oMsg.Append(&uiLocal, 4, true);
        unsigned int uiTarget = m_refStation.GetReferencedHandle();
        oMsg.Append(&uiTarget, 4, true);
        if (!Network::GetInstance()->ConnectToURLs(
                &m_oCallContext, oMsg.GetBuffer(), 0, &m_lstURLs, &m_pEndPoint,
                s_uiConnectionTimeout
            )) {
            SetStep(JCS_STEP(SelectConnectionTechnique));
            return;
        }
        if (m_oCallContext.GetState() == CallContext::CallPending) {
            SetToSuspended();
            ResumeOnCallCompletion(
                &m_oCallContext, new (JCS_FILE, 0x190) JCS_STEP(ProcessConnectionResult)
            );
        } else {
            SetStep(JCS_STEP(ProcessConnectionResult));
        }
    }

    void JobConnectStation::ProcessConnectionResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            SetStep(JCS_STEP(CompleteConnection));
        } else {
            SetStep(JCS_STEP(SelectConnectionTechnique));
        }
    }

    void JobConnectStation::TryWaitingForIncomingEndPoint() {
        m_uiAttempts = 10;
        SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
    }

    void JobConnectStation::QueueOperation(DOOperation *pOperation) {
        Job *pJob = new (JCS_FILE, 0x1AE) JobDOOperation(pOperation);
        pJob->SetToSuspended();
        QueueJob(pJob);
        GetScheduler()->Queue(pJob, false);
    }

    void JobConnectStation::QueueJob(Job *pJob) {
        pJob->AcquireRef();
        m_lstQueuedJobs.push_back(pJob);
    }

    void JobConnectStation::ExecuteQueuedJobs() {
        while (!m_lstQueuedJobs.empty()) {
            Job *pJob = m_lstQueuedJobs.front();
            m_lstQueuedJobs.pop_front();
            pJob->SetToReady();
            pJob->SetToRunning();
            pJob->PerformExecution(Time(0));
            pJob->ReleaseRef();
        }
    }

    void JobConnectStation::CancelQueuedJobs() {
        while (!m_lstQueuedJobs.empty()) {
            Job *pJob = m_lstQueuedJobs.front();
            m_lstQueuedJobs.pop_front();
            pJob->SetToComplete();
            pJob->ReleaseRef();
        }
    }

    void JobConnectStation::CompleteConnection() {
        m_bCompleting = true;
        if (AcquireDO(m_refStation) == NULL) {
            Station *pStation = (Station *)DuplicatedObject::CreateDuplica(
                m_refStation.GetHandle(), MasterStationRef(m_refStation.GetHandle(), true)
            );
            m_pEndPoint->SetStationHandle(m_refStation.GetReferencedHandle());
            pStation->SetConnection(m_pEndPoint);
            m_pEndPoint = NULL;
            if (!pStation->AddToStoreAsDuplica(Station::GetLocalStationHandle(), NULL)) {
                m_pEndPoint = pStation->ReleaseConnection();
                pStation->DeleteDuplicaMainRef();
                SetStep(JCS_STEP(ConnectionFailed));
                return;
            }
            pStation->GetEndPoint()->RegisterProtocol(ObjDupProtocol::GetInstance());
            AcquireDO(m_refStation);
        } else if (m_refStation->IsDeleted()) {
            if (m_pEndPoint != NULL) {
                m_pEndPoint->RegisterProtocol(ObjDupProtocol::GetInstance());
                m_pEndPoint->SetStationHandle(m_refStation.GetReferencedHandle());
                m_refStation->SetConnection(m_pEndPoint);
                m_pEndPoint = NULL;
            }
            m_refStation->UndeleteMainRef();
            m_refStation->ClearAtEOS();
        }
        Complete();
        ExecuteQueuedJobs();
        SetStep(JCS_STEP(ConnectOrphanStation));
    }

    void JobConnectStation::ConnectOrphanStation() {
        if (!m_refStation.IsValid()) {
            SetStep(JCS_STEP(ConnectionFailed));
            return;
        }
        DOCallContext *pContext = CallRegister::GetInstance()->FindCall(
            m_refStation->GetHandle(), m_refStation->GetHandle()
        );
        if (pContext != NULL) {
            m_bConnectingOrphan = true;
            m_bCompleting = false;
            SetToSuspended();
            ResumeOnCallCompletion(
                pContext, new (JCS_FILE, 0x218) JCS_STEP(ProcessConnectOrphanResult)
            );
        } else {
            SetStep(JCS_STEP(ProcessConnectOrphanResult));
        }
    }

    void JobConnectStation::ProcessConnectOrphanResult() {
        m_bConnectingOrphan = false;
        AcquireDO(m_refStation);
        if (m_refStation.IsValid() && !m_refStation->IsDeleted()) {
            SetStep(JCS_STEP(ConnectionSucceeded));
        } else {
            SetStep(JCS_STEP(ConnectionFailed));
        }
    }

    void JobConnectStation::ConnectionCancelled() {
        m_bConnectingOrphan = false;
        ConnectionFailed();
    }

    void JobConnectStation::ConnectionFailed() {
        DOCallContext *pContext = CallRegister::GetInstance()->FindCall(
            m_refStation.GetHandle(), m_refStation.GetHandle()
        );
        if (pContext != NULL) {
            pContext->Cancel(4);
        }
        CancelQueuedJobs();
        SetToComplete();
        AcquireDO(m_refStation);
        if (m_refStation.IsValid()) {
            StationManager::GetInstance()->DisconnectStation(m_refStation.operator->());
        }
        bool bDisconnectAll = s_bDisconnectOnError
            && Station::GetLocalStationHandle().mValue > m_refStation.GetReferencedHandle();
        if (bDisconnectAll) {
            SelectionIteratorTemplate<Station> it(true, true);
            while (!it.EndReached()) {
                if (it->IsNotConnected()) {
                    it->Disconnect(true);
                }
                it.Next(false);
            }
        }
    }

    void JobConnectStation::Trace(unsigned int uiFlags) {
        JobChangeConnection::Trace(uiFlags);
        if (m_refStation.operator->() != NULL) {
            m_refStation->Trace(uiFlags);
            m_refStation->GetEndPoint();
        }
    }

}
