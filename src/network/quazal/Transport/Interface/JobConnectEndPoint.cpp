// Quazal NetZ - .\Transport\Interface\JobConnectEndPoint.cpp
// Retail TU: .text 0x82B2C698..0x82B2FC38, built /Od /Oi- /Ob1 /GR-
// (objects.json; the vtables carry no RTTI locator).
//
// The job connects an EndPoint to the first URL of a list that answers. It is
// a StepSequenceJob: the constructor queues the connection techniques (direct,
// then via routing) and every step sets the next one by name. Step and method
// names are the ones retail passes with each step; the helpers retail factors
// out of the steps (MustAbort, ResumeWithResult, SetCallContextState,
// CanRouteTo, UpdateCurrentURL) are named here.
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Platform/qStd.h"
#include "Platform/Result.h"
#include "Platform/ScopedCS.h"

#define JCEP_FILE ".\\Transport\\Interface\\JobConnectEndPoint.cpp"

namespace Quazal {

    class DebugString {
    public:
        DebugString() {}
    };

    class Time : public RootObject {
    public:
        Time &operator=(const Time &);
        static unsigned int ConvertDeadlineToTimeout(Time);
        static Time ConvertTimeoutToDeadline(unsigned int);

        unsigned long long m_ui64Value;
    };

    class UserContext {
    public:
        UserContext(void *p) : m_pPointer(p) {}
        ~UserContext() {}

        void *m_pPointer;
    };

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class Buffer : public RefCountedObject {
    public:
        virtual ~Buffer();
    };

    class InetAddress : public RootObject {
    public:
        InetAddress();
        ~InetAddress();

        char m_data[0x80];
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        StationURL(const StationURL &);
        ~StationURL();
        InetAddress *GetInetAddress() const;
        unsigned short GetPortNumber() const;
        void SetPortNumber(unsigned short);
        unsigned int GetConnectionID() const;
        unsigned int GetRVConnectionID() const;
        unsigned int GetType() const;

        char m_data[0x64];
    };

    class CallbackRoot : public RootObject {
    public:
        CallbackRoot() {}
        virtual ~CallbackRoot() {}
        virtual void Call();
        virtual void CallObjectMethod() = 0;
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
        void Reset();
        void SetStateImpl(_State, qResult, bool);
        void RegisterCancellationCallback(CallbackRoot *);

        _State GetState() const { return m_eState; }
        void SetTimeout(Time t) { m_tTimeout = t; }

        unsigned int m_unk8; // 0x8
        _State m_eState; // 0xc
        unsigned int m_unk10[0xe];
        Time m_tTimeout; // 0x48
    };

    class CallContextRegister {
    public:
        CallContext *GetCallContext(unsigned int);
    };

    class PseudoSingleton : public RootObject {
    public:
        static unsigned int GetCurrentContext();
    };

    class Scheduler;

    class InstanceTable : public RootObject {
    public:
        unsigned int GetInstanceFromVector(unsigned int, unsigned int);
    };

    class InstanceControl : public RootObject {
    public:
        static InstanceTable s_oInstanceTable;

        char m_data[0xc];
        void *m_pDelegatorInstance; // 0xc
    };

    // Retail calls GetInstance out of line (0x823EA910): the /Ob1 inliner
    // declines it after reserving its locals.
    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance = (InstanceControl *)
                InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (pInstance != 0) {
                pCore = (Core *)pInstance->m_pDelegatorInstance;
            }
            return pCore;
        }
        Scheduler *GetScheduler() const { return m_pScheduler; }
        CallContextRegister *GetCallContextRegister() const { return m_pCallContextRegister; }

        char m_data[0x8];
        Scheduler *m_pScheduler; // 0x8
        CallContextRegister *m_pCallContextRegister; // 0xc
    };

    class Scheduler : public RootObject {
    public:
        static Scheduler *GetInstance();

        char m_data[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
    };

    inline Scheduler *Scheduler::GetInstance() {
        Core *pCore = Core::GetInstance();
        if (pCore == 0) {
            return 0;
        } else {
            return pCore->GetScheduler();
        }
    }

    class EndPoint;
    typedef void (*pfCompletion)(EndPoint *, qResult, const UserContext *);

    class EndPoint : public RootObject {
    public:
        virtual bool IsNotConnected() = 0;
        virtual bool IsConnecting() = 0;
        virtual bool IsDisconnecting() = 0;
        virtual bool IsFaulty() = 0;
        virtual bool IsConnected() = 0;
        virtual bool PeerIsConnected() = 0;
        virtual bool PeerIsDisconnected() = 0;
        virtual void Unk7();
        virtual void SetKeepAliveTimeout(unsigned int) = 0;
        virtual void Unk9();
        virtual unsigned int GetKeepAliveTimeout() = 0;
        virtual unsigned int GetMaxSilenceTime() = 0;
        virtual void SetPeerConnected() = 0;
        virtual void SetPeerDisconnected() = 0;
        virtual int GetConnectionState() = 0;
        virtual bool SetConnectionState(int) = 0;
        virtual void Unk16();
        virtual void SignalEvent(unsigned int);
        virtual unsigned int GetRTT() = 0;
        virtual unsigned int GetRTTAverage() = 0;
        virtual qResult
        _Connect(Buffer *, Buffer *, pfCompletion, const UserContext &, unsigned int) = 0;
        virtual qResult _Disconnect(pfCompletion, const UserContext &, unsigned int) = 0;

        qResult Connect(
            Buffer *pConnectData, Buffer *pConnectResponse, pfCompletion pfCallback,
            const UserContext &oContext, unsigned int uiTimeout
        ) {
            return _Connect(pConnectData, pConnectResponse, pfCallback, oContext, uiTimeout);
        }
        qResult
        Disconnect(pfCompletion pfCallback, const UserContext &oContext, unsigned int uiTimeout) {
            return _Disconnect(pfCallback, oContext, uiTimeout);
        }
        void Close();
        void SetConnectionID(unsigned int);
    };

    class SystemComponent : public RootObject {
    public:
        class Use {
        public:
            Use(SystemComponent *, const char *);
            ~Use();

            char m_data[0xc];
        };
    };

    class ConnectionManager : public RootObject {
    public:
        EndPoint *OpenEndPoint(const StationURL &);
        void ConfigureEndPointForRouting(EndPoint *);
        bool IsTerminating() const;
        bool IsDirectConnectionEnabled() const { return m_bDirectConnection; }
        bool IsRoutingEnabled() const { return m_bRouting; }

        char m_data[0x8];
        SystemComponent m_oComponent; // 0x8
        char m_data9[0xbc - 0x9];
        bool m_bDirectConnection; // 0xbc
        bool m_bRouting; // 0xbd
    };

    class RoutingTable : public RootObject {
    public:
        bool Find(const InetAddress &, InetAddress &) const;
    };

    class RoutingSubsystem : public RootObject {
    public:
        char m_data[0x8];
        RoutingTable m_oTable; // 0x8
    };

    class Router : public RootObject {
    public:
        char m_data[0x38];
        RoutingSubsystem m_oRouting; // 0x38
    };

    class TransportAdapter : public RootObject {
    public:
        virtual ~TransportAdapter();
        virtual void Unk1();
        virtual void ResolveURL(CallContext *, StationURL *);
    };

    class NATTraversalEngine : public RootObject {
    public:
        virtual ~NATTraversalEngine();
        virtual void Unk1();
        virtual void PrepareTraversal(const StationURL &);
        bool GetUpdatedURL(const StationURL &, StationURL *);
    };

    class Network : public RootObject {
    public:
        NATTraversalEngine *GetNATTraversalEngine();
        void SortURLs(qList<StationURL> &);
        static TransportAdapter *s_pTransportAdapter;
    };

    class RoutingTransport : public RootObject {
    public:
        virtual ~RoutingTransport();
        virtual void Unk1();
        virtual void Unk2();
        virtual void Unk3();
        virtual void Unk4();
        virtual void Unk5();
        virtual void Unk6();
        virtual void Unk7();
        virtual void Unk8();
        virtual Router *GetRouter();
    };

    class NetZ : public RootObject {
    public:
        char m_data[0x4c];
        RoutingTransport *m_pTransport; // 0x4c
    };

    void *GetInstanceType1Delegator();

    inline RoutingTransport *GetTransport() {
        NetZ *pNetZ = (NetZ *)GetInstanceType1Delegator();
        if (pNetZ == 0) {
            return 0;
        } else {
            return pNetZ->m_pTransport;
        }
    }

    class Job : public RefCountedObject {
    public:
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute();
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual void GetTraceInfo();
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        enum State {
            Initial = 0,
            Waiting = 1,
            Suspended = 2,
            Ready = 3,
            Running = 4,
            Complete = 5
        };

        void SetToWaiting(int);
        void SetToSuspended();
        void SetToReady();
        void SetToComplete();
        State GetState() const { return m_eState; }

        unsigned int m_unk8[5];
        State m_eState; // 0x1c
        unsigned int m_unk20[14];
        unsigned int m_uiTraceFlags; // 0x58
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

        unsigned int m_unk5c;
    };

    class JobConnectEndPoint;

    class ConnectCancelCallback : public CallbackRoot {
    public:
        ConnectCancelCallback(JobConnectEndPoint *);
        virtual ~ConnectCancelCallback();
        virtual void CallObjectMethod();

        JobConnectEndPoint *m_pJob; // 0x4
    };

    class JobConnectEndPoint : public StepSequenceJob {
    public:
        JobConnectEndPoint(
            ConnectionManager *, unsigned int, Buffer *, Buffer *, const qList<StationURL> &,
            EndPoint **, Time
        );
        virtual ~JobConnectEndPoint();
        virtual void TestSuspendedJobState();
        virtual void CheckExceptions();
        virtual void Trace(unsigned int);

        void OnCancellation();
        bool MustAbort();
        void ResumeWithResult(qResult);
        void SortURLs();
        void SelectConnectionTechnique();
        void TryDirectConnect();
        void TryConnectViaRouting();
        bool CanRouteTo(const StationURL &);
        void TestCurrentURL();
        void PrepareNATTraversal();
        void ResolveCurrentURL();
        void WaitForURLResolution();
        void TryCurrentURL();
        void ProcessConnectionResult();
        void ProcessConnectionFailure();
        bool UpdateCurrentURL();
        void CheckForUpdatedURL();
        void SetCallContextState(CallContext::_State, qResult);
        void ConnectionFailed();
        void ConnectionCancelled();
        void ConnectionSucceeded();

        static bool s_bForceNATTraversal;

        SystemComponent::Use m_oUse; // 0x60
        EndPoint *m_pEndPoint; // 0x6c
        qResult m_rResult; // 0x70
        ConnectionManager *m_pConnectionManager; // 0x7c
        Time m_tTimeout; // 0x80
        unsigned int m_uiAttemptTimeout; // 0x88
        ConnectCancelCallback *m_pCancelCallback; // 0x8c
        qList<Step> m_lstTechniques; // 0x90
        unsigned int m_uiUnk98; // 0x98
        CallContext m_oConnectContext; // 0xa0
        CallContext m_oResolveContext; // 0xf0
        unsigned int m_uiCallID; // 0x140
        Buffer *m_pConnectData; // 0x144
        Buffer *m_pConnectResponse; // 0x148
        EndPoint **m_ppEndPoint; // 0x14c
        qList<StationURL> m_lstURLs; // 0x150
        bool m_bViaRouting; // 0x158
        qList<StationURL>::iterator m_itCurrentURL; // 0x15c
    };

    bool JobConnectEndPoint::s_bForceNATTraversal;

    ConnectCancelCallback::ConnectCancelCallback(JobConnectEndPoint *pJob) : m_pJob(pJob) {}

    ConnectCancelCallback::~ConnectCancelCallback() { m_pJob = 0; }

    void ConnectCancelCallback::CallObjectMethod() { m_pJob->OnCancellation(); }

    inline void TraceURLs(const qList<StationURL> &) {}

#line 82
    JobConnectEndPoint::JobConnectEndPoint(
        ConnectionManager *pConnectionManager, unsigned int uiCallID, Buffer *pConnectData,
        Buffer *pConnectResponse, const qList<StationURL> &lstURLs, EndPoint **ppEndPoint,
        Time tTimeout
    )
        : StepSequenceJob(DebugString()), m_oUse(&pConnectionManager->m_oComponent, "JobConnectEndPoint"),
          m_pEndPoint(0), m_pConnectionManager(pConnectionManager), m_tTimeout(tTimeout),
          m_uiUnk98(0), m_uiCallID(uiCallID), m_pConnectData(pConnectData),
          m_pConnectResponse(pConnectResponse), m_ppEndPoint(ppEndPoint), m_lstURLs(lstURLs),
          m_bViaRouting(false), m_itCurrentURL(m_lstURLs.end()) {
        m_uiTraceFlags = 0x4000;
        if (m_pConnectData != 0) {
            m_pConnectData->AcquireRef();
        }
        if (m_pConnectResponse != 0) {
            m_pConnectResponse->AcquireRef();
        }
        m_rResult = qResult(0x10001);
        if (m_pConnectionManager->IsDirectConnectionEnabled()) {
            m_lstTechniques.push_back(Step(
                (JobStateFunc)&JobConnectEndPoint::TryDirectConnect,
                "JobConnectEndPoint::TryDirectConnect"
            ));
        }
        if (m_pConnectionManager->IsRoutingEnabled()) {
            m_lstTechniques.push_back(Step(
                (JobStateFunc)&JobConnectEndPoint::TryConnectViaRouting,
                "JobConnectEndPoint::TryConnectViaRouting"
            ));
        }
        TraceURLs(m_lstURLs);
        SetStep(Step((JobStateFunc)&JobConnectEndPoint::SortURLs, "JobConnectEndPoint::SortURLs"));
        if (!m_lstURLs.empty() && !m_lstTechniques.empty()) {
            m_uiAttemptTimeout = Time::ConvertDeadlineToTimeout(m_tTimeout)
                / (m_lstURLs.size() * m_lstTechniques.size());
        } else {
            m_uiAttemptTimeout = 0;
        }
        CallContext *pContext =
            Core::GetInstance()->GetCallContextRegister()->GetCallContext(m_uiCallID);
        m_pCancelCallback = new (JCEP_FILE, __LINE__) ConnectCancelCallback(this);
        pContext->RegisterCancellationCallback(m_pCancelCallback);
    }

    JobConnectEndPoint::~JobConnectEndPoint() {
        if (m_pConnectData != 0) {
            m_pConnectData->ReleaseRef();
        }
        if (m_pConnectResponse != 0) {
            m_pConnectResponse->ReleaseRef();
        }
    }

    void JobConnectEndPoint::OnCancellation() {
        m_pEndPoint = 0;
        m_pCancelCallback->m_pJob = 0;
        SetToComplete();
    }

    bool JobConnectEndPoint::MustAbort() {
        if (Core::GetInstance()->GetCallContextRegister()->GetCallContext(m_uiCallID) == 0) {
            return true;
        } else if (m_pConnectionManager->IsTerminating()) {
            return true;
        }
        return false;
    }

    void JobConnectEndPoint::TestSuspendedJobState() {
        if (MustAbort()) {
            ResumeWithResult(qResult(0x80010004));
        }
    }

    void JobConnectEndPoint::CheckExceptions() {
        if (MustAbort()) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ConnectionCancelled,
                "JobConnectEndPoint::ConnectionCancelled"
            ));
        }
    }

    namespace {
        void ConnectCallback(EndPoint *pEndPoint, qResult r, const UserContext *pContext) {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            JobConnectEndPoint *pJob = (JobConnectEndPoint *)pContext->m_pPointer;
            if (pJob->GetState() == Job::Suspended) {
                pJob->ResumeWithResult(r);
            }
        }

        void DisconnectCallback(EndPoint *pEndPoint, qResult r, const UserContext *pContext) {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            JobConnectEndPoint *pJob = (JobConnectEndPoint *)pContext->m_pPointer;
            pJob->SetToReady();
            pJob->ReleaseRef();
        }
    }

    void JobConnectEndPoint::ResumeWithResult(qResult r) {
        Trace(m_uiTraceFlags);
        m_rResult = r;
        SetToReady();
        ReleaseRef();
    }

    void JobConnectEndPoint::SortURLs() {
        ((Network *)GetInstanceType1Delegator())->SortURLs(m_lstURLs);
        SetStep(Step(
            (JobStateFunc)&JobConnectEndPoint::SelectConnectionTechnique,
            "JobConnectEndPoint::SelectConnectionTechnique"
        ));
    }

    void JobConnectEndPoint::SelectConnectionTechnique() {
        if (m_lstTechniques.empty()) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ConnectionFailed,
                "JobConnectEndPoint::ConnectionFailed"
            ));
        } else {
            SetStep(*m_lstTechniques.begin());
            m_lstTechniques.pop_front();
        }
    }

    void JobConnectEndPoint::TryDirectConnect() {
        m_itCurrentURL = m_lstURLs.begin();
        m_bViaRouting = false;
        if (m_itCurrentURL == m_lstURLs.end()) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::SelectConnectionTechnique,
                "JobConnectEndPoint::SelectConnectionTechnique"
            ));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
                "JobConnectEndPoint::TestCurrentURL"
            ));
        }
    }

    void JobConnectEndPoint::TryConnectViaRouting() {
        m_itCurrentURL = m_lstURLs.begin();
        m_bViaRouting = true;
        SetStep(Step(
            (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
            "JobConnectEndPoint::TestCurrentURL"
        ));
    }

    bool JobConnectEndPoint::CanRouteTo(const StationURL &url) {
        InetAddress oAddress;
        bool bResult =
            GetTransport()->GetRouter()->m_oRouting.m_oTable.Find(*url.GetInetAddress(), oAddress);
        return bResult;
    }

    void JobConnectEndPoint::TestCurrentURL() {
        if (m_itCurrentURL == m_lstURLs.end()) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::SelectConnectionTechnique,
                "JobConnectEndPoint::SelectConnectionTechnique"
            ));
        } else if (((Network *)GetInstanceType1Delegator())->GetNATTraversalEngine() == 0
                   || CanRouteTo(*m_itCurrentURL)) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ResolveCurrentURL,
                "JobConnectEndPoint::ResolveCurrentURL"
            ));
        } else if (s_bForceNATTraversal && (*m_itCurrentURL).GetRVConnectionID() != 0) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::PrepareNATTraversal,
                "JobConnectEndPoint::PrepareNATTraversal"
            ));
        } else if (!(((*m_itCurrentURL).GetType() & 1) == 1)) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ResolveCurrentURL,
                "JobConnectEndPoint::ResolveCurrentURL"
            ));
        } else if (!(((*m_itCurrentURL).GetType() & 2) == 2)) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ResolveCurrentURL,
                "JobConnectEndPoint::ResolveCurrentURL"
            ));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::PrepareNATTraversal,
                "JobConnectEndPoint::PrepareNATTraversal"
            ));
        }
    }

    void JobConnectEndPoint::PrepareNATTraversal() {
        NATTraversalEngine *pEngine =
            ((Network *)GetInstanceType1Delegator())->GetNATTraversalEngine();
        pEngine->PrepareTraversal(*m_itCurrentURL);
        SetStep(Step(
            (JobStateFunc)&JobConnectEndPoint::ResolveCurrentURL,
            "JobConnectEndPoint::ResolveCurrentURL"
        ));
    }

    void JobConnectEndPoint::ResolveCurrentURL() {
        m_oResolveContext.SetTimeout(Time::ConvertTimeoutToDeadline(m_uiAttemptTimeout));
        UpdateCurrentURL();
        Network::s_pTransportAdapter->ResolveURL(&m_oResolveContext, &*m_itCurrentURL);
        SetStep(Step(
            (JobStateFunc)&JobConnectEndPoint::WaitForURLResolution,
            "JobConnectEndPoint::WaitForURLResolution"
        ));
    }

    void JobConnectEndPoint::WaitForURLResolution() {
        if (m_oResolveContext.GetState() != CallContext::CallPending) {
            if (m_oResolveContext.GetState() == CallContext::CallSuccess) {
                SetStep(Step(
                    (JobStateFunc)&JobConnectEndPoint::TryCurrentURL,
                    "JobConnectEndPoint::TryCurrentURL"
                ));
            } else {
                ++m_itCurrentURL;
                SetStep(Step(
                    (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
                    "JobConnectEndPoint::TestCurrentURL"
                ));
            }
            m_oResolveContext.Reset();
        } else {
            SetToWaiting(100);
        }
    }

    void JobConnectEndPoint::TryCurrentURL() {
        EndPoint *pEndPoint = m_pConnectionManager->OpenEndPoint(*m_itCurrentURL);
        if (pEndPoint == 0) {
            ++m_itCurrentURL;
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
                "JobConnectEndPoint::TestCurrentURL"
            ));
        } else {
            m_pEndPoint = pEndPoint;
            if (m_bViaRouting) {
                m_pConnectionManager->ConfigureEndPointForRouting(m_pEndPoint);
            }
            m_rResult = m_pEndPoint->Connect(
                m_pConnectData, m_pConnectResponse, ConnectCallback, UserContext(this),
                m_uiAttemptTimeout
            );
            if (m_rResult.Equals(false)) {
                m_pEndPoint->Close();
                m_pEndPoint = 0;
                ++m_itCurrentURL;
                SetStep(Step(
                    (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
                    "JobConnectEndPoint::TestCurrentURL"
                ));
            } else {
                AcquireRef();
                SetToSuspended();
                SetStep(Step(
                    (JobStateFunc)&JobConnectEndPoint::ProcessConnectionResult,
                    "JobConnectEndPoint::ProcessConnectionResult"
                ));
            }
        }
    }

    void JobConnectEndPoint::ProcessConnectionResult() {
        if (m_rResult.Equals(true)) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ConnectionSucceeded,
                "JobConnectEndPoint::ConnectionSucceeded"
            ));
        } else {
            if (m_pEndPoint->IsConnected()) {
                bool bResult = m_pEndPoint->Disconnect(
                    DisconnectCallback, UserContext(this), m_uiAttemptTimeout
                );
                if (bResult) {
                    AcquireRef();
                    SetToSuspended();
                } else {
                    SetToComplete();
                }
            }
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::ProcessConnectionFailure,
                "JobConnectEndPoint::ProcessConnectionFailure"
            ));
        }
    }

    void JobConnectEndPoint::ProcessConnectionFailure() {
        m_pEndPoint->Close();
        m_pEndPoint = 0;
        if (((Network *)GetInstanceType1Delegator())->GetNATTraversalEngine() != 0) {
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::CheckForUpdatedURL,
                "JobConnectEndPoint::CheckForUpdatedURL"
            ));
        } else {
            ++m_itCurrentURL;
            SetStep(Step(
                (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
                "JobConnectEndPoint::TestCurrentURL"
            ));
        }
    }

    bool JobConnectEndPoint::UpdateCurrentURL() {
        if (((Network *)GetInstanceType1Delegator())->GetNATTraversalEngine() != 0) {
            StationURL urlUpdated;
            if (((Network *)GetInstanceType1Delegator())
                    ->GetNATTraversalEngine()
                    ->GetUpdatedURL(*m_itCurrentURL, &urlUpdated)
                && (*m_itCurrentURL).GetPortNumber() != urlUpdated.GetPortNumber()) {
                (*m_itCurrentURL).SetPortNumber(urlUpdated.GetPortNumber());
                return true;
            }
        }
        return false;
    }

    void JobConnectEndPoint::CheckForUpdatedURL() {
        StationURL urlUpdated;
        if (!UpdateCurrentURL()) {
            ++m_itCurrentURL;
        }
        SetStep(Step(
            (JobStateFunc)&JobConnectEndPoint::TestCurrentURL,
            "JobConnectEndPoint::TestCurrentURL"
        ));
    }

    void JobConnectEndPoint::SetCallContextState(CallContext::_State eState, qResult r) {
        CallContext *pContext =
            Core::GetInstance()->GetCallContextRegister()->GetCallContext(m_uiCallID);
        if (pContext != 0) {
            pContext->SetStateImpl(eState, r, true);
        }
    }

    void JobConnectEndPoint::ConnectionFailed() {
        SetCallContextState(CallContext::CallError, qResult(0x80050002));
        SetToComplete();
    }

    void JobConnectEndPoint::ConnectionCancelled() {
        SetCallContextState(CallContext::CallCancelled, qResult(0x80010004));
        SetToComplete();
    }

    void JobConnectEndPoint::ConnectionSucceeded() {
        if ((*m_itCurrentURL).GetConnectionID() != 0) {
            m_pEndPoint->SetConnectionID((*m_itCurrentURL).GetConnectionID());
        }
        *m_ppEndPoint = m_pEndPoint;
        m_pEndPoint = 0;
        SetCallContextState(CallContext::CallSuccess, qResult(0x10001));
        SetToComplete();
    }

    void JobConnectEndPoint::Trace(unsigned int) {
        for (qList<StationURL>::iterator it = m_lstURLs.begin(); it != m_lstURLs.end(); ++it) {
        }
    }

}
