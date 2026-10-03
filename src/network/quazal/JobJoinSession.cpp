// Quazal NetZ - .\JobJoinSession.cpp
//
// Retail TU: .text 0x82AC8658..0x82ACAB10, built /Od /Oi- /EHs-c- /Ob1 /GR-
// (objects.json). It holds the JoinCancelCallback the job registers on the
// caller's call context, the JobJoinSession step sequence, and at its end the
// container helpers it instantiates first (list<StationURL>::push_front and
// insert, and the participant list's clear/destroy).
//
// The job is a StepSequenceJob: every step either sets the next step directly
// or issues an asynchronous call and resumes on its completion. Step names are
// the strings retail passes with each step ("JobJoinSession::InitiateConnection",
// ...). PrepareURL, Cancel and SignalCallContext have no retail name; theirs are
// descriptive.
//
// Shared Quazal classes come from their headers. StepSequenceJob is declared
// here with the layout retail's code reads (Core/StepSequenceJob.h's does not
// match it), as are the classes no header declares yet. At /Od the local
// NAMES set the stack layout (a walk over the scope's symbol hash buckets),
// and an inline function the compiler declines to expand still reserves its
// slots in the caller, so both are chosen to reproduce retail's frames.

#include "Platform/qStd.h"
#include "Platform/ScopedCS.h"
#include "Platform/Callback.h"
#include "Platform/RefCountedObject.h"
#include "Platform/Result.h"
#include "Platform/Time.h"
#include "Plugins/StationURL.h"
#include "Plugins/Message.h"
#include "Plugins/EndPoint.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/Station.h"
#include "ObjDup/StationManager.h"
#include "ObjDup/ObjDupProtocol.h"
#include "Core/CallContext.h"
#include "Core/CallContextRegister.h"
#include "Core/Core.h"
#include "Core/Scheduler.h"
#include "Core/SystemComponent.h"
#include "Core/SystemComponents.h"
#include "Core/NetZ.h"
#include "Core/Job.h"
#include "ObjDup/DOClass.h"

#define JJS_FILE ".\\JobJoinSession.cpp"

namespace Quazal {

    // qList::begin called out of line: retail's begin() here is a second-level
    // call, so it goes through this one-level helper.
    template <class T>
    inline typename qList<T>::iterator ListBegin(qList<T> &lst) {
        return lst.begin();
    }

    inline ByteStream &operator<<(ByteStream &oStream, unsigned int ui) {
        oStream.Append((const unsigned char *)&ui, 4, 1);
        return oStream;
    }

    ByteStream &operator<<(ByteStream &, const qList<StationURL> &);

    inline CallContextRegister *GetCallContextRegister(Core *pCore) {
        return pCore->m_pCallContextRegister;
    }

    inline SystemComponents *GetSystemComponents(Core *pCore) { return pCore->m_pSystemComponents; }

    inline bool IsFaulty(SystemComponent *pComponent) {
        return pComponent->GetState() == SystemComponent::Faulty;
    }

    inline bool IsReady(SystemComponent *pComponent) {
        return pComponent->GetState() == SystemComponent::Ready
            || pComponent->GetState() == SystemComponent::ReadyInUse;
    }

    // The duplication-space component (SystemComponents + 0x24).
    inline SystemComponent *GetDupSpace(SystemComponents *pComponents) {
        return *(SystemComponent **)((char *)pComponents + 0x24);
    }

    bool LocalStationIsReady();
    bool WellKnownObjectsCreated();

    class NATTraversalEngine : public RootObject {
    public:
        unsigned int GetLocalCID() const;
    };

    class PRUDPTransport;

    class Network : public RootObject {
    public:
        qProtectedList<StationURL> *GetStationURLs();
        NATTraversalEngine *GetNATTraversalEngine();

        char m_data[0x4c];
        PRUDPTransport *m_pTransport; // 0x4C
    };

    unsigned short GetWellKnownPort();

    // Retail calls this out of line (0x823EBC90, folded with an /O1 COMDAT of
    // the same body): /Ob1 declines it here and still reserves its locals.
    inline Network *GetNetwork() {
        unsigned int uiContext = PseudoSingleton::GetCurrentContext();
        InstanceControl *pInstance =
            (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(1, uiContext);
        Network *pNetwork = 0;
        if (pInstance != 0) {
            pNetwork = (Network *)pInstance->m_pDelegatorInstance;
        }
        return pNetwork;
    }

    // Retail reads the transport here and never uses the value: a void inline
    // whose null path returns before the read (the doubled branch after the
    // null test is that early return).
    inline void CheckTransport() {
        Network *pNetwork = GetNetwork();
        if (pNetwork == 0) {
            return;
        } else {
            PRUDPTransport *pTransport = pNetwork->m_pTransport;
        }
    }

    inline CriticalSection &GetLock(qProtectedList<StationURL> *pList) { return pList->mCSList; }
    inline qList<StationURL> &GetList(qProtectedList<StationURL> *pList) { return pList->mOList; }

    class ConnectivityTester : public RootObject {
    public:
        virtual ~ConnectivityTester();
    };

    class ParticipantList;

    class ConnectivityTesterRef : public RootObject {
    public:
        static ConnectivityTesterRef *GetInstance();
        ConnectivityTester *Get() const { return m_pTester; }
        bool Test(CallContext *, ParticipantList *, unsigned int);

        ConnectivityTester *m_pTester;
    };

    class Participant {
    public:
        ~Participant();
    };

    class ParticipantList : public qList<Participant> {
    public:
        ParticipantList();
        ~ParticipantList();
        void SetResult(unsigned int);
    };

    void ExtractParticipants(Message *, ParticipantList *);

    class ConnectionManager : public RootObject {
    public:
        static ConnectionManager *GetInstance();
        bool Connect(
            CallContext *, Buffer *, int, qList<StationURL> &, EndPoint **, unsigned int
        );
        bool Disconnect(CallContext *, EndPoint *);
        void SetJoining(bool);
        int GetMode();
        void AddPeerAddress(InetAddress *);
    };


    class DOClassesTable : public RootObject {
    public:
        static DOClassesTable *GetInstance();
        unsigned int GetMaxClassID();

        unsigned int m_unk0;
        DOClass **m_ppClasses; // 0x4
    };

    // The DO class of the well-known objects the join waits for (its
    // DORefTemplate<>::IsValid is retail 0x82A9AA00).
    class WKObject : public DuplicatedObject {
    public:
        static unsigned int GetClassID();
    };

    class JobJoinSession;

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
        virtual void Trace(unsigned int);

        void SetStep(const Step &);
        int GetTimeInStep() const { return Time::GetTime() - m_tStepTime; }
        void ResumeOnCallCompletion(CallContext *, Step *);

        Time m_tStepTime; // 0x38
        unsigned int m_unk40[6];
        unsigned int m_unk58; // 0x58
        unsigned int m_unk5c;
    };

    class JoinCancelCallback : public CallbackRoot {
    public:
        JoinCancelCallback(JobJoinSession *);
        virtual ~JoinCancelCallback();
        virtual void CallObjectMethod();
        void Detach() { m_pJob = 0; }

        JobJoinSession *m_pJob; // 0x4
    };

    class JobJoinSession : public StepSequenceJob {
    public:
        JobJoinSession(const qList<StationURL> &, unsigned int);
        virtual ~JobJoinSession();
        virtual void Trace(unsigned int);

        static void PrepareURL(StationURL &);
        void Cancel();
        void InitiateConnection();
        void ProcessConnectionResult();
        void SendGetParticipantsRequest();
        void SendJoinRequest();
        void WaitForResponse();
        void ProcessWelcome();
        void WaitForJoinTermination();
        void SignalCallContext(CallContext::_State, qResult);
        void JoinDenied();
        void InitiateDisconnect();
        void CompleteDisconnect();
        void ConnectivityTestFailed();
        void JoinFailed();
        void JoinSuccess();
        void ProcessGetParticipantsResponse(Message *, bool);
        void TestConnection();
        void ProcessPositiveJoinResponse(unsigned char, DOHandle, DOHandle);
        void CompleteJob();
        void SetNewContactPoint(const StationURL &);
        void ProcessNegativeJoinResponse(unsigned char, int);

        // Retail evaluates these three Connect arguments into stack temps, the
        // return slots of expanded inline helpers.
        static unsigned int GetConnectTimeout() { return s_uiConnectTimeout; }
        static ConnectionManager *GetConnectionManager() { return ConnectionManager::GetInstance(); }
        static Buffer *GetMsgBuffer(Message &oMsg) { return oMsg.GetBuffer(); }

        static unsigned int s_uiConnectTimeout;
        static int s_iJoinResponseTimeout;

        qList<StationURL> m_lURLs; // 0x60
        EndPoint *m_pEndPoint; // 0x68
        bool m_bNewContactPoint; // 0x6c
        unsigned char m_ucJoinResponse; // 0x6d
        unsigned int m_uiCallID; // 0x70
        JoinCancelCallback *m_pCancelCallback; // 0x74
        CallContext m_oCallContext; // 0x78
        ParticipantList m_lParticipants; // 0xc8
        qResult m_oResult; // 0xd0
    };

    JoinCancelCallback::JoinCancelCallback(JobJoinSession *pJob) : m_pJob(pJob) {}

    JoinCancelCallback::~JoinCancelCallback() { m_pJob = 0; }

    void JoinCancelCallback::CallObjectMethod() { m_pJob->Cancel(); }

    JobJoinSession::JobJoinSession(const qList<StationURL> &lURLs, unsigned int uiCallID)
        : StepSequenceJob(DebugString()), m_lURLs(lURLs), m_pCancelCallback(0) {
        qList<StationURL>::iterator it = m_lURLs.std::list<StationURL, MemAllocator<StationURL> >::begin();
        while (it != m_lURLs.end()) {
            PrepareURL(*it);
            ++it;
        }
        m_pEndPoint = 0;
        m_ucJoinResponse = 0;
        m_uiCallID = uiCallID;
        CallContext *pContext = GetCallContextRegister(Core::GetInstance())->GetCallContext(uiCallID);
        m_pCancelCallback = new (JJS_FILE, 0x48) JoinCancelCallback(this);
        pContext->SetCancelCallback(m_pCancelCallback);
        ObjDupProtocol::GetInstance()->SetJoinSession(this);
        m_unk58 = 4;
        SetStep(Step(
            (JobStateFunc)&JobJoinSession::InitiateConnection, "JobJoinSession::InitiateConnection"
        ));
        m_oResult = 0x8006000E;
        m_bNewContactPoint = false;
        ConnectionManager::GetInstance()->SetJoining(false);
    }

    JobJoinSession::~JobJoinSession() {}

    void JobJoinSession::PrepareURL(StationURL &url) {
        if (url.GetPortNumber() == 0) {
            CheckTransport();
            url.SetPortNumber(GetWellKnownPort());
        }
        url.SetStreamType((Stream::Type)1);
        url.SetStreamID(1);
    }

    void JobJoinSession::Cancel() {
        m_oCallContext.SetFlag(4);
        m_pEndPoint = 0;
        m_oResult = 0x8006000B;
        m_pCancelCallback->Detach();
        ObjDupProtocol::GetInstance()->SetJoinSession(0);
        SetToComplete();
    }

    void JobJoinSession::InitiateConnection() {
        {
            ScopedCS oCS(GetLock(GetNetwork()->GetStationURLs()));
            qList<StationURL>::iterator it = ListBegin(GetList(GetNetwork()->GetStationURLs()));
            while (it != GetList(GetNetwork()->GetStationURLs()).end()) {
                (*it).Trace(0x4000);
                ++it;
            }
        }
        Message oMsg;
        oMsg << Station::GetLocalStation().GetValue();
        // Retail builds this second word in a temporary (0 stored at 0xa4,
        // copied to 0xa4-4 and appended), the shape of a default DOHandle
        // converted to unsigned int. Written that way here, /Ob1 runs out of
        // inline budget and calls ~ScopedCS above out of line instead of
        // expanding it (measured), so the local stays.
        unsigned int uiReserved = 0;
        oMsg << uiReserved;
        if (!GetConnectionManager()->Connect(
                &m_oCallContext, GetMsgBuffer(oMsg), 0, m_lURLs, &m_pEndPoint, GetConnectTimeout()
            )) {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            return;
        }
        if (m_oCallContext.GetState() == CallContext::CallPending) {
            SetToSuspended();
            ResumeOnCallCompletion(
                &m_oCallContext,
                new (JJS_FILE, 0x85) Step(
                    (JobStateFunc)&JobJoinSession::ProcessConnectionResult,
                    "JobJoinSession::ProcessConnectionResult"
                )
            );
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::ProcessConnectionResult,
                "JobJoinSession::ProcessConnectionResult"
            ));
        }
    }

    void JobJoinSession::ProcessConnectionResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::SendGetParticipantsRequest,
                "JobJoinSession::SendGetParticipantsRequest"
            ));
        } else {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
        }
        m_oCallContext.Reset();
    }

    void JobJoinSession::SendGetParticipantsRequest() {
        if (ConnectivityTesterRef::GetInstance()->Get() != 0) {
            Message *pMessage = ObjDupProtocol::GetInstance()->CreateGetParticipantsRequest();
            qList<StationURL> lstURLs;
            ScopedCS oLock(GetLock(GetNetwork()->GetStationURLs()));
            qList<StationURL>::iterator iterURL = ListBegin(GetList(GetNetwork()->GetStationURLs()));
            NATTraversalEngine *pEngine = GetNetwork()->GetNATTraversalEngine();
            while (iterURL != GetList(GetNetwork()->GetStationURLs()).end()) {
                StationURL url(*iterURL);
                if (pEngine != 0 && pEngine->GetLocalCID() != 0) {
                    url.SetRVConnectionID(pEngine->GetLocalCID());
                }
                lstURLs.push_back(url);
                ++iterURL;
            }
            *pMessage << lstURLs;
            if (!ObjDupProtocol::GetInstance()->Send(m_pEndPoint, pMessage, 1)) {
                SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            } else {
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::WaitForResponse, "JobJoinSession::WaitForResponse"
                ));
            }
            delete pMessage;
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::SendJoinRequest, "JobJoinSession::SendJoinRequest"
            ));
        }
    }

    void JobJoinSession::SendJoinRequest() {
        m_ucJoinResponse = 0;
        Message *pMsg = ObjDupProtocol::GetInstance()->CreateJoinRequest();
        bool bResult = false;
        if (ObjDupProtocol::GetInstance()->Send(m_pEndPoint, pMsg, 1).Equals(bResult)) {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForResponse, "JobJoinSession::WaitForResponse"
            ));
        }
        delete pMsg;
    }

    void JobJoinSession::WaitForResponse() {
        switch (m_ucJoinResponse) {
        case 1:
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::ProcessWelcome, "JobJoinSession::ProcessWelcome"
            ));
            break;
        case 0:
            if (!m_pEndPoint->IsConnected()) {
                SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            } else if (GetTimeInStep() > s_iJoinResponseTimeout) {
                SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            } else {
                SetToWaiting(0x32);
            }
            break;
        case 2:
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::InitiateDisconnect, "JobJoinSession::InitiateDisconnect"
            ));
            break;
        case 3:
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinDenied, "JobJoinSession::JoinDenied"));
            break;
        case 4:
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::TestConnection, "JobJoinSession::TestConnection"
            ));
            break;
        case 5:
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            break;
        }
    }

    void JobJoinSession::ProcessWelcome() {
        SetStep(Step(
            (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
            "JobJoinSession::WaitForJoinTermination"
        ));
    }

    void JobJoinSession::WaitForJoinTermination() {
        if (!m_pEndPoint->IsConnected()) {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            return;
        }
        if (!LocalStationIsReady()) {
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        SystemComponent *pDupSpace =
            (SystemComponent *)(Core::GetInstance() == 0 ? 0 : GetSystemComponents(Core::GetInstance()))->unk24;
        pDupSpace->Initialize();
        if (IsFaulty(pDupSpace)) {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            return;
        }
        if (!IsReady(pDupSpace)) {
            pDupSpace->Trace(SystemComponent::Ready, true);
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        if (!WellKnownObjectsCreated()) {
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        {
            DORefTemplate<Station> refLocal(Station::GetLocalStation());
            if (!refLocal.IsValid()) {
                SetToWaiting(0x32);
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                    "JobJoinSession::WaitForJoinTermination"
                ));
                return;
            }
        }
        if (StationManager::GetInstance()->ConnectStation(StationManager::GetInstance()->GetInitialStation()) != 0) {
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        {
        DOClassesTable *pTable = DOClassesTable::GetInstance();
        unsigned int uiMax = pTable->GetMaxClassID();
        for (unsigned int i = 0; i <= uiMax; i++) {
            DOClass *pClass = pTable->m_ppClasses[i];
            if (pClass != 0) {
                DOHandle hWellKnown = pClass->GetWKHandle();
                DORefTemplate<WKObject> refWK(hWellKnown);
                if (!refWK.IsValid()) {
                    SetToWaiting(0x32);
                    SetStep(Step(
                        (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                        "JobJoinSession::WaitForJoinTermination"
                    ));
                    return;
                }
            }
        }
        }
        SetStep(Step((JobStateFunc)&JobJoinSession::JoinSuccess, "JobJoinSession::JoinSuccess"));
    }

    void JobJoinSession::SignalCallContext(CallContext::_State eState, qResult oResult) {
        CallContext *pContext = GetCallContextRegister(Core::GetInstance())->GetCallContext(m_uiCallID);
        if (pContext != 0) {
            pContext->SetStateImpl(eState, oResult, true);
        }
    }

    void JobJoinSession::JoinDenied() {
        if (m_oResult.Equals((int)0x8006000E)) {
            m_oResult = 0x8006000C;
        }
        SetStep(Step(
            (JobStateFunc)&JobJoinSession::InitiateDisconnect, "JobJoinSession::InitiateDisconnect"
        ));
    }

    void JobJoinSession::InitiateDisconnect() {
        if (!m_bNewContactPoint) {
            ConnectionManager::GetInstance()->SetJoining(true);
        }
        if (!ConnectionManager::GetInstance()->Disconnect(&m_oCallContext, m_pEndPoint)) {
            m_pEndPoint = 0;
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::CompleteDisconnect, "JobJoinSession::CompleteDisconnect"
            ));
        } else {
            m_pEndPoint = 0;
            if (m_oCallContext.GetState() == CallContext::CallPending) {
                SetToSuspended();
                ResumeOnCallCompletion(
                    &m_oCallContext,
                    new (JJS_FILE, 0x166) Step(
                        (JobStateFunc)&JobJoinSession::CompleteDisconnect,
                        "JobJoinSession::CompleteDisconnect"
                    )
                );
            } else {
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::CompleteDisconnect,
                    "JobJoinSession::CompleteDisconnect"
                ));
            }
        }
    }

    void JobJoinSession::CompleteDisconnect() {
        if (m_bNewContactPoint) {
            m_bNewContactPoint = false;
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::InitiateConnection, "JobJoinSession::InitiateConnection"
            ));
        } else {
            SetStep(Step((JobStateFunc)&JobJoinSession::CompleteJob, "JobJoinSession::CompleteJob"));
        }
        m_oCallContext.Reset();
    }

    void JobJoinSession::ConnectivityTestFailed() {
        m_oResult = 0x8006000D;
        if (m_pEndPoint == 0) {
            SetStep(Step((JobStateFunc)&JobJoinSession::CompleteJob, "JobJoinSession::CompleteJob"));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::InitiateDisconnect, "JobJoinSession::InitiateDisconnect"
            ));
        }
    }

    void JobJoinSession::JoinFailed() {
        m_oResult = 0x8006000B;
        if (m_pEndPoint == 0) {
            SetStep(Step((JobStateFunc)&JobJoinSession::CompleteJob, "JobJoinSession::CompleteJob"));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::InitiateDisconnect, "JobJoinSession::InitiateDisconnect"
            ));
        }
    }

    void JobJoinSession::JoinSuccess() {
        m_oResult = 0x00060001;
        Station::GetLocalInstance()->m_oConnectionInfo.m_bURLInitialized = true;
        Station::GetLocalInstance()->Update(&Station::GetLocalInstance()->m_oConnectionInfo);
        SetStep(Step((JobStateFunc)&JobJoinSession::CompleteJob, "JobJoinSession::CompleteJob"));
    }

    void JobJoinSession::ProcessGetParticipantsResponse(Message *pMsg, bool bAccepted) {
        ScopedCS oCS(Scheduler::GetInstance()->unk38);
        if (bAccepted) {
            m_lParticipants.clear();
            ExtractParticipants(pMsg, &m_lParticipants);
            m_ucJoinResponse = 4;
        } else {
            m_ucJoinResponse = 5;
        }
    }

    void JobJoinSession::TestConnection() {
        if (!m_lParticipants.empty()) {
            switch (m_oCallContext.GetState()) {
            case CallContext::CallInit: {
                ConnectivityTesterRef *pTester = ConnectivityTesterRef::GetInstance();
                if (!pTester->Test(&m_oCallContext, &m_lParticipants, 5000)) {
                    SetStep(Step(
                        (JobStateFunc)&JobJoinSession::ConnectivityTestFailed,
                        "JobJoinSession::ConnectivityTestFailed"
                    ));
                    return;
                }
            }
            case CallContext::CallPending:
                SetToWaiting(0xFA);
                break;
            case CallContext::CallSuccess:
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::SendJoinRequest, "JobJoinSession::SendJoinRequest"
                ));
                m_oCallContext.Reset();
                break;
            case CallContext::CallError:
                m_lParticipants.SetResult(4);
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::ConnectivityTestFailed,
                    "JobJoinSession::ConnectivityTestFailed"
                ));
                m_oCallContext.Reset();
                break;
            }
        } else {
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::SendJoinRequest, "JobJoinSession::SendJoinRequest"
            ));
        }
    }

    void JobJoinSession::ProcessPositiveJoinResponse(
        unsigned char ucResponse, DOHandle hMaster, DOHandle hStation
    ) {
        Station::SetLocalStation(hMaster);
        m_pEndPoint->SetPID(hStation.GetValue());
        if (ConnectionManager::GetInstance()->GetMode() == 1) {
            ConnectionManager::GetInstance()->AddPeerAddress(
                m_pEndPoint->GetAddress().GetInetAddress()
            );
            m_pEndPoint->Unk7();
        }
        StationManager::GetInstance()->SetInitialConnectionPoint(hStation, m_pEndPoint);
        m_ucJoinResponse = ucResponse;
    }

    void JobJoinSession::CompleteJob() {
        ObjDupProtocol::GetInstance()->SetJoinSession(0);
        SetToComplete();
        if (m_oResult != (int)0x00060001) {
            ObjDupProtocol::GetInstance()->StopToListen();
            SignalCallContext(CallContext::CallError, m_oResult);
        } else {
            Station::GetLocalInstance()->SetState((Station::_State)3);
            NetZ::GetInstance()->CompleteJoin();
            SignalCallContext(CallContext::CallSuccess, qResult(0x00060001));
        }
    }

    void JobJoinSession::SetNewContactPoint(const StationURL &url) {
        m_lURLs.clear();
        m_lURLs.push_front(url);
        PrepareURL(m_lURLs.front());
        m_bNewContactPoint = true;
    }

    void JobJoinSession::ProcessNegativeJoinResponse(unsigned char ucResponse, int iReason) {
        m_ucJoinResponse = ucResponse;
        m_oResult = iReason;
    }

    void JobJoinSession::Trace(unsigned int uiFlags) {
        if (m_pEndPoint != 0) {
            m_pEndPoint->SignalEvent(uiFlags);
        }
    }

}
