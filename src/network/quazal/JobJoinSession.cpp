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
// The classes the job calls into are declared here only as far as this TU uses
// them, with the layouts retail's code reads. At /Od the local NAMES set the
// stack layout (a walk over the scope's symbol hash buckets), and an inline
// function the compiler declines to expand still reserves its slots in the
// caller, so both are chosen to reproduce retail's frames.

#include <list>

#define JJS_FILE ".\\JobJoinSession.cpp"

namespace Quazal {

    class RootObject {
    public:
        static void *operator new(unsigned int, const char *, unsigned int);
        static void *operator new(unsigned int, void *p) { return p; }
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
        static void operator delete(void *, void *) {}
        ~RootObject() {}
    };

    class MemoryManager : public RootObject {
    public:
        enum _InstructionType {
            _InstType0,
            _InstType1,
            _InstType2,
            _InstType3,
            _InstType4,
            _InstType5,
            _InstType6,
            _InstType7
        };
        static MemoryManager *GetDefaultMemoryManager();
        static void *Allocate(
            MemoryManager *, unsigned long, const char *, unsigned int, _InstructionType
        );
        static void Free(MemoryManager *, void *, _InstructionType);
    };

    template <class T>
    class MemAllocator {
    public:
        typedef unsigned int size_type;
        typedef int difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;
        typedef const T *const_pointer;
        typedef const T &const_reference;

        template <class T2>
        struct rebind {
            typedef MemAllocator<T2> other;
        };

        MemAllocator() {}

        template <class T2>
        operator MemAllocator<T2>() const {
            return MemAllocator<T2>();
        }

        pointer address(reference value) const { return &value; }
        const_pointer address(const_reference value) const { return &value; }
        size_type max_size() const { return size_type(-1) / sizeof(T); }

        pointer allocate(const size_type count, const void *hint = 0) const {
            return (pointer)MemoryManager::Allocate(
                MemoryManager::GetDefaultMemoryManager(),
                count * sizeof(T),
                "Unknown",
                0,
                MemoryManager::_InstType7
            );
        }
        void deallocate(pointer ptr, size_type count) const {
            MemoryManager::Free(
                MemoryManager::GetDefaultMemoryManager(), ptr, MemoryManager::_InstType7
            );
        }
        void construct(pointer ptr, const_reference value) const { new (ptr) T(value); }
        void destroy(pointer ptr) const { ptr->~T(); }
    };

    template <class T>
    class qList : public std::list<T, MemAllocator<T> >, public RootObject {
    public:
        typedef typename std::list<T, MemAllocator<T> >::iterator iterator;
        typedef typename std::list<T, MemAllocator<T> >::const_iterator const_iterator;
        iterator begin() { return std::list<T, MemAllocator<T> >::begin(); }
    };

    class MutexPrimitive : public RootObject {
    public:
        static bool s_bNoOp;
        void *m_hMutex;
    };

    class CriticalSection : public RootObject {
    public:
        void EnterImpl();
        void LeaveImpl();
        void Enter() {
            if (!MutexPrimitive::s_bNoOp)
                EnterImpl();
        }
        void Leave() {
            if (!MutexPrimitive::s_bNoOp)
                LeaveImpl();
        }
        char m_data[0x14];
    };

    class ScopedCS : public RootObject {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), m_pCS(&cs) { m_pCS->Enter(); }
        ~ScopedCS() { EndScope(); }
        void EndScope() {
            if (m_bInScope) {
                m_pCS->Leave();
                m_bInScope = false;
            }
        }
        bool m_bInScope;
        CriticalSection *m_pCS;
    };

    template <class T>
    class qProtectedList : public RootObject {
    public:
        CriticalSection mCSList;
        qList<T> &GetList() { return mOList; }
        CriticalSection &GetLock() { return mCSList; }

        qList<T> mOList;
    };

    class DebugString {
    public:
        DebugString() {}
    };

    struct qResult {
        qResult();
        qResult(const int &);
        bool Equals(const bool &) const;
        operator bool() const;
        bool operator==(const int &) const;
        bool operator!=(const int &) const;
        qResult &operator=(const int &);

        int m_iCode;
        int m_iLine;
        const char *m_szFile;
    };

    class Time : public RootObject {
    public:
        Time(unsigned long long ui64Value) : m_ui64Value(ui64Value) {}
        static Time GetTime();
        long long operator-(const Time &) const;

        unsigned long long m_ui64Value;
    };

    namespace Stream {
        enum Type {
            Invalid = 0,
            DO = 1
        };
    }

    class InetAddress;

    class StationURL : public RootObject {
    public:
        StationURL(const StationURL &);
        ~StationURL();
        unsigned short GetPortNumber() const;
        void SetPortNumber(unsigned short);
        void SetStreamType(Stream::Type);
        void SetStreamID(unsigned char);
        void SetRVConnectionID(unsigned int);
        InetAddress *GetInetAddress() const;
        void Trace(unsigned int) const;

        unsigned int m_data[0x64 / 4];
    };

    class DOHandle {
    public:
        DOHandle() : m_uiValue(0) {}
        DOHandle(const DOHandle &o) : m_uiValue(o.m_uiValue) {}
        ~DOHandle() {}
        operator unsigned int() const { return m_uiValue; }

        unsigned int m_uiValue;
    };

    namespace StationManager {
        DOHandle GetLocalStationHandle();
        void SetMasterStationHandle(DOHandle);
    }

    class DuplicatedObject;

    class DORef : public RootObject {
    public:
        DORef(DOHandle);
        ~DORef();

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        bool IsValid() const;
    };

    class DataSet {};

    class StationState : public DataSet {
    public:
        bool m_bJoined;
    };

    class SessionClock {
    public:
        static long long (*s_pfGetTime)();
        static Time GetTime() {
            if (s_pfGetTime != 0) {
                return Time(s_pfGetTime());
            } else {
                return Time(0);
            }
        }
    };

    class DuplicatedObject : public RootObject {
    public:
        bool UpdateImpl(DataSet *, const Time &);
        bool Update(DataSet *pDataSet) { return UpdateImpl(pDataSet, SessionClock::GetTime()); }
    };

    class Buffer;

    class ByteStream : public RootObject {
    public:
        void Append(const void *, unsigned int, bool);
        ByteStream &operator<<(unsigned int ui) {
            Append(&ui, 4, true);
            return *this;
        }
    };

    class Message : public ByteStream {
    public:
        Message();
        ~Message();
        Buffer *GetBuffer();

        char m_data[0x30];
    };

    ByteStream &operator<<(ByteStream &, const qList<StationURL> &);

    class CallbackRoot : public RootObject {
    public:
        CallbackRoot() {}
        virtual ~CallbackRoot() {}
        virtual void Call();
        virtual void CallObjectMethod() = 0;
    };

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
        void Reset();
        void SetFlag(unsigned int);
        void RegisterCancelCallback(CallbackRoot *);
        void SetState(_State, qResult, bool);

        _State GetState() const { return m_eState; }

        unsigned int m_unk8; // 0x8
        _State m_eState; // 0xc
        unsigned int m_unk10[16];
    };

    class CallContextRegister {
    public:
        CallContext *GetContext(unsigned int);
    };

    class EndPoint : public RootObject {
    public:
        virtual ~EndPoint();
        virtual void Func1();
        virtual void Func2();
        virtual void Func3();
        virtual bool IsConnected();
        virtual void Func5();
        virtual void Func6();
        virtual void Disconnect();
        virtual void Func8();
        virtual void Func9();
        virtual void Func10();
        virtual void Func11();
        virtual void Func12();
        virtual void Func13();
        virtual void Func14();
        virtual void Func15();
        virtual void Func16();
        virtual void Trace(unsigned int);

        void SetPID(unsigned int);
        const StationURL &GetURL() const { return m_oURL; }

        unsigned int m_unk4;
        StationURL m_oURL; // 0x8
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
            Ready = 4,
            Running = 8,
            Faulty = 0x80
        };
        void SetState(_State, bool);
        void Use();
        _State GetState() const { return m_eState; }
        bool IsFaulty() const { return GetState() == Faulty; }
        bool IsReady() const { return GetState() == Ready || GetState() == Running; }

        unsigned int m_unk0[3];
        _State m_eState; // 0xc
    };

    class SystemComponents : public RootObject {
    public:
        unsigned int m_unk0[9];
        SystemComponent *m_pDupSpace; // 0x24
    };

    // Declared out of line here. With the header's inline body, /Ob1 declines
    // it and reserves its this/ui/idx in every caller: CompleteJob's frame then
    // matches retail, but the constructor's and ProcessGetParticipantsResponse's
    // grow past it (measured), so this TU keeps the out-of-line declaration.
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

    class PseudoSingleton : public RootObject {
    public:
        static unsigned int GetCurrentContext();
    };

    class Scheduler : public RootObject {
    public:
        static Scheduler *GetInstance();

        char m_data[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
    };

    // Retail calls GetInstance out of line (0x823EA910, an /O1 COMDAT): the
    // /Ob1 inliner gives up on it here, after reserving its locals.
    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (pInstance != 0) {
                pCore = (Core *)pInstance->m_pDelegatorInstance;
            }
            return pCore;
        }
        Scheduler *GetScheduler() const { return m_pScheduler; }
        CallContextRegister *GetCallContextRegister() const { return m_pCallContextRegister; }
        SystemComponents *GetSystemComponents() const { return m_pComponents; }

        unsigned int m_unk0[2];
        Scheduler *m_pScheduler; // 0x8
        CallContextRegister *m_pCallContextRegister; // 0xc
        SystemComponents *m_pComponents; // 0x10
    };

    inline Scheduler *Scheduler::GetInstance() {
        Core *pCore = Core::GetInstance();
        if (pCore == 0) {
            return 0;
        } else {
            return pCore->GetScheduler();
        }
    }


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

    void *GetInstanceType1Delegator();
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

    class Station : public DuplicatedObject {
    public:
        static Station *GetLocalStation();
        static bool IsReadyToJoin();
        void SetState(int);

        char m_data[0x70];
        StationState m_oState; // 0x70
    };

    class StationTable : public RootObject {
    public:
        static StationTable *GetInstance();
        int GetStationState(DOHandle);
        void AddStation(DOHandle, EndPoint *);
        DOHandle GetMasterHandle() const { return m_hMaster; }

        char m_data[0x50];
        DOHandle m_hMaster; // 0x50
    };

    class DOClass : public RootObject {
    public:
        DOHandle GetWKHandle();
    };

    class DOClassesTable : public RootObject {
    public:
        static DOClassesTable *GetInstance();
        unsigned int GetMaxClassID();

        unsigned int m_unk0;
        DOClass **m_ppClasses; // 0x4
    };

    class WKObject : public RootObject {
    public:
        static bool AllCreated();
    };

    class NetZCore : public RootObject {
    public:
        static NetZCore *GetInstance(unsigned int uiContext) {
            return (NetZCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static NetZCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        void StartSession();
    };

    class JobJoinSession;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        class Message *CreateGetParticipantsRequest();
        class Message *CreateJoinRequest();
        qResult Send(EndPoint *, Message *, unsigned int);
        void StopToListen();

        char m_data[0x30];
        void SetJoinSession(JobJoinSession *pJoinSession) { m_pJoinSession = pJoinSession; }

        JobJoinSession *m_pJoinSession; // 0x30
    };

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

        void SetToWaiting(unsigned int);
        void SetToSuspended();
        void SetToComplete();

        unsigned int m_unk8[12];
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
        virtual void Trace(unsigned int);

        void SetStep(const Step &);
        int GetTimeInStep() const { return Time::GetTime() - m_tStepTime; }
        void ResumeOnCallCompletion(CallContext *, Step *);

        Time m_tStepTime; // 0x38
        unsigned int m_unk40[6];
        unsigned int m_unk58; // 0x58
        unsigned int m_unk5c;
    };

    class JobJoinSession;

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
        CallContext *pContext = Core::GetInstance()->GetCallContextRegister()->GetContext(uiCallID);
        m_pCancelCallback = new (JJS_FILE, 0x48) JoinCancelCallback(this);
        pContext->RegisterCancelCallback(m_pCancelCallback);
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
        url.SetStreamType(Stream::DO);
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
            ScopedCS oCS(GetNetwork()->GetStationURLs()->GetLock());
            qList<StationURL>::iterator it = GetNetwork()->GetStationURLs()->GetList().begin();
            while (it != GetNetwork()->GetStationURLs()->GetList().end()) {
                (*it).Trace(0x4000);
                ++it;
            }
        }
        Message oMsg;
        oMsg << (unsigned int)StationManager::GetLocalStationHandle();
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
            ScopedCS oLock(GetNetwork()->GetStationURLs()->GetLock());
            qList<StationURL>::iterator iterURL = GetNetwork()->GetStationURLs()->GetList().begin();
            NATTraversalEngine *pEngine = GetNetwork()->GetNATTraversalEngine();
            while (iterURL != GetNetwork()->GetStationURLs()->GetList().end()) {
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
        if (!Station::IsReadyToJoin()) {
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        SystemComponent *pDupSpace =
            (Core::GetInstance() == 0 ? 0 : Core::GetInstance()->GetSystemComponents())->m_pDupSpace;
        pDupSpace->Use();
        if (pDupSpace->IsFaulty()) {
            SetStep(Step((JobStateFunc)&JobJoinSession::JoinFailed, "JobJoinSession::JoinFailed"));
            return;
        }
        if (!pDupSpace->IsReady()) {
            pDupSpace->SetState(SystemComponent::Ready, true);
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        if (!WKObject::AllCreated()) {
            SetToWaiting(0x32);
            SetStep(Step(
                (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                "JobJoinSession::WaitForJoinTermination"
            ));
            return;
        }
        {
            DORefTemplate<Station> refLocal(StationManager::GetLocalStationHandle());
            if (!refLocal.IsValid()) {
                SetToWaiting(0x32);
                SetStep(Step(
                    (JobStateFunc)&JobJoinSession::WaitForJoinTermination,
                    "JobJoinSession::WaitForJoinTermination"
                ));
                return;
            }
        }
        if (StationTable::GetInstance()->GetStationState(StationTable::GetInstance()->GetMasterHandle()) != 0) {
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
        CallContext *pContext = Core::GetInstance()->GetCallContextRegister()->GetContext(m_uiCallID);
        if (pContext != 0) {
            pContext->SetState(eState, oResult, true);
        }
    }

    void JobJoinSession::JoinDenied() {
        if (m_oResult == (int)0x8006000E) {
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
        Station::GetLocalStation()->m_oState.m_bJoined = true;
        Station::GetLocalStation()->Update(&Station::GetLocalStation()->m_oState);
        SetStep(Step((JobStateFunc)&JobJoinSession::CompleteJob, "JobJoinSession::CompleteJob"));
    }

    void JobJoinSession::ProcessGetParticipantsResponse(Message *pMsg, bool bAccepted) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
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
        StationManager::SetMasterStationHandle(hMaster);
        m_pEndPoint->SetPID(hStation);
        if (ConnectionManager::GetInstance()->GetMode() == 1) {
            ConnectionManager::GetInstance()->AddPeerAddress(
                m_pEndPoint->GetURL().GetInetAddress()
            );
            m_pEndPoint->Disconnect();
        }
        StationTable::GetInstance()->AddStation(hStation, m_pEndPoint);
        m_ucJoinResponse = ucResponse;
    }

    void JobJoinSession::CompleteJob() {
        ObjDupProtocol::GetInstance()->SetJoinSession(0);
        SetToComplete();
        if (m_oResult != (int)0x00060001) {
            ObjDupProtocol::GetInstance()->StopToListen();
            SignalCallContext(CallContext::CallError, m_oResult);
        } else {
            Station::GetLocalStation()->SetState(3);
            NetZCore::GetInstance()->StartSession();
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
            m_pEndPoint->Trace(uiFlags);
        }
    }

}
