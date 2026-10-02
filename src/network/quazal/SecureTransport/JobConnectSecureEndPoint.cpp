// Quazal NetZ - .\SecureTransport\JobConnectSecureEndPoint.cpp
//
// Retail TU: .text 0x82B421C8..0x82B43590, built /Od /Oi- /EHs-c- /Ob1 /GR-
// (objects.json). No function carries an EH record, and the vtable at
// 0x8218EB34 has no RTTI locator in front of it.
//
// The job that opens a secure (Kerberos-authenticated) PRUDP connection for a
// SecureEndPoint. It runs as a small state machine over m_eStep:
//   0 ParseURL, then 2
//   1 RequestConnectionData: ask for a ticket and for the server's URLs
//   2 wait until both have arrived, then 3 (else back to 1)
//   3 PerformConnect: build the request and connect through ConnectionManager
//   4 CompleteConnection: validate the server's response
// and reports through the endpoint's completion callback when the job is done.
//
// The classes this TU touches are declared here with their retail layouts.
// /Od lays a scope's locals out by a walk over the symbol table's hash
// buckets, so the local NAMES below determine the stack offsets, and every
// allocation passes __FILE__/__LINE__, so the #line directives are
// load-bearing.

#include <list>

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
        qList() {}
        ~qList() {}
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

    class Scheduler : public RootObject {
    public:
        static Scheduler *GetInstance();

        char m_data[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
    };

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

    // Retail calls GetInstance out of line (0x823EA910): the /Ob1 inliner
    // gives up on it, after reserving its locals.
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

        char m_data[0x8];
        Scheduler *m_pScheduler; // 0x8
    };

    inline Scheduler *Scheduler::GetInstance() {
        Core *pCore = Core::GetInstance();
        if (pCore == 0) {
            return 0;
        } else {
            return pCore->GetScheduler();
        }
    }

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

    class Time : public RootObject {
    public:
        Time() : m_ui64Value(0) {}
        ~Time() {}
        Time &operator=(const Time &);
        static Time FromMilliseconds(unsigned int);
        static unsigned int ToMilliseconds(Time);

        unsigned long long m_ui64Value;
    };

    class String : public RootObject {
    public:
        ~String();
        unsigned int GetLength() const;

        char *m_szContent;
    };

    class InetAddress;

    class StationURL : public RootObject {
    public:
        StationURL();
        StationURL(const StationURL &);
        ~StationURL();
        StationURL &operator=(const StationURL &);
        unsigned int GetConnectionID() const;
        void SetConnectionID(unsigned int);
        unsigned int GetPrincipalID() const;
        String GetAddress() const;
        unsigned short GetPortNumber() const;
        unsigned char GetStreamID() const;
        void SetStreamID(unsigned char);
        InetAddress *GetInetAddress() const;
        bool SetInetAddress(const InetAddress *);

        unsigned int m_data[0x64 / 4];
    };

    class UserContext : public RootObject {
    public:
        UserContext(void *p) : m_pPointer(p) {}
        ~UserContext() {}
        void *GetPointer() const { return m_pPointer; }

        void *m_pPointer;
    };

    class Buffer;

    class CallContext : public RootObject {
    public:
        enum _State {
            CallInit = 0,
            CallPending = 1,
            CallSuccess = 2,
            CallError = 3,
            CallCancelled = 4
        };
        typedef void (*CompletionCallback)(CallContext *, const UserContext *);

        CallContext();
        virtual ~CallContext();
        void RegisterCompletionCallback(CompletionCallback, const UserContext &, bool);
        void Trace(unsigned int);
        _State GetState() const { return m_eState; }

        // The vfptr is padded to 8 because of the Time member.
        unsigned int m_unk8; // 0x8
        _State m_eState; // 0xc
        char m_unk10[0x48 - 0x10];
        Time m_tTimeout; // 0x48
    };

    class ProtocolCallContext : public CallContext {
    public:
        ProtocolCallContext();
        virtual ~ProtocolCallContext();

        char m_unk50[0x68 - 0x50];
    };

    class EndPoint;
    class Ticket;

    class TicketManager : public RootObject {
    public:
        bool AcquireTicket(CallContext *, unsigned int, unsigned int, Ticket **);
        void ReleaseTicket(Ticket *);
    };

    class AuthenticationClient : public RootObject {
    public:
        TicketManager *GetTicketManager() const { return m_pTicketManager; }

        char m_unk0[0x28];
        TicketManager *m_pTicketManager; // 0x28
    };

    class _DDL_ConnectionData : public RootObject {
    public:
        _DDL_ConnectionData() {}
        virtual ~_DDL_ConnectionData() {}

        StationURL m_urlRegularProtocols; // 0x4
        unsigned int m_uiConnectionID; // 0x68
    };

    class ConnectionData : public _DDL_ConnectionData {
    public:
        ConnectionData() {}
        virtual ~ConnectionData() {}
    };

    class SecureConnectionClient : public RootObject {
    public:
        bool RequestConnectionData(
            ProtocolCallContext *, unsigned int, unsigned int, qList<ConnectionData> *
        );
    };

    class ConnectionManager : public RootObject {
    public:
        bool ConnectImpl(
            CallContext *, Buffer *, Buffer *, const qList<StationURL> &, EndPoint **,
            unsigned int
        );
    };

    class StreamSettings : public RootObject {
    public:
        ConnectionManager *GetConnectionManager() const { return m_pConnectionManager; }

        char m_unk0[0x10];
        ConnectionManager *m_pConnectionManager; // 0x10
    };

    class Credentials : public RootObject {
    public:
        StreamSettings *GetSettings() const { return m_pSettings; }

        char m_unk0[0x8];
        unsigned int m_uiPID; // 0x8
        char m_unkc[0x18 - 0xc];
        StreamSettings *m_pSettings; // 0x18
    };

    class SecureStream : public RootObject {
    public:
        AuthenticationClient *GetAuthenticationClient() const { return m_pAuthenticationClient; }
        SecureConnectionClient *GetSecureConnectionClient() const {
            return m_pSecureConnectionClient;
        }
        Credentials *GetCredentials() const { return m_pCredentials; }

        char m_unk0[0x18];
        AuthenticationClient *m_pAuthenticationClient; // 0x18
        SecureConnectionClient *m_pSecureConnectionClient; // 0x1c
        Credentials *m_pCredentials; // 0x20
    };

    class qResult;

    class EndPoint : public RootObject {
    public:
        typedef void (*pfCompletion)(EndPoint *, qResult, const UserContext *);

        void SetConnectionID(unsigned int);
        void SetPrincipalID(unsigned int);
        unsigned int GetConnectionID() const { return m_uiConnectionID; }
        SecureStream *GetStream() const { return m_pStream; }

        void *m_vtbl; // 0x0
        SecureStream *m_pStream; // 0x4
        char m_unk8[0x74 - 0x8];
        unsigned int m_uiConnectionID; // 0x74
    };

    class SecureEndPoint : public EndPoint {
    public:
        void SetAssociatedEndPoint(EndPoint *);
    };

    class BitStream : public RootObject {
    public:
        BitStream();
        ~BitStream();
        void AdjustLength();
        Buffer *GetBuffer() const { return m_pBuffer; }

        unsigned int m_unk0;
        Buffer *m_pBuffer; // 0x4
        char m_unk8[0x14 - 0x8];
    };

    class KerberosAuthentication : public RootObject {
    public:
        static void
        PrepareConnectionRequest(BitStream *, AuthenticationClient *, Ticket *, unsigned int *);
        static bool ValidateConnectionResponse(BitStream *, unsigned int);
    };

    class RefCountedObject : public RootObject {
    public:
        RefCountedObject();
        virtual ~RefCountedObject();
        virtual RefCountedObject *AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
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

        Job(const DebugString &);
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute() = 0;
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual void GetTraceInfo() const;
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();
        virtual void Trace(unsigned int);

        State GetState() const { return m_eState; }
        void Resume();
        void SetToSuspended();
        void SetToRunning();
        void SetToComplete();

        char m_unk8[0x1c - 0x8];
        State m_eState; // 0x1c
        char m_unk20[0x28 - 0x20];
        Time m_tDeadline; // 0x28
        bool m_unk30;
        bool m_unk31;
    };

    class JobConnectSecureEndPoint : public Job {
    public:
        JobConnectSecureEndPoint(
            SecureEndPoint *, const StationURL *, Buffer *, Buffer *, EndPoint::pfCompletion,
            const UserContext &, unsigned int
        );
        virtual ~JobConnectSecureEndPoint();
        virtual void Execute();
        virtual void Trace(unsigned int);

        int GetStep() const { return m_eStep; }
        void ExecuteStep();
        void ParseURL();
        static void RequestCompletionCallback(CallContext *, const UserContext *);
        void CheckConnectionData();
        void RequestConnectionData();
        bool IsConnectionDataAvailable();
        void PrepareConnectionRequest();
        void GetConnectionURLs(qList<StationURL> &);
        void PerformConnect();
        static void ConnectCompletionCallback(CallContext *, const UserContext *);
        void ProcessConnectionResult(CallContext *);
        void CompleteConnection();

        int m_eStep; // 0x38
        ProtocolCallContext m_oTicketContext; // 0x40
        Ticket *m_pTicket; // 0xa8
        BitStream *m_pRequest; // 0xac
        BitStream *m_pResponse; // 0xb0
        unsigned int m_uiSessionKey; // 0xb4
        ProtocolCallContext m_oDataContext; // 0xb8
        CallContext m_oConnectContext; // 0x120
        SecureEndPoint *m_pEndPoint; // 0x170
        EndPoint *m_pConnectedEndPoint; // 0x174
        qList<ConnectionData> m_lstConnectionData; // 0x178
        StationURL m_urlUnused; // 0x180
        qResult m_rResult; // 0x1e4
        StationURL m_oURL; // 0x1f0
        Buffer *m_pBuffer; // 0x254
        EndPoint::pfCompletion m_pfCallback; // 0x258
        UserContext m_oContext; // 0x25c
        Time m_tTimeout; // 0x260
        unsigned int m_uiPID; // 0x268
        unsigned int m_uiCID; // 0x26c
    };

    JobConnectSecureEndPoint::JobConnectSecureEndPoint(
        SecureEndPoint *pEndPoint, const StationURL *pURL, Buffer *pConnectData,
        Buffer *pBuffer, EndPoint::pfCompletion pfCallback, const UserContext &oContext,
        unsigned int uiTimeout
    )
        : Job(DebugString()), m_pEndPoint(pEndPoint), m_oURL(*pURL), m_pBuffer(pBuffer),
          m_pfCallback(pfCallback), m_oContext(oContext) {
        m_eStep = 0;
        m_tTimeout = Time::FromMilliseconds(uiTimeout);
        m_rResult = qResult(0x10001);
        m_pTicket = 0;
        m_pRequest = 0;
        m_pResponse = 0;
        m_uiPID = 0;
        m_uiCID = 0;
        m_pConnectedEndPoint = 0;
    }

    JobConnectSecureEndPoint::~JobConnectSecureEndPoint() {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pTicket != 0) {
            SecureStream *pStream = m_pEndPoint->m_pStream;
            pStream->GetAuthenticationClient()->GetTicketManager()->ReleaseTicket(m_pTicket);
            m_pTicket = 0;
        }
        if (m_pRequest != 0) {
            delete m_pRequest;
            m_pRequest = 0;
        }
        if (m_pResponse != 0) {
            delete m_pResponse;
            m_pResponse = 0;
        }
    }

    inline void JobConnectSecureEndPoint::ExecuteStep() {
        switch (m_eStep) {
        case 0:
            ParseURL();
            m_eStep = 2;
            break;
        case 2:
            CheckConnectionData();
            break;
        case 1:
            RequestConnectionData();
            break;
        case 3:
            PerformConnect();
            break;
        case 4:
            CompleteConnection();
            break;
        }
    }

    void JobConnectSecureEndPoint::Execute() {
        Trace(0x4000);
        do {
            ExecuteStep();
            Trace(0x4000);
        } while (GetState() == Running);
        if (GetState() == Complete) {
        }
        if (GetState() == Complete && m_pfCallback != 0) {
            m_pfCallback(m_pEndPoint, m_rResult, &m_oContext);
        }
    }

    void JobConnectSecureEndPoint::ParseURL() {
        m_uiCID = m_oURL.GetConnectionID();
        m_uiPID = m_oURL.GetPrincipalID();
        bool bValid = m_oURL.GetAddress().GetLength() != 0 && m_oURL.GetPortNumber() > 0
            && m_oURL.GetStreamID() > 0;
        if (bValid) {
            StationURL url;
            url.SetInetAddress(m_oURL.GetInetAddress());
            url.SetStreamID(m_oURL.GetStreamID());
            ConnectionData oData;
            oData.m_urlRegularProtocols = url;
            oData.m_uiConnectionID = m_uiCID;
            m_lstConnectionData.push_back(oData);
        }
        if (m_uiPID == 0 && m_uiCID == 0) {
            m_rResult = qResult(0x80050003);
            m_eStep = 5;
            SetToComplete();
        }
    }

    void JobConnectSecureEndPoint::RequestCompletionCallback(
        CallContext *pContext, const UserContext *pUserContext
    ) {
        JobConnectSecureEndPoint *pJob = (JobConnectSecureEndPoint *)pUserContext->GetPointer();
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (pJob->GetState() == Suspended) {
            pJob->Resume();
        }
    }

    void JobConnectSecureEndPoint::CheckConnectionData() {
        if (IsConnectionDataAvailable()) {
            m_eStep = 3;
        } else {
            m_eStep = 1;
        }
    }

    void JobConnectSecureEndPoint::RequestConnectionData() {
        SecureStream *pStream = m_pEndPoint->m_pStream;
        unsigned int uiPID = pStream->GetCredentials()->m_uiPID;
        if (m_uiPID != 0 && m_pTicket == 0
            && m_oTicketContext.GetState() == CallContext::CallInit) {
            m_oTicketContext.RegisterCompletionCallback(
                RequestCompletionCallback, UserContext(this), true
            );
            if (!pStream->GetAuthenticationClient()->GetTicketManager()->AcquireTicket(
                    &m_oTicketContext, uiPID, m_uiPID, &m_pTicket
                )) {
                m_rResult = qResult(0x8001000D);
                m_eStep = 5;
                SetToComplete();
                return;
            }
        }
        if (m_lstConnectionData.empty()
            && m_oDataContext.GetState() == CallContext::CallInit) {
            m_oDataContext.RegisterCompletionCallback(
                RequestCompletionCallback, UserContext(this), true
            );
            if (!pStream->GetSecureConnectionClient()->RequestConnectionData(
                    &m_oDataContext, m_uiCID, m_uiPID, &m_lstConnectionData
                )) {
                m_rResult = qResult(0x8001000D);
                m_eStep = 5;
                SetToComplete();
                return;
            }
        }
        if (m_oTicketContext.GetState() == CallContext::CallPending
            || m_oDataContext.GetState() == CallContext::CallPending) {
            m_eStep = 2;
            SetToSuspended();
            return;
        } else {
            m_rResult = qResult(0x8001000D);
            m_eStep = 5;
            SetToComplete();
        }
    }

    bool JobConnectSecureEndPoint::IsConnectionDataAvailable() {
        if (m_uiPID == 0) {
            return false;
        }
        if (m_lstConnectionData.empty()) {
            return false;
        }
        if (m_pTicket == 0) {
            return false;
        }
        return true;
    }

    void JobConnectSecureEndPoint::PerformConnect() {
        if (m_lstConnectionData.empty()) {
            m_rResult = qResult(0x80050003);
            m_eStep = 5;
            SetToComplete();
            return;
        }
        SecureStream *pStream = m_pEndPoint->GetStream();
        Credentials *pCred = pStream->GetCredentials();
        StreamSettings *pConnectionSettings = pCred->GetSettings();
        ConnectionManager *pConnectionManager = pConnectionSettings->GetConnectionManager();
        PrepareConnectionRequest();
        if (m_pResponse != 0) {
            delete m_pResponse;
        }
#line 311
        m_pResponse = new (__FILE__, __LINE__) BitStream;
        qList<StationURL> lstStationURLs;
        GetConnectionURLs(lstStationURLs);
        m_oConnectContext.RegisterCompletionCallback(
            ConnectCompletionCallback, UserContext(this), false
        );
        if (!pConnectionManager->ConnectImpl(
                &m_oConnectContext, m_pRequest->GetBuffer(), m_pResponse->GetBuffer(), lstStationURLs,
                &m_pConnectedEndPoint, Time::ToMilliseconds(m_tTimeout)
            )) {
            m_rResult = qResult(0x8001000D);
            m_eStep = 4;
            SetToRunning();
        } else {
            m_eStep = 4;
            SetToSuspended();
        }
    }

    inline void JobConnectSecureEndPoint::PrepareConnectionRequest() {
        SecureStream *pStream = m_pEndPoint->m_pStream;
        if (m_pRequest != 0) {
            delete m_pRequest;
        }
#line 267
        m_pRequest = new (__FILE__, __LINE__) BitStream;
        KerberosAuthentication::PrepareConnectionRequest(
            m_pRequest, pStream->GetAuthenticationClient(), m_pTicket, &m_uiSessionKey
        );
    }

    inline void JobConnectSecureEndPoint::GetConnectionURLs(qList<StationURL> &lstURLs) {
        qList<ConnectionData>::iterator it = m_lstConnectionData.begin();
        while (it != m_lstConnectionData.end()) {
            StationURL url((*it).m_urlRegularProtocols);
            url.SetConnectionID((*it).m_uiConnectionID);
            lstURLs.push_back(url);
            it++;
        }
    }

    void JobConnectSecureEndPoint::ConnectCompletionCallback(
        CallContext *pContext, const UserContext *pUserContext
    ) {
        JobConnectSecureEndPoint *pJob = (JobConnectSecureEndPoint *)pUserContext->GetPointer();
        pJob->ProcessConnectionResult(pContext);
    }

    void JobConnectSecureEndPoint::ProcessConnectionResult(CallContext *pContext) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (pContext->GetState() == CallContext::CallSuccess) {
            m_pEndPoint->SetAssociatedEndPoint(m_pConnectedEndPoint);
            m_pConnectedEndPoint->SetPrincipalID(m_uiPID);
            m_pEndPoint->SetPrincipalID(m_uiPID);
            m_pEndPoint->SetConnectionID(m_pConnectedEndPoint->GetConnectionID());
        } else {
            m_rResult = qResult(0x80050001);
        }
        Resume();
    }

    void JobConnectSecureEndPoint::CompleteConnection() {
        if (m_rResult.Equals(true)) {
            m_pResponse->AdjustLength();
            if (!KerberosAuthentication::ValidateConnectionResponse(m_pResponse, m_uiSessionKey)) {
                m_rResult = qResult(0x8005000A);
            }
        }
        m_eStep = 5;
        SetToComplete();
    }

    void JobConnectSecureEndPoint::Trace(unsigned int uiFlags) {
        if (GetStep() != 2) {
            return;
        } else {
            m_oTicketContext.Trace(uiFlags);
            m_oDataContext.Trace(uiFlags);
        }
    }

}
