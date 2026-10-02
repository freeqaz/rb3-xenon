// Quazal NetZ - .\ObjDupProtocol.cpp
//
// The retail TU is 0x82A937E0..0x82A97EF0: the ObjDupProtocol methods, the
// JobExecuteDelayedRMC job, and the container helpers it instantiates. It is
// built /Od /Ob1 with EH off, so every helper the classes below define in the
// class body is expanded in place (its `this` and return value spilled to
// stack temps), while the ones defined out of line are called.
//
// The surrounding NetZ classes are declared here only as far as this TU uses
// them; their members are defined in other TUs. At /Od the local NAMES set the
// stack layout (a walk over the scope's symbol hash buckets), so they were
// chosen to reproduce retail's frames.

namespace Quazal {

    typedef unsigned int size_t;

    class RootObject {
    public:
        static void *operator new(size_t, const char *, unsigned int);
        static void operator delete(void *);
    };

    namespace PseudoSingleton {
        unsigned int GetCurrentContext();
        void SetCurrentContext(unsigned int);
    }

    // Per-context component table; the protocol reaches its siblings through it.
    class InstanceTable {
    public:
        void *GetInstance(unsigned int type, unsigned int context);
    };
    extern InstanceTable s_oInstanceTable;

    class String : public RootObject {
    public:
        String();
        ~String();
        String &operator=(const char *);
        void Format(const char *, ...);
        const char *CStr() const { return m_szContent; }

        char *m_szContent;
    };

    struct qResult {
        int m_iCode;
        int m_iLine;
        const char *m_szFile;
    };

    class Time {
    public:
        Time();
        unsigned long long m_ullValue;
    };

    class DOHandle {
    public:
        DOHandle() : m_uiValue(0) {}
        DOHandle(unsigned int uiValue) : m_uiValue(uiValue) {}
        DOHandle(const DOHandle &o) : m_uiValue(o.m_uiValue) {}
        ~DOHandle() {}
        bool operator!=(const DOHandle &o) const { return m_uiValue != o.m_uiValue; }
        operator unsigned int() const { return m_uiValue; }

        void SaveTo(class ByteStream *, bool) const;
        const char *GetClassName() const;
        const char *GetDataSetName(unsigned char) const;

        unsigned int m_uiValue;
    };

    class Buffer : public RootObject {
    public:
        Buffer(unsigned int uiSize);
        virtual ~Buffer();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        Buffer &operator=(const Buffer &);

        char m_pad4[0x10];
    };

    class ByteStream : public RootObject {
    public:
        void Append(const void *, unsigned int, bool);
        void Extract(void *, unsigned int, bool);
        ByteStream &operator<<(const DOHandle &);
        ByteStream &operator>>(DOHandle &);
        ByteStream &operator<<(const bool &);
        ByteStream &operator>>(bool &);
        void AppendString(const char *, unsigned int);
        void ExtractString(char *, unsigned int);
    };

    class EndPoint;

    class Message : public ByteStream {
    public:
        Message();
        Message(Buffer *);
        ~Message();
        bool IsValid();
        void SetReceptionTime(const Time &);
        void SetSourceEndPoint(EndPoint *);
        void Rewind();
        Buffer *GetBuffer();
        unsigned int GetPayloadSize();
        void ExtractMessage(Message *);

        unsigned int GetSourceStation() { return m_uiSourceStation; }

        char m_pad0[0x24];
        unsigned int m_uiSourceStation; // 0x24
        char m_pad28[4];
        EndPoint *m_pSourceEndPoint; // 0x2C
    };

    class EndPoint {
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
        virtual void _v15();
        virtual void _v16();
        virtual void _v17();
        virtual void _v18();
        virtual void _v19();
        virtual void _v20();
        virtual void _v21();
        virtual qResult Send(Buffer *, unsigned int);

        unsigned int GetStationID() { return m_uiStationID; }

        char m_pad4[0x70];
        unsigned int m_uiStationID; // 0x74
    };

    class BandwidthMonitor {
    public:
        bool AcceptIncoming(Buffer *);
        void CountOutgoing(Buffer *);
    };

    class DOProtocolContext {
    public:
        DOProtocolContext();
        ~DOProtocolContext();
        void Enter();

        char m_pad[0x30];
    };

    class DOProtocolContextList {
    public:
        void Add(DOProtocolContext *);
        void Remove(DOProtocolContext *);
    };

    // Registers a DOProtocol context for the scope of one message.
    class ScopedDOProtocolContext {
    public:
        ScopedDOProtocolContext(DOProtocolContextList *pList, DOProtocolContext *pContext)
            : m_pList(pList), m_pContext(pContext) {
            m_pContext->Enter();
            m_pList->Add(m_pContext);
        }
        ~ScopedDOProtocolContext() { m_pList->Remove(m_pContext); }

        DOProtocolContextList *m_pList;
        DOProtocolContext *m_pContext;
    };

    class DOProtocolHandler {
    public:
        void Process(void *, Message *, Message *, unsigned int *, EndPoint *);

        char m_pad0[0x48];
        DOProtocolContextList m_oContexts; // 0x48
    };

    class ParticipationManager {
    public:
        void ProcessParticipants(Message *, bool);
        void ProcessJoinAccepted(unsigned char, DOHandle, DOHandle);
        void ProcessRedirect(class StationURL &);
        void ProcessJoinRefused(unsigned char, unsigned int);
    };

    class Protocol;
    class ObjDupProtocol;

    // The type-4 component: the NetZ core object of the current context.
    class NetZCore {
    public:
        static NetZCore *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (NetZCore *)s_oInstanceTable.GetInstance(4, uiContext);
        }
        BandwidthMonitor *GetBandwidthMonitor() { return m_pBandwidthMonitor; }
        ObjDupProtocol *GetObjDupProtocol() { return m_pObjDupProtocol; }

        char m_pad0[0x18];
        BandwidthMonitor *m_pBandwidthMonitor; // 0x18
        char m_pad1c[0xC];
        ObjDupProtocol *m_pObjDupProtocol; // 0x28
        char m_pad2c[4];
        void *m_p30; // 0x30
        DOProtocolHandler *m_pDOProtocolHandler; // 0x34
        char m_pad38[4];
        class MessageSigner *m_pMessageSigner; // 0x3C
    };

    class Session {
    public:
        static Session *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (Session *)s_oInstanceTable.GetInstance(4, uiContext);
        }
        bool IsTerminating();
        bool IsJoining();
    };

    class NetZ {
    public:
        static NetZ *GetInstance();
        bool IsTerminating();
    };

    class Job : public RootObject {
    public:
        void Postpone();
    };

    class JobProcessMessage : public Job {
    public:
        JobProcessMessage(ObjDupProtocol *, Message *);
    };

    class JobProcessJoinRequest : public Job {
    public:
        JobProcessJoinRequest(EndPoint *, class StationInfo *, class JoinRequestData *);
    };

    class Scheduler {
    public:
        void Queue(Job *, bool);
    };

    // The type-3 component; it owns the scheduler.
    class SchedulerHolder {
    public:
        static SchedulerHolder *GetInstance();
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    inline Scheduler *GetScheduler() {
        SchedulerHolder *pHolder = SchedulerHolder::GetInstance();
        if (pHolder == 0) {
            return 0;
        }
        return pHolder->GetScheduler();
    }

    class TraceLog {
    public:
        static TraceLog *GetInstance();
        bool TraceIsOn(unsigned int uiFlag) { return (m_uiFlags & uiFlag) == uiFlag; }

        char m_pad0[0x10];
        unsigned int m_uiFlags; // 0x10
    };

    class DuplicatedObject;

    class Station {
    public:
        static bool IsLocalStationMaster();
        static Station *GetLocalStation();
        bool IsDisconnected();
        const char *GetStationURL(unsigned int);
        unsigned int GetHandle() { return m_uiHandle; }

        char m_pad0[0x14];
        unsigned int m_uiHandle; // 0x14
    };

    class DORef {
    public:
        DORef(DOHandle);
        ~DORef();
        bool IsValid();

        DuplicatedObject *m_pObject;
    };

    class StationRef : public DORef {
    public:
        StationRef(DOHandle hStation) : DORef(hStation) {}
        bool IsValid();
        Station *operator->() {
            if (!IsValid()) {
                return 0;
            }
            return (Station *)m_pObject;
        }
    };

    class StationProxy {
    public:
        StationProxy();
        ~StationProxy();
        void FaultDetection(EndPoint *);
        void PrepareParticipantsMessage(Message *);
        void AddPretendant(EndPoint *, Message *);
    };

    class PendingStation {
    public:
        void Queue(Job *);
    };

    class StationTable {
    public:
        static StationTable *GetInstance();
        int GetStationState(DOHandle);
        PendingStation *GetPendingStation(DOHandle);
        void ExtractStations(Message *);
    };

    class CallContextRegister {
    public:
        CallContextRegister();
        virtual ~CallContextRegister();
        virtual void Register(void *);

        char m_pad4[0x24];
    };

    class Data : public RootObject {
    public:
        ~Data();
    };

    class StationInfoFactory;
    StationInfoFactory *GetStationInfoFactory(unsigned int);

    class StationInfo : public Data {
    public:
        StationInfo(StationInfoFactory *);
        void Write(Message *);
        void Read(Message *, bool, String *);

        char m_pad[0x1C];
    };

    class JoinRequestData : public Data {
    public:
        JoinRequestData();
        void Read(Message *);

        String m_strA;
        String m_strB;
        char m_pad8[8];
    };

    class MessageSigner {
    public:
        void Sign(Message *, Time, bool);
    };

    class StationURL {
    public:
        StationURL(const char *);
        ~StationURL();

        char m_pad[0x6C];
    };

    class JoinResponseObserver {
    public:
        virtual void _v0();
        virtual void _v1();
        virtual void OnJoinResponse(Message *);
    };

    class Protocol : public RootObject {
    public:
        Protocol() {}
        virtual ~Protocol() {}
        virtual void Receive(EndPoint *, Buffer *) = 0;
        virtual void FaultDetection(EndPoint *, unsigned int) = 0;
        virtual void PeerDisconnected(EndPoint *) = 0;
    };

    namespace StationManager {
        void FaultDetected(DOHandle, unsigned int);
        DOHandle GetLocalStationHandle();
    }

    class ObjDupProtocol : public Protocol {
    public:
        ObjDupProtocol();
        virtual ~ObjDupProtocol();
        virtual void Receive(EndPoint *, Buffer *);
        virtual void FaultDetection(EndPoint *, unsigned int);
        virtual void PeerDisconnected(EndPoint *);

        static void AddMessageType(Message *, unsigned char);
        Message *CreateMessage(unsigned char);
        void ReleaseMessage(Message *);
        qResult Send(EndPoint *, Message *, unsigned int);
        void QueueMessageFromLocalStation(Message *);
        void QueueMessage(Message *, unsigned int, EndPoint *, bool);
        void TraceMessage(Message *, unsigned char);
        bool Dispatch(JobProcessMessage *, Message *);
        bool ShouldDispatch(Message *);
        unsigned char ExtractMessageType(Message *);
        bool CheckSourceStation(JobProcessMessage *, Message *, unsigned char);

        Message *CreateDOProtocolMessage();
        bool ProcessDOProtocolMessage(Message *);
        Message *CreateGetParticipantsRequest();
        bool ParseGetParticipantsRequest(Message *, bool, bool, String *);
        void ProcessGetParticipantsRequest(EndPoint *, Message *);
        Message *CreateGetParticipantsResponse();
        bool ParseGetParticipantsResponse(Message *, bool, bool, String *);
        void ProcessGetParticipantsResponse(Message *);
        Message *CreateJoinRequest();
        bool ParseJoinRequestMessage(Message *, bool, bool, String *);
        void ProcessJoinRequest(EndPoint *, StationInfo *, JoinRequestData *);
        Message *CreateJoinResponse(unsigned char);
        bool ParseJoinResponseMessage(Message *, bool, bool, String *);
        void ProcessJoinResponse(Message *, unsigned char &);
        static JoinResponseObserver *GetJoinResponseObserver();

        bool ParseSpecificMessage(Message *, unsigned int, bool, String *);

        static ObjDupProtocol *GetInstance();
        bool IsListeningOnWellKnown() const;
        bool IsListeningOnAnyPort() const;
        bool IsListening(unsigned short *) const;

        bool m_bListeningOnAnyPort; // 0x4
        bool m_bListeningOnWellKnown; // 0x5
        CallContextRegister m_oCallContextRegister; // 0x8
        ParticipationManager *m_pParticipationManager; // 0x30
        StationProxy m_oStationProxy; // 0x34
        char m_pad35[0xB];
        unsigned int m_uiContext; // 0x40
        unsigned int m_uiFlags; // 0x44
        unsigned int m_uiMaxPendingMessages; // 0x48
        unsigned short m_usWellKnownPort; // 0x4C
        unsigned short m_usAnyPort; // 0x4E
    };

    // A per-context global: slot 0 lives inline, the others in a table.
    template <class T>
    class PseudoGlobalVariable {
    public:
        T &GetRef() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            if (uiContext == 0) {
                return m_tDefault;
            }
            return m_ptValues[uiContext];
        }

        char m_pad0[8];
        T *m_ptValues; // 0x8
        T m_tDefault; // 0xC
    };

    extern PseudoGlobalVariable<JoinResponseObserver *> s_pJoinResponseObserver;

    ObjDupProtocol::ObjDupProtocol() {
        m_bListeningOnAnyPort = false;
        m_bListeningOnWellKnown = false;
        m_uiContext = PseudoSingleton::GetCurrentContext();
        m_uiFlags = 0;
        m_uiMaxPendingMessages = 7;
        m_pParticipationManager = 0;
        m_usWellKnownPort = 0;
        m_usAnyPort = 0;
    }

    ObjDupProtocol::~ObjDupProtocol() {}

    void ObjDupProtocol::AddMessageType(Message *pMsg, unsigned char ucType) {
        pMsg->Append(&ucType, 1, true);
    }

    void ObjDupProtocol::FaultDetection(EndPoint *pEndPoint, unsigned int uiReason) {
        PseudoSingleton::SetCurrentContext(m_uiContext);
        m_oStationProxy.FaultDetection(pEndPoint);
        DOHandle hStation = pEndPoint->GetStationID();
        if (hStation != DOHandle()) {
            StationManager::FaultDetected(hStation, uiReason);
        }
    }

    void ObjDupProtocol::PeerDisconnected(EndPoint *pEndPoint) {
        PseudoSingleton::SetCurrentContext(m_uiContext);
        m_oStationProxy.FaultDetection(pEndPoint);
    }

    Message *ObjDupProtocol::CreateMessage(unsigned char ucType) {
        Message *pMsg = new (__FILE__, 0x7F) Message();
        AddMessageType(pMsg, ucType);
        return pMsg;
    }

    void ObjDupProtocol::ReleaseMessage(Message *pMsg) { delete pMsg; }

    void ObjDupProtocol::Receive(EndPoint *pEndPoint, Buffer *pBuffer) {
        PseudoSingleton::SetCurrentContext(m_uiContext);
        if (NetZCore::GetInstance()->GetBandwidthMonitor()->AcceptIncoming(pBuffer)) {
            Message *pMsg = new (__FILE__, 0x8C) Message(pBuffer);
            if (pMsg->IsValid()) {
                QueueMessage(
                    pMsg,
                    pEndPoint != 0 ? DOHandle(pEndPoint->GetStationID())
                                   : StationManager::GetLocalStationHandle(),
                    pEndPoint,
                    false
                );
            } else {
                delete pMsg;
            }
        }
    }

    qResult ObjDupProtocol::Send(EndPoint *pEndPoint, Message *pMsg, unsigned int uiFlags) {
        Buffer *pBuffer = new (__FILE__, 0x9E) Buffer(0x400);
        *pBuffer = *pMsg->GetBuffer();
        NetZCore::GetInstance()->GetBandwidthMonitor()->CountOutgoing(pBuffer);
        qResult r = pEndPoint->Send(pBuffer, uiFlags);
        pBuffer->ReleaseRef();
        return r;
    }

    void ObjDupProtocol::QueueMessageFromLocalStation(Message *pMsg) {
        Buffer *pBuffer = new (__FILE__, 0xA7) Buffer(0x400);
        *pBuffer = *pMsg->GetBuffer();
        NetZCore::GetInstance()->GetBandwidthMonitor()->CountOutgoing(pBuffer);
        Receive(0, pBuffer);
        pBuffer->ReleaseRef();
    }

    void ObjDupProtocol::QueueMessage(
        Message *pMsg, unsigned int uiSourceStation, EndPoint *pEndPoint, bool bUrgent
    ) {
        Time tNow;
        pMsg->SetReceptionTime(tNow);
        pMsg->m_uiSourceStation = uiSourceStation;
        pMsg->SetSourceEndPoint(pEndPoint);
        GetScheduler()->Queue(new (__FILE__, 0xB5) JobProcessMessage(this, pMsg), bUrgent);
    }

    void ObjDupProtocol::TraceMessage(Message *pMsg, unsigned char ucType) {
        String strTrace;
        ParseSpecificMessage(pMsg, ucType, false, &strTrace);
        pMsg->Rewind();
        unsigned char ucSkip = 0;
        pMsg->Extract(&ucSkip, 1, true);
    }

    bool ObjDupProtocol::Dispatch(JobProcessMessage *pJob, Message *pMsg) {
        bool bResult = false;
        PseudoSingleton::SetCurrentContext(m_uiContext);
        if (ShouldDispatch(pMsg)) {
            unsigned char ucType = ExtractMessageType(pMsg);
            if (TraceLog::GetInstance()->TraceIsOn(0x200)) {
                TraceMessage(pMsg, ucType);
            }
            if (CheckSourceStation(pJob, pMsg, ucType)) {
                bool bParsed = ParseSpecificMessage(pMsg, ucType, true, 0);
                bResult = !bParsed;
            }
        }
        return bResult;
    }

    bool ObjDupProtocol::ShouldDispatch(Message *pMsg) {
        bool bResult = true;
        if (Session::GetInstance()->IsTerminating()) {
            bResult = false;
        } else if (pMsg->GetSourceStation() != 0) {
            DOHandle hStation = pMsg->GetSourceStation();
            StationRef refStation(hStation);
            if (refStation.IsValid()) {
                if (refStation->IsDisconnected()) {
                    bResult = false;
                }
            }
        }
        return bResult;
    }

    unsigned char ObjDupProtocol::ExtractMessageType(Message *pMsg) {
        unsigned char ucType = 0;
        pMsg->Extract(&ucType, 1, true);
        if (Session::GetInstance()->IsJoining()) {
            switch (ucType) {
            case 0x08:
            case 0x0B:
            case 0x0F:
            case 0xFF:
                break;
            case 0x0A:
                break;
            case 0x10:
                break;
            default:
                ucType = 0xFE;
            }
        }
        return ucType;
    }

    bool ObjDupProtocol::CheckSourceStation(
        JobProcessMessage *pJob, Message *pMsg, unsigned char ucType
    ) {
        bool bResult = true;
        bool bCheck = true;
        switch (ucType) {
        case 0x00:
        case 0x01:
        case 0x0F:
        case 0x14:
        case 0x15:
        case 0xFE:
        case 0xFF:
            bCheck = false;
        }
        if (pMsg->GetSourceStation() != 0) {
            DOHandle hStation = pMsg->GetSourceStation();
            if (bCheck) {
                if (StationTable::GetInstance()->GetStationState(hStation) == 2) {
                    PendingStation *pStation =
                        StationTable::GetInstance()->GetPendingStation(hStation);
                    pMsg->Rewind();
                    pJob->Postpone();
                    pStation->Queue(pJob);
                    bResult = false;
                }
            }
        }
        return bResult;
    }

    Message *ObjDupProtocol::CreateDOProtocolMessage() { return CreateMessage(0x10); }

    bool ObjDupProtocol::ProcessDOProtocolMessage(Message *pMsg) {
        EndPoint *pEndPoint = pMsg->m_pSourceEndPoint;
        Message oReply;
        DOProtocolHandler *pHandler = NetZCore::GetInstance()->m_pDOProtocolHandler;
        DOProtocolContext oContext;
        ScopedDOProtocolContext oScope(&pHandler->m_oContexts, &oContext);
        unsigned int uiResult;
        pHandler->Process(NetZCore::GetInstance()->m_p30, pMsg, &oReply, &uiResult, pEndPoint);
        return true;
    }

    Message *ObjDupProtocol::CreateGetParticipantsRequest() {
        Message *pMsg = CreateMessage(0x14);
        return pMsg;
    }

    bool ObjDupProtocol::ParseGetParticipantsRequest(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        EndPoint *pEndPoint = pMsg->m_pSourceEndPoint;
        if (bTrace) {
            *pTrace = "GET_PARTICIPANTS_REQUEST message.";
        }
        if (bProcess) {
            ProcessGetParticipantsRequest(pEndPoint, pMsg);
        }
        return true;
    }

    void ObjDupProtocol::ProcessGetParticipantsRequest(EndPoint *pEndPoint, Message *pMsg) {
        Message *pReply = CreateGetParticipantsResponse();
        if (Station::IsLocalStationMaster()) {
            *pReply << true;
            m_oStationProxy.PrepareParticipantsMessage(pReply);
            m_oStationProxy.AddPretendant(pEndPoint, pMsg);
        } else {
            *pReply << false;
        }
        Send(pEndPoint, pReply, 1);
        delete pReply;
    }

    Message *ObjDupProtocol::CreateGetParticipantsResponse() {
        Message *pMsg = CreateMessage(0x15);
        return pMsg;
    }

    bool ObjDupProtocol::ParseGetParticipantsResponse(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        if (bTrace) {
            *pTrace = "GET_PARTICIPANTS_RESPONSE message.";
        }
        if (bProcess) {
            ProcessGetParticipantsResponse(pMsg);
        }
        return true;
    }

    void ObjDupProtocol::ProcessGetParticipantsResponse(Message *pMsg) {
        if (m_pParticipationManager == 0) {
            return;
        }
        bool bMaster;
        *pMsg >> bMaster;
        m_pParticipationManager->ProcessParticipants(pMsg, bMaster);
    }

    Message *ObjDupProtocol::CreateJoinRequest() {
        Message *pMsg = CreateMessage(0);
        StationInfo oInfo(GetStationInfoFactory(1));
        oInfo.Write(pMsg);
        NetZCore::GetInstance()->m_pMessageSigner->Sign(pMsg, Time(), true);
        return pMsg;
    }

    bool ObjDupProtocol::ParseJoinRequestMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        EndPoint *pEndPoint = pMsg->m_pSourceEndPoint;
        String strTrace;
        StationInfo *pInfo = new (__FILE__, 0x1AA) StationInfo(GetStationInfoFactory(1));
        pInfo->Read(pMsg, bTrace, &strTrace);
        JoinRequestData *pData = new (__FILE__, 0x1AD) JoinRequestData();
        pData->Read(pMsg);
        if (bTrace) {
            pTrace->Format("JOIN_REQUEST message. %s", strTrace.CStr());
        }
        if (bProcess) {
            ProcessJoinRequest(pEndPoint, pInfo, pData);
        } else {
            delete pData;
            delete pInfo;
        }
        return true;
    }

    void ObjDupProtocol::ProcessJoinRequest(
        EndPoint *pEndPoint, StationInfo *pInfo, JoinRequestData *pData
    ) {
        JobProcessJoinRequest *pJob =
            new (__FILE__, 0x1BD) JobProcessJoinRequest(pEndPoint, pInfo, pData);
        GetScheduler()->Queue(pJob, false);
    }

    Message *ObjDupProtocol::CreateJoinResponse(unsigned char ucResponse) {
        Message *pMsg = CreateMessage(1);
        pMsg->Append(&ucResponse, 1, true);
        if (ucResponse == 2) {
            StationRef refStation(Station::GetLocalStation()->GetHandle());
            pMsg->AppendString(refStation->GetStationURL(0), 0x100);
        }
        return pMsg;
    }

    bool ObjDupProtocol::ParseJoinResponseMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned char ucResponse;
        pMsg->Extract(&ucResponse, 1, true);
        if (bTrace) {
            pTrace->Format("JOIN_RESPONSE message. Response is %d", ucResponse);
        }
        if (bProcess) {
            ProcessJoinResponse(pMsg, ucResponse);
        }
        return true;
    }

    void ObjDupProtocol::ProcessJoinResponse(Message *pMsg, unsigned char &ucResponse) {
        if (m_pParticipationManager == 0) {
            return;
        }
        if (NetZ::GetInstance()->IsTerminating()) {
            return;
        }
        if (ucResponse == 1) {
            DOHandle hMaster;
            DOHandle hStation;
            *pMsg >> hMaster;
            *pMsg >> hStation;
            StationTable::GetInstance()->ExtractStations(pMsg);
            if ((m_uiFlags & 4) == 4) {
                GetJoinResponseObserver()->OnJoinResponse(pMsg);
            }
            m_pParticipationManager->ProcessJoinAccepted(ucResponse, hMaster, hStation);
        } else {
            if (ucResponse == 2) {
                char szURL[0x100];
                pMsg->ExtractString(szURL, 0x100);
                StationURL oURL(szURL);
                m_pParticipationManager->ProcessRedirect(oURL);
            }
            unsigned int uiReason;
            pMsg->Extract(&uiReason, 4, true);
            m_pParticipationManager->ProcessJoinRefused(ucResponse, uiReason);
        }
    }

    JoinResponseObserver *ObjDupProtocol::GetJoinResponseObserver() {
        return s_pJoinResponseObserver.GetRef();
    }

}
