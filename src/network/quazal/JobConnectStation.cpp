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
// /Od frames: an inline the compiler declines still reserves its frame in the
// caller, and inline parameters get stack homes, so several helpers below are
// spelled the way retail's frames require (see the notes at each).

#include "Core/CallContext.h"
#include "Core/Core.h"
#include "Core/NetZ.h"
#include "Core/SystemComponent.h"
#include "ObjDup/CallRegister.h"
#include "ObjDup/DOCallContext.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/DuplicatedObject.h"
#include "ObjDup/MasterStationRef.h"
#include "ObjDup/ObjDupProtocol.h"
#include "ObjDup/SelectionIterator.h"
#include "ObjDup/Session.h"
#include "ObjDup/Station.h"
#include "ObjDup/StationManager.h"
#include "Platform/LogicalClock.h"
#include "Platform/Result.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"
#include "Platform/Time.h"
#include "Platform/UserContext.h"
#include "Plugins/EndPoint.h"
#include "Plugins/Message.h"
#include "Plugins/StationURL.h"
#include "Platform/qStd.h"

namespace Quazal {

    class DOOperation;

    // TU-local declarations of classes this TU only calls into.

    // The incoming-connection listener (NetZ + 0x40).
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

    // NetZ + 0x1c; the static accessor is out of line (retail 0x82AD1BC8).
    class ConnectionManager {
    public:
        static ConnectionManager *GetInstance();
        bool IsShuttingDown();
        void RegisterEndPoint(EndPoint *);
        bool ConnectToURLs(
            CallContext *, Buffer *, unsigned int, qList<StationURL> *, EndPoint **, unsigned int
        );
        // Inline wrapper: callers evaluate the timeout and buffer into its
        // parameter homes and fetch the instance before the call.
        static bool Connect(
            CallContext *pContext, Buffer *pBuffer, unsigned int uiFlags,
            qList<StationURL> *pURLs, EndPoint **ppEndPoint, unsigned int uiTimeout
        ) {
            ConnectionManager *pManager = GetInstance();
            return pManager->ConnectToURLs(
                pContext, pBuffer, uiFlags, pURLs, ppEndPoint, uiTimeout
            );
        }
        bool RegistersEndPoints() const { return m_bRegistersEndPoints; }

        char m_pad0[0xBD];
        bool m_bRegistersEndPoints; // 0xbd
    };

    class Job;

    // Core/Scheduler.h pulls in Core/Job.h, whose layout is not retail's (see
    // Job below), so the scheduler is declared here as far as this TU uses it.
    class Scheduler {
    public:
        void Queue(Job *, bool);

        char m_pad0[0x3C];
        CriticalSection m_csSystemLock; // 0x3C
    };

    inline Scheduler *GetScheduler() {
        Core *inst = Core::GetInstance();
        if (!inst) {
            return 0;
        } else {
            return inst->GetScheduler();
        }
    }

    // The DO call context Session::RetrieveURLs completes (0xC0 bytes; its
    // constructor is retail 0x82A9CB78).
    class RetrieveURLsContext : public DOCallContext {
    public:
        RetrieveURLsContext(DOHandle, bool);
        virtual ~RetrieveURLsContext();
        virtual void _v6();
        virtual void _v7();
        virtual void Trace(unsigned int);

        unsigned char unkA8[0x18];
    };

    // StepSequenceJob is declared locally, as in JobBackEndServicesLogin and
    // ObjDupProtocol: retail keeps the current Step (a pointer to member plus
    // name, 0x10 bytes) at 0x48, which Core/StepSequenceJob.h does not match.
    // The job QueueOperation schedules for one DOOperation (0x40 bytes).
    class JobDOOperation : public Job {
    public:
        JobDOOperation(DOOperation *);
        virtual void Execute();

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
        virtual void Execute();
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
    // tracks it until it completes (its destructor hands the job to
    // StationManager unless m_bCompleted is set). Its members are in the
    // StepSequenceJob TU (constructor 0x82AF9538, destructor 0x82AF9638).
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
        static unsigned int GetConnectionTimeout() { return s_uiConnectionTimeout; }

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

    // Declared as far as this TU calls it; its TU is StationManager.cpp.

#define JCS_FILE ".\\JobConnectStation.cpp"
#define JCS_STEP(name) Step((JobStateFunc)&JobConnectStation::name, "JobConnectStation::" #name)

    JobConnectStation::JobConnectStation(DOHandle hStation)
        : JobChangeConnection("JobConnectStation", hStation),
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
        if (StationManager::GetInstance()->StationIsDead(m_refStation.m_hReferencedDO)) {
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
        m_refStation.GetPtr();
        if (m_refStation.IsValid() && !m_refStation->IsFaulty()) {
            if (!m_refStation->IsDeleted()) {
                m_refStation->Trace(1);
            }
            switch (m_refStation->GetState()) {
            case 3:
                if (m_refStation->IsConnected()) {
                    SetStep(JCS_STEP(CompleteConnection));
                } else {
                    SetStep(JCS_STEP(SelectConnectionTechnique));
                }
                break;
            case 1:
                if (m_refStation->IsConnected()) {
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
                return;
            }
            SetStep(JCS_STEP(CompleteConnection));
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
        if (ConnectionManager::GetInstance()->IsShuttingDown()) {
            SetStep(JCS_STEP(ConnectionFailed));
            return;
        }
        EndPoint *pIncomingEndPoint = FindIncomingEndPoint(m_refStation.m_hReferencedDO);
        if (pIncomingEndPoint == NULL) {
            if (m_uiAttempts > 0) {
                SetStep(JCS_STEP(TryConnectViaIncomingEndPointImpl));
                SetToWaiting(100);
            } else {
                SetStep(JCS_STEP(SelectConnectionTechnique));
            }
            return;
        }
        m_pEndPoint = pIncomingEndPoint;
        Message oMsg;
        oMsg << Station::GetLocalStation().GetValue();
        oMsg << m_refStation.GetHandle().GetValue();
        if (ConnectionManager::GetInstance()->RegistersEndPoints()) {
            ConnectionManager::GetInstance()->RegisterEndPoint(m_pEndPoint);
        }
        m_rResult = m_pEndPoint->Connect(
            oMsg.GetBuffer(), 0, ConnectCallback, UserContext(this), GetConnectionTimeout()
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
        } else {
            AcquireRef();
            SetToSuspended();
            SetStep(JCS_STEP(ProcessIncomingConnectionResult));
        }
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
        if (StationManager::GetInstance()->RetrieveStationURLs(m_refStation.m_hReferencedDO, &m_lstURLs)) {
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
        if (refSession->RetrieveURLs(&m_oURLsContext, m_refStation.GetHandle(), &m_lstURLs)
            == false) {
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
        qList<StationURL>::iterator it = m_lstURLs.begin();
        while (it != m_lstURLs.end()) {
            it->Trace(0x200000);
            ++it;
        }
        SetStep(JCS_STEP(PrepareURLs));
    }

    void JobConnectStation::PrepareURLs() {
        qList<StationURL>::iterator it = m_lstURLs.begin();
        while (it != m_lstURLs.end()) {
            StationURL &url = *it;
            url.SetStreamType((Stream::Type)1);
            url.SetStreamID(1);
            ++it;
        }
        SetStep(JCS_STEP(DirectConnectViaURLs));
    }

    void JobConnectStation::DirectConnectViaURLs() {
        m_oCallContext.Reset();
        Message oMsg;
        oMsg << Station::GetLocalStation().GetValue();
        oMsg << m_refStation.GetHandle().GetValue();
        if (!ConnectionManager::Connect(
                &m_oCallContext, oMsg.GetBuffer(), 0, &m_lstURLs, &m_pEndPoint,
                GetConnectionTimeout()
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
        if (!m_refStation.IsAcquired()) {
            Station *pStation = (Station *)DuplicatedObject::CreateDuplica(
                m_refStation.m_hReferencedDO, MasterStationRef(m_refStation.m_hReferencedDO, 1)
            );
            m_pEndPoint->SetStationHandle(m_refStation.GetReferencedHandle());
            pStation->SetConnection(m_pEndPoint);
            m_pEndPoint = NULL;
            if (!pStation->AddToStoreAsDuplica(Station::GetLocalStation(), NULL)) {
                m_pEndPoint = pStation->ReleaseConnection();
                pStation->DeleteDuplicaMainRef();
                SetStep(JCS_STEP(ConnectionFailed));
                return;
            }
            pStation->GetEndPoint()->RegisterProtocol(ObjDupProtocol::GetInstance());
            m_refStation.GetPtr();
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
        m_refStation.GetPtr();
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
            m_refStation.m_hReferencedDO, m_refStation.m_hReferencedDO
        );
        if (pContext != NULL) {
            pContext->Cancel(4);
        }
        CancelQueuedJobs();
        SetToComplete();
        m_refStation.GetPtr();
        if (m_refStation.IsValid()) {
            StationManager::GetInstance()->DisconnectStation(m_refStation.operator->());
        }
        if (!!(s_bDisconnectOnError
               && Station::GetLocalStation().GetValue() > m_refStation.GetHandle().GetValue())) {
            SelectionIteratorTemplate<Station> it(true, true);
            while (!it.EndReached()) {
                if (it->IsAPeer()) {
                    it->SignalFault(true);
                }
                it.Next(false);
            }
        }
    }

    void JobConnectStation::Trace(unsigned int uiFlags) {
        JobChangeConnection::Trace(uiFlags);
        if (m_refStation.operator->() == NULL) {
            return;
        }
        m_refStation->Trace(uiFlags);
        m_refStation->GetEndPoint();
    }

}
