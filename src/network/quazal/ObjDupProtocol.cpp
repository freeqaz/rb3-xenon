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

#include "Core/InstanceControl.h"
#include "Core/PseudoSingleton.h"
#include "Platform/qStd.h"

namespace Quazal {

    void SetCurrentContextIfRequired(unsigned int);



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
        Time(unsigned long long ullValue) : m_ullValue(ullValue) {}
        unsigned long long m_ullValue;
    };

    class DOHandle {
    public:
        DOHandle() : m_uiValue(0) {}
        DOHandle(unsigned int uiValue) : m_uiValue(uiValue) {}
        DOHandle(const DOHandle &o) : m_uiValue(o.m_uiValue) {}
        ~DOHandle() {}
        bool operator!=(const DOHandle &o) const { return m_uiValue != o.m_uiValue; }
        bool operator<(const DOHandle &o) const { return m_uiValue < o.m_uiValue; }
        unsigned int GetClassID() const { return (m_uiValue & 0xFFC00000) >> 22; }
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

    // A read cursor into a ByteStream; retail builds one from the raw offset
    // and hands it back by value.
    class StreamPosition {
    public:
        StreamPosition(unsigned int uiPosition) : m_uiPosition(uiPosition) {}
        unsigned int m_uiPosition;
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
        void SetPosition(StreamPosition);

        template <class T>
        ByteStream &operator<<(const T &t) {
            Append(&t, sizeof(T), true);
            return *this;
        }
        template <class T>
        ByteStream &operator>>(T &t) {
            Extract(&t, sizeof(T), true);
            return *this;
        }
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
        EndPoint *GetSourceEndPoint() { return m_pSourceEndPoint; }
        unsigned int GetPosition() { return m_uiPosition; }

        char m_pad0[8];
        unsigned int m_uiPosition; // 0x8
        char m_padc[0x18];
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
        static NetZCore *GetInstance(unsigned int uiContext) {
            return (NetZCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static NetZCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        BandwidthMonitor *GetBandwidthMonitor() { return m_pBandwidthMonitor; }
        class MessageSigner *GetMessageSigner() { return m_pMessageSigner; }
        ObjDupProtocol *GetObjDupProtocol() { return m_pObjDupProtocol; }
        class Listener *GetListener() { return m_pListener; }

        char m_pad0[0x18];
        BandwidthMonitor *m_pBandwidthMonitor; // 0x18
        char m_pad1c[0xC];
        ObjDupProtocol *m_pObjDupProtocol; // 0x28
        char m_pad2c[4];
        void *m_p30; // 0x30
        DOProtocolHandler *m_pDOProtocolHandler; // 0x34
        char m_pad38[4];
        class MessageSigner *m_pMessageSigner; // 0x3C
        class Listener *m_pListener; // 0x40
    };

    // The type-4 instance (.\DOCore.cpp, 0x82AC0470..0x82AC1808).
    class DOCore {
    public:
        static DOCore *GetInstance(unsigned int uiContext) {
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static DOCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        bool IsTerminated() const;
        bool HasStartedTermination() const;
    };

    // Compiled to nothing in this build; only its address reaches Job's ctor.
    class DebugString {
    public:
        DebugString() {}
    };

    class Job : public RootObject {
    public:
        Job(const DebugString &);
        virtual ~Job();
        virtual void _v1();
        virtual void _v2();
        virtual void _v3();
        virtual void Execute() = 0;
        void Postpone();
        void SetResult(void *);

        char m_pad4[0x34];
    };

    class JobProcessMessage : public Job {
    public:
        JobProcessMessage(ObjDupProtocol *, Message *);
        virtual void Execute();

        char m_pad38[8];
    };

    class JobProcessJoinRequest : public Job {
    public:
        JobProcessJoinRequest(EndPoint *, class StationInfo *, class _DS_StationIdentification *);
        virtual void Execute();

        char m_pad38[0x58];
    };

    class Scheduler {
    public:
        void Queue(Job *, bool);

        char m_pad0[0x3C];
        CriticalSection m_csSystemLock; // 0x3C
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

    // The type-3 component; it owns the scheduler.
    class SchedulerHolder {
    public:
        static SchedulerHolder *GetInstance() {
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(
                    3, PseudoSingleton::GetCurrentContext()
                );
            return inst ? (SchedulerHolder *)inst->m_pDelegatorInstance : 0;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    inline Scheduler *GetScheduler() {
        SchedulerHolder *pHolder = SchedulerHolder::GetInstance();
        if (pHolder == 0) {
            return 0;
        } else {
            return pHolder->GetScheduler();
        }
    }

    class TraceLog {
    public:
        static TraceLog *GetInstance();
        bool TraceIsOn(unsigned int uiFlag) { return (m_uiFlags & uiFlag) == uiFlag; }

        char m_pad0[0x10];
        unsigned int m_uiFlags; // 0x10
    };

    class DuplicatedObject;

    class StationURL;

    // Walks the stations of the session.
    class StationSelection {
    public:
        StationSelection(bool, bool);
        ~StationSelection();
        void GotoStart();
        void Refresh();
        bool EndReached();
        class Station *Current();
        void Next(int);

        char m_pad[0x28];
    };

    class StationURLs {
    public:
        void SetURL(unsigned int, const char *);
        void AddURL(StationURL &);
    };

    class Station {
    public:
        static bool IsLocalStationMaster();
        static Station *GetLocalStation();
        bool IsDisconnected();
        const char *GetStationURL(unsigned int);
        unsigned int GetHandle() { return m_uiHandle; }
        void ProcessEOS();
        void SendMessage(Message *, bool);
        unsigned int GetURLCount();
        unsigned int GetID();
        DOHandle GetHandleValue();

        char m_pad0[0x14];
        unsigned int m_uiHandle; // 0x14
        char m_pad18[0x58];
        class StationURLs m_oURLs; // 0x70
    };

    class DORef {
    public:
        DORef(DOHandle);
        ~DORef();
        bool IsValid();

        DuplicatedObject *m_pObject;
    };

    // DORefTemplate<T>::IsValid is emitted once per T in the DuplicatedObject TU
    // (0x82A76568 Station, 0x82A76640 DuplicatedObject).
    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        bool IsValid() const;
        T *operator->() {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)m_pObject;
            }
        }
        T *Get() {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)m_pObject;
            }
        }
    };
    typedef DORefTemplate<Station> StationRef;

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

    class CallContext {
    public:
        virtual void _v0();
        virtual void AcquireRef();
        virtual void ReleaseRef();

    };

    // An RMC response hands its message to the context as the user context.
    class UserContext {
    public:
        UserContext(void *pPointer) : m_pPointer(pPointer) {}
        void *m_pPointer;
    };

    // The contexts in the protocol's call register are DO call contexts
    // (.\DOCallContext.cpp).
    class DOCallContext : public CallContext {
    public:
        enum _Outcome {
        };
        static const char *GetOutcomeString(_Outcome);
        void SignalResponse(UserContext);
        void SignalOutcome(DOHandle, _Outcome);
    };

    // The protocol's register of outstanding DO calls (retail's CallRegister,
    // whose methods are in the CallRegister TU). Its ID lookups are expanded
    // here, so this TU carries their out-of-line copies.
    class CallRegister {
    public:
        CallRegister();
        virtual ~CallRegister();
        virtual void Register(void *);

        DOCallContext *GetCallContextRef(unsigned short usCallID) {
            ScopedCS oCS(GetScheduler()->m_csSystemLock);
            DOCallContext *pContext = FindCallContext(usCallID);
            if (pContext != 0) {
                pContext->AcquireRef();
            }
            return pContext;
        }
        DOCallContext *FindCallContext(unsigned short usCallID) {
            qMap<unsigned short, DOCallContext *>::iterator it = m_mapCalls.find(usCallID);
            if (it != m_mapCalls.end()) {
                return it->second;
            } else {
                return 0;
            }
        }
        void CancelPendingCalls();

        qMap<unsigned short, DOCallContext *> m_mapCalls; // 0x4
        char m_pad20[8];
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

    // The joining station's identification: the StationIdentification dataset,
    // whose constructor and extractor StationDDL.cpp defines.
    class _DS_StationIdentification : public Data {
    public:
        _DS_StationIdentification();
        void ExtractFrom(Message *);

        String m_strIdentificationToken; // 0x0
        String m_strProcessName; // 0x4
        unsigned int m_uiProcessType; // 0x8
        unsigned int m_uiProductVersion; // 0xc
    };

    class MessageSigner {
    public:
        void Sign(Message *, Time, bool);
    };

    class JoinResponseObserver {
    public:
        virtual void _v0();
        virtual void _v1();
        virtual void OnJoinResponse(Message *);
    };

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
        virtual void _v15();
        virtual void _v16();
        virtual void _v17();
        virtual void _v18();
        virtual void CallMethod(DuplicatedObject *, unsigned short, Message *);
    };

    class DuplicatedObject {
    public:
        static void UpdateDatasets(Message *, DOHandle, unsigned char);
        static DOClass *GetDOClass(unsigned int);
        bool IsADuplicationMaster() const;
        bool RemoveFromStore(DOHandle, bool, bool);
        void FillDuplicaStationsList(qList<DOHandle> *);
        bool FlagIsSet(unsigned short usFlag) { return (m_usFlags & usFlag) == usFlag; }
        bool Fetch(DOHandle, DOHandle, bool, DOHandle);

        char m_pad0[0x20];
        unsigned short m_usFlags; // 0x20
        char m_pad22[0x26];
        DOHandle m_hHandle; // 0x48
    };

    typedef DORefTemplate<DuplicatedObject> DOCoreRef;

    class FetchRef : public DORef {
    public:
        FetchRef(DOHandle hObject) : DORef(hObject) {}
        bool IsValid();
        DuplicatedObject *operator->() {
            if (!IsValid()) {
                return 0;
            } else {
                return m_pObject;
            }
        }
    };

    class RMCContext {
    public:
        unsigned int GetTargetObject() { return m_uiTargetObject; }
        unsigned short GetCallID() { return m_usCallID; }
        unsigned int GetFlags() { return m_uiFlags; }
        unsigned int GetMethodID() { return m_uiMethodID; }
        unsigned short GetProtocolID() { return m_usProtocolID; }

        char m_pad0[8];
        unsigned int m_uiTargetObject; // 0x8
        char m_padc[0x44];
        unsigned short m_usCallID; // 0x50
        char m_pad52[2];
        unsigned int m_uiFlags; // 0x54
        char m_pad58[0x10];
        unsigned int m_uiMethodID; // 0x68
        char m_pad6c[0x34];
        unsigned short m_usProtocolID; // 0xA0
    };

    class CallMethodOperation : public RootObject {
    public:
        CallMethodOperation(
            unsigned short, DOHandle, unsigned int, DOHandle, unsigned short, Message *
        );
        virtual ~CallMethodOperation();
        void Prepare();
        bool PostponeOperation();
        void Execute();
        void Abort();
        unsigned short GetCallID() { return m_usCallID; }
        void *GetResult() { return m_pResult; }

        char m_pad4[0x28];
        void *m_pResult; // 0x2C
        unsigned short m_usCallID; // 0x30
        char m_pad32[0x1A];
    };
    class FetchContext {
    public:
        unsigned short GetCallID() { return m_usCallID; }
        unsigned int GetTargetObject() { return m_uiTargetObject; }

        char m_pad0[0x50];
        unsigned short m_usCallID; // 0x50
        char m_pad52[0x16];
        unsigned int m_uiTargetObject; // 0x68
    };

    class MigrationContext {
    public:
        DuplicatedObject *GetObject() { return m_pObject; }
        unsigned char GetFlags() { return m_ucFlags; }
        unsigned short GetCallID() { return m_usCallID; }

        char m_pad0[0x50];
        unsigned short m_usCallID; // 0x50
        char m_pad52[0xA];
        unsigned int m_uiTarget; // 0x5C
        char m_pad60[4];
        DuplicatedObject *m_pObject; // 0x64
        unsigned int m_uiMethod; // 0x68
        char m_pad6c[0x34];
        unsigned char m_ucFlags; // 0xA0
    };

    class ProtocolCallContext {
    public:
        ProtocolCallContext();
        ~ProtocolCallContext();
        void CallMigration(
            Message *,
            unsigned short *,
            const DOHandle &,
            unsigned int *,
            unsigned int *,
            unsigned char *,
            qList<DOHandle> *
        );

        char m_pad[0x68];
    };


    class StationURL {
    public:
        StationURL(const char *);
        ~StationURL();
        unsigned int GetType() const;
        const char *GetURL() const;

        char m_pad[0x6C];
    };

    class StationURLList {
    public:
        bool IsEmpty() {
            ScopedCS oCS(m_cs);
            return m_lstURLs.empty();
        }

        CriticalSection m_cs; // 0x0
        qList<StationURL> m_lstURLs; // 0x14
    };

    class Listener {
    public:
        virtual void _v0();
        virtual void _v1();
        virtual void _v2();
        virtual void Start(bool, bool);
        virtual void Stop();

        char m_pad4[0xC];
        bool m_bStarted; // 0x10
    };

    class PRUDPTransport {
    public:
        virtual void _v0();
        virtual void _v1();
        virtual bool Listen(unsigned short, unsigned short *, bool, bool);
        virtual void _v3();
        virtual void StopListening(unsigned short);
    };

    class NetZ {
    public:
        static NetZ *GetInstance();
        static int GetMode();
        bool IsTerminating();
        StationURLList *GetLocalURLs();

        char m_pad0[0x4C];
        PRUDPTransport *m_pTransport; // 0x4C
    };

    void *GetInstanceType1Delegator();
    unsigned short GetWellKnownPort();

    inline PRUDPTransport *GetTransport() {
        NetZ *pNetZ = (NetZ *)GetInstanceType1Delegator();
        if (pNetZ == 0) {
            return 0;
        } else {
            return pNetZ->m_pTransport;
        }
    }



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
        void ProcessJoinRequest(EndPoint *, StationInfo *, _DS_StationIdentification *);
        Message *CreateJoinResponse(unsigned char);
        bool ParseJoinResponseMessage(Message *, bool, bool, String *);
        void ProcessJoinResponse(Message *, unsigned char &);
        static JoinResponseObserver *GetJoinResponseObserver();
        Message *CreateUpdateMessage(unsigned int *, unsigned char *);
        bool ParseUpdateMessage(Message *, bool, bool, String *);
        Message *CreateDeleteMessage(DOHandle);
        bool ParseDeleteMessage(Message *, bool, bool, String *);
        void ProcessDeleteMessage(DOHandle, DOHandle);
        Message *CreateActionMessage(DOHandle *, unsigned short *);
        bool ParseActionMessage(Message *, bool, bool, String *);
        void ProcessActionMessage(Message *, DOCoreRef *, unsigned short *);
        Message *CreateRMCCallMessage(RMCContext *);
        bool ParseRMCCallMessage(Message *, bool, bool, String *);
        bool ProcessRMCCallMessage(
            Message *, unsigned short &, DOHandle &, unsigned int &, DOHandle &, unsigned short &
        );
        Message *CreateRMCResponseMessage(CallMethodOperation *);
        bool ParseRMCResponseMessage(Message *, bool, bool, String *);
        void ProcessRMCResponse(Message *, unsigned short *);
        Message *CreateFetchRequestMessage(FetchContext *);
        bool ParseFetchRequestMessage(Message *, bool, bool, String *);
        void ProcessFetchRequestMessage(DOHandle &, DOHandle &, unsigned short &);
        Message *CreateMigrationMessage(MigrationContext *);
        Message *CreateCallOutcomeMessage(unsigned short, int);
        bool ParseCallOutcomeMessage(Message *, bool, bool, String *);
        void ProcessCallOutcome(DOHandle, unsigned short, int);
        bool ProcessBundleMessage(Message *, bool, bool, String *);
        Message *CreateEOSMessage(DOHandle);
        void QueueEOS(DOHandle);
        bool ParseEOSMessage(Message *, bool, bool, String *);
        void ProcessEOS(const DOHandle &);
        bool ShouldGrabWellKnown();
        bool ListenOnWellKnown();
        bool StartToListen();
        bool ListenOnAnyPort();
        void StopToListen();
        bool AddLocalURLs(Station *);
        bool ParseMessage(Message *, bool, String *);

        bool ParseSpecificMessage(Message *, unsigned int, bool, String *);

        static ObjDupProtocol *GetInstance();
        bool IsListeningOnWellKnown() const;
        bool IsListeningOnAnyPort() const;
        bool IsListening(unsigned short *) const;

        bool m_bListeningOnAnyPort; // 0x4
        bool m_bListeningOnWellKnown; // 0x5
        CallRegister m_oCallRegister; // 0x8
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
            } else {
                return m_ptValues[uiContext];
            }
        }

        char m_pad0[8];
        T *m_ptValues; // 0x8
        T m_tDefault; // 0xC
    };

    extern PseudoGlobalVariable<JoinResponseObserver *> s_pJoinResponseObserver;

}

// Runs an RMC call that had to wait for its target object.
class JobExecuteDelayedRMC : public Quazal::Job {
public:
    JobExecuteDelayedRMC(Quazal::CallMethodOperation *pOperation, Quazal::Message *pMessage)
        : Quazal::Job(Quazal::DebugString()) {
        m_pOperation = pOperation;
        m_pMessage = pMessage;
    }
    virtual ~JobExecuteDelayedRMC() {
        delete m_pOperation;
        delete m_pMessage;
    }
    virtual void Execute() {
        Quazal::StreamPosition oPosition = m_pMessage->GetPosition();
        m_pOperation->Prepare();
        if (!m_pOperation->PostponeOperation()) {
            m_pOperation->Abort();
        } else {
            GetMessage()->SetPosition(oPosition);
            m_pOperation->Execute();
            SetResult(m_pOperation->GetResult());
        }
    }

    Quazal::Message *GetMessage() { return m_pMessage; }

    Quazal::CallMethodOperation *m_pOperation; // 0x38
    Quazal::Message *m_pMessage; // 0x3C
};

namespace Quazal {

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
        SetCurrentContextIfRequired(m_uiContext);
        m_oStationProxy.FaultDetection(pEndPoint);
        DOHandle hStation = pEndPoint->GetStationID();
        if (hStation != DOHandle()) {
            StationManager::FaultDetected(hStation, uiReason);
        }
    }

    void ObjDupProtocol::PeerDisconnected(EndPoint *pEndPoint) {
        SetCurrentContextIfRequired(m_uiContext);
        m_oStationProxy.FaultDetection(pEndPoint);
    }

    Message *ObjDupProtocol::CreateMessage(unsigned char ucType) {
        Message *pMsg = new (__FILE__, 0x7F) Message();
        AddMessageType(pMsg, ucType);
        return pMsg;
    }

    void ObjDupProtocol::ReleaseMessage(Message *pMsg) { delete pMsg; }

    void ObjDupProtocol::Receive(EndPoint *pEndPoint, Buffer *pBuffer) {
        SetCurrentContextIfRequired(m_uiContext);
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
        String strDesc;
        ParseSpecificMessage(pMsg, ucType, false, &strDesc);
        pMsg->Rewind();
        unsigned char ucSkip = 0;
        pMsg->Extract(&ucSkip, 1, true);
    }

    bool ObjDupProtocol::Dispatch(JobProcessMessage *pJob, Message *pMsg) {
        bool bResult = false;
        SetCurrentContextIfRequired(m_uiContext);
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
        if (DOCore::GetInstance()->IsTerminated()) {
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
        if (DOCore::GetInstance()->HasStartedTermination()) {
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
        StationInfo oStationInfo(GetStationInfoFactory(1));
        oStationInfo.Write(pMsg);
        NetZCore::GetInstance()->GetMessageSigner()->Sign(pMsg, Time(0), true);
        return pMsg;
    }

    bool ObjDupProtocol::ParseJoinRequestMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        EndPoint *pSourceEndPoint = pMsg->m_pSourceEndPoint;
        String strTrace;
        StationInfo *pInfo = new (__FILE__, 0x1AA) StationInfo(GetStationInfoFactory(1));
        pInfo->Read(pMsg, bTrace, &strTrace);
        _DS_StationIdentification *pData = new (__FILE__, 0x1AD) _DS_StationIdentification();
        pData->ExtractFrom(pMsg);
        if (bTrace) {
            pTrace->Format("JOIN_REQUEST message. %s", strTrace.CStr());
        }
        if (bProcess) {
            ProcessJoinRequest(pSourceEndPoint, pInfo, pData);
        } else {
            delete pData;
            delete pInfo;
        }
        return true;
    }

    void ObjDupProtocol::ProcessJoinRequest(
        EndPoint *pEndPoint, StationInfo *pInfo, _DS_StationIdentification *pData
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

    Message *ObjDupProtocol::CreateUpdateMessage(unsigned int *puiHandle, unsigned char *pucDataSet) {
        DOHandle hObject = *puiHandle;
        Message *pMsg = CreateMessage(2);
        pMsg->Append(puiHandle, 4, true);
        pMsg->Append(pucDataSet, 1, true);
        return pMsg;
    }

    bool ObjDupProtocol::ParseUpdateMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned int uiHandle;
        *pMsg >> uiHandle;
        DOHandle hObject = uiHandle;
        unsigned char ucDataSet;
        *pMsg >> ucDataSet;
        if (bTrace) {
            pTrace->Format(
                "UPDATE message for object %x (%s), dataset %d (%s)",
                hObject,
                hObject.GetClassName(),
                ucDataSet,
                hObject.GetDataSetName(ucDataSet)
            );
        }
        if (bProcess) {
            DuplicatedObject::UpdateDatasets(pMsg, uiHandle, ucDataSet);
        }
        return true;
    }

    Message *ObjDupProtocol::CreateDeleteMessage(DOHandle hObject) {
        Message *pMsg = CreateMessage(4);
        *pMsg << hObject;
        return pMsg;
    }

    bool ObjDupProtocol::ParseDeleteMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        DOHandle hObject;
        *pMsg >> hObject;
        if (bTrace) {
            pTrace->Format("DELETE message for object %s %x", hObject.GetClassName(), hObject);
        }
        if (bProcess) {
            ProcessDeleteMessage(hObject, pMsg->GetSourceStation());
        }
        return true;
    }

    void ObjDupProtocol::ProcessDeleteMessage(DOHandle hObject, DOHandle hSource) {
        DOCoreRef refSource(hSource);
        if (!refSource.IsValid()) {
            return;
        }
        if (!refSource->FlagIsSet(1)) {
            return;
        }
        if (refSource->IsADuplicationMaster()) {
            return;
        } else {
            refSource->RemoveFromStore(hObject, true, false);
        }
    }

    Message *ObjDupProtocol::CreateActionMessage(DOHandle *phObject, unsigned short *pusMethodID) {
        Message *pMsg = CreateMessage(5);
        phObject->SaveTo(pMsg, false);
        pMsg->Append(pusMethodID, 2, true);
        return pMsg;
    }

    bool ObjDupProtocol::ParseActionMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned int uiHandle;
        *pMsg >> uiHandle;
        DOHandle hObject = uiHandle;
        DOCoreRef refObject(uiHandle);
        if (refObject.Get() == 0) {
            bProcess = false;
        }
        unsigned short usMethodID;
        *pMsg >> usMethodID;
        if (bTrace) {
            pTrace->Format(
                "ACTION message for object %s %x. MethodID: %d",
                hObject.GetClassName(),
                hObject,
                usMethodID
            );
        }
        if (bProcess) {
            ProcessActionMessage(pMsg, &refObject, &usMethodID);
        }
        return true;
    }

    void ObjDupProtocol::ProcessActionMessage(
        Message *pMsg, DOCoreRef *pRefObject, unsigned short *pusMethodID
    ) {
        DuplicatedObject::GetDOClass((*pRefObject)->m_hHandle.GetClassID())
            ->CallMethod((*pRefObject).operator->(), *pusMethodID, pMsg);
    }

    Message *ObjDupProtocol::CreateRMCCallMessage(RMCContext *pContext) {
        m_oCallRegister.Register(pContext);
        Message *pMsg = CreateMessage(0xA);
        *pMsg << pContext->GetCallID();
        *pMsg << pContext->GetTargetObject();
        *pMsg << pContext->GetFlags();
        *pMsg << pContext->GetMethodID();
        *pMsg << pContext->GetProtocolID();
        return pMsg;
    }

    bool ObjDupProtocol::ParseRMCCallMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned int uiFlags = 0;
        DOHandle hCaller;
        DOHandle hTarget;
        unsigned short usCallID;
        unsigned short usMethodID;
        *pMsg >> usCallID;
        *pMsg >> uiFlags;
        *pMsg >> hCaller;
        *pMsg >> hTarget;
        *pMsg >> usMethodID;
        if (bTrace) {
            pTrace->Format(
                "RMC_CALL message RMC_ID: %d, Flags: %d, Source: %x, TargetObject: %x, MethodID: %d",
                usCallID,
                uiFlags,
                hCaller.m_uiValue,
                hTarget,
                usMethodID
            );
        }
        if (bProcess) {
            return ProcessRMCCallMessage(pMsg, usCallID, hCaller, uiFlags, hTarget, usMethodID);
        } else {
            return true;
        }
    }

    bool ObjDupProtocol::ProcessRMCCallMessage(
        Message *pMsg,
        unsigned short &usCallID,
        DOHandle &hCaller,
        unsigned int &uiFlags,
        DOHandle &hTarget,
        unsigned short &usMethodID
    ) {
        CallMethodOperation *pOp = new (__FILE__, 0x29D)
            CallMethodOperation(usCallID, hCaller, uiFlags, hTarget, usMethodID, pMsg);
        StreamPosition posMsg = pMsg->GetPosition();
        pOp->Prepare();
        if (!pOp->PostponeOperation()) {
            if (uiFlags & 4) {
                pOp->Execute();
            } else {
                pOp->Abort();
            }
            delete pOp;
            return true;
        } else {
            pOp->Execute();
            pMsg->SetPosition(posMsg);
            JobExecuteDelayedRMC *pJob =
                new (__FILE__, 0x2AB) JobExecuteDelayedRMC(pOp, pMsg);
            GetScheduler()->Queue(pJob, false);
            return false;
        }
    }

}

namespace Quazal {

    Message *ObjDupProtocol::CreateRMCResponseMessage(CallMethodOperation *pOperation) {
        Message *pMsg = CreateMessage(0xB);
        *pMsg << pOperation->GetCallID();
        return pMsg;
    }

    bool ObjDupProtocol::ParseRMCResponseMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned short usCallID;
        *pMsg >> usCallID;
        if (bTrace) {
            pTrace->Format("RMC_RESPONSE message RMC_ID: %d", usCallID);
        }
        if (bProcess) {
            ProcessRMCResponse(pMsg, &usCallID);
        }
        return true;
    }

    void ObjDupProtocol::ProcessRMCResponse(Message *pMsg, unsigned short *pusCallID) {
        CallContext *pContext = m_oCallRegister.GetCallContextRef(*pusCallID);
        if (pContext != 0) {
            static_cast<DOCallContext *>(pContext)->SignalResponse(UserContext(pMsg));
            pContext->ReleaseRef();
        }
    }

    Message *ObjDupProtocol::CreateFetchRequestMessage(FetchContext *pContext) {
        Message *pMsg = CreateMessage(0xD);
        *pMsg << pContext->GetCallID();
        *pMsg << pContext->GetTargetObject();
        *pMsg << (unsigned int)StationManager::GetLocalStationHandle();
        return pMsg;
    }

    bool ObjDupProtocol::ParseFetchRequestMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        DOHandle hObject;
        DOHandle hStation;
        unsigned short usCallID;
        *pMsg >> usCallID;
        *pMsg >> hObject;
        *pMsg >> hStation;
        if (bTrace) {
            pTrace->Format(
                "FETCH message for object %s %x, SourceStation: %x",
                hObject.GetClassName(),
                hObject,
                hStation
            );
        }
        if (bProcess) {
            ProcessFetchRequestMessage(hObject, hStation, usCallID);
        }
        return true;
    }

    void ObjDupProtocol::ProcessFetchRequestMessage(
        DOHandle &hObject, DOHandle &hStation, unsigned short &usCallID
    ) {
        FetchRef refObject((DOHandle)hObject);
        int iOutcome = 0x80010001;
        if (refObject.IsValid()) {
            if (!refObject->FlagIsSet(1)) {
                iOutcome = 0x80060004;
            } else {
                if (refObject->Fetch(hStation, hStation, true, DOHandle())) {
                    iOutcome = 0x60001;
                } else {
                    iOutcome = 0x80010006;
                }
            }
        } else {
            iOutcome = 0x80060004;
        }
        if (iOutcome != 0x60001) {
            StationRef refStation(hStation);
            Message *pOutcome = CreateCallOutcomeMessage(usCallID, iOutcome);
            refStation->SendMessage(pOutcome, true);
            delete pOutcome;
        }
    }

    Message *ObjDupProtocol::CreateMigrationMessage(MigrationContext *pContext) {
        m_oCallRegister.Register(pContext);
        Message *pMsg = CreateDOProtocolMessage();
        ProtocolCallContext oCallContext;
        qList<DOHandle> lstStations;
        pContext->GetObject()->FillDuplicaStationsList(&lstStations);
        unsigned char ucFlags = pContext->GetFlags();
        unsigned short usCallID = pContext->GetCallID();
        unsigned int uiTarget = pContext->m_uiTarget;
        unsigned int uiMethod = pContext->m_uiMethod;
        oCallContext.CallMigration(
            pMsg,
            &usCallID,
            StationManager::GetLocalStationHandle(),
            &uiMethod,
            &uiTarget,
            &ucFlags,
            &lstStations
        );
        return pMsg;
    }

    Message *ObjDupProtocol::CreateCallOutcomeMessage(unsigned short usCallID, int iOutcome) {
        Message *pMsg = CreateMessage(8);
        *pMsg << usCallID;
        *pMsg << (unsigned int)iOutcome;
        return pMsg;
    }

    bool ObjDupProtocol::ParseCallOutcomeMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        unsigned short usCallID;
        *pMsg >> usCallID;
        unsigned int uiOutcome;
        *pMsg >> uiOutcome;
        int eOutcome = uiOutcome;
        if (bTrace) {
            pTrace->Format(
                "CALL_OUTCOME message for call %d. Outcome is %s",
                usCallID,
                DOCallContext::GetOutcomeString((DOCallContext::_Outcome)eOutcome)
            );
        }
        if (bProcess) {
            ProcessCallOutcome(pMsg->GetSourceStation(), usCallID, eOutcome);
        }
        return true;
    }

    void ObjDupProtocol::ProcessCallOutcome(DOHandle hStation, unsigned short usCallID, int iOutcome) {
        CallContext *pContext = m_oCallRegister.GetCallContextRef(usCallID);
        if (pContext != 0) {
            static_cast<DOCallContext *>(pContext)->SignalOutcome(hStation, (DOCallContext::_Outcome)iOutcome);
            pContext->ReleaseRef();
        }
    }

    bool ObjDupProtocol::ProcessBundleMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        if (bTrace) {
            pTrace->Format("MESSAGE_BUNDLE message. Size: %d", pMsg->GetPayloadSize());
        }
        if (bProcess) {
            bool bFinished = false;
            Message *pSubMsg;
            stlpmtx_std::list<Message *, MemAllocator<Message *> > lstMessages;
            while (!bFinished) {
                pSubMsg = new (__FILE__, 0x34D) Message();
                pMsg->ExtractMessage(pSubMsg);
                if (pSubMsg->GetPayloadSize() != 0) {
                    lstMessages.push_front(pSubMsg);
                } else {
                    delete pSubMsg;
                    bFinished = true;
                }
            }
            while (!lstMessages.empty()) {
                QueueMessage(
                    lstMessages.front(),
                    pMsg->GetSourceStation(),
                    pMsg->GetSourceEndPoint(),
                    true
                );
                lstMessages.pop_front();
            }
        }
        return true;
    }

    Message *ObjDupProtocol::CreateEOSMessage(DOHandle hStation) {
        Message *pMsg = CreateMessage(0xFF);
        *pMsg << hStation;
        return pMsg;
    }

    void ObjDupProtocol::QueueEOS(DOHandle hStation) {
        Message *pEOS = CreateEOSMessage(hStation);
        Message *pCopy = new (__FILE__, 0x36E) Message(pEOS->GetBuffer());
        delete pEOS;
        QueueMessage(pCopy, StationManager::GetLocalStationHandle(), 0, false);
    }

    bool ObjDupProtocol::ParseEOSMessage(
        Message *pMsg, bool bProcess, bool bTrace, String *pTrace
    ) {
        DOHandle hStation;
        *pMsg >> hStation;
        if (bTrace) {
            pTrace->Format("EOS message for station %x", hStation.m_uiValue);
        }
        if (bProcess) {
            ProcessEOS(hStation);
        }
        return true;
    }

    void ObjDupProtocol::ProcessEOS(const DOHandle &hStation) {
        StationRef refStation(hStation);
        if (refStation.IsValid()) {
            refStation->ProcessEOS();
        }
    }

    bool ObjDupProtocol::ShouldGrabWellKnown() {
        if (m_bListeningOnWellKnown) {
            return false;
        }
        StationRef refLocal(StationManager::GetLocalStationHandle());
        if (!refLocal.IsValid()) {
            return false;
        }
        StationSelection oStations(true, true);
        oStations.GotoStart();
        oStations.Refresh();
        while (!oStations.EndReached()) {
            bool bLower = oStations.Current()->GetID() == refLocal->GetID()
                && oStations.Current()->GetHandleValue() < refLocal->GetHandleValue();
            if (bLower) {
                return false;
            }
            oStations.Next(0);
        }
        return true;
    }

    ObjDupProtocol *ObjDupProtocol::GetInstance() {
        return NetZCore::GetInstance()->GetObjDupProtocol();
    }

    bool ObjDupProtocol::ListenOnWellKnown() {
        PRUDPTransport *pTransport;
        if (GetTransport() != 0) {
            pTransport = GetTransport();
        }
        if (GetTransport()->Listen(GetWellKnownPort(), 0, true, false)) {
            m_usWellKnownPort = GetWellKnownPort();
            if (!NetZCore::GetInstance()->GetListener()->m_bStarted) {
                NetZCore::GetInstance()->GetListener()->Start(true, false);
            }
            m_bListeningOnWellKnown = true;
        }
        return m_bListeningOnWellKnown;
    }

    bool ObjDupProtocol::StartToListen() {
        if (IsListening(0)) {
            return true;
        }
        if (ListenOnWellKnown()) {
            return true;
        }
        return ListenOnAnyPort();
    }

    bool ObjDupProtocol::ListenOnAnyPort() {
        if (GetTransport()->Listen(0, &m_usAnyPort, true, false)) {
            if (!NetZCore::GetInstance()->GetListener()->m_bStarted) {
                NetZCore::GetInstance()->GetListener()->Start(true, false);
            }
            m_bListeningOnAnyPort = true;
        }
        if (!m_bListeningOnAnyPort) {
            SystemError::SignalError(0, 0, 0xE0030019, 0);
        }
        return m_bListeningOnAnyPort;
    }

    void ObjDupProtocol::StopToListen() {
        if (NetZCore::GetInstance()->GetListener()->m_bStarted) {
            NetZCore::GetInstance()->GetListener()->Stop();
        }
        if (m_bListeningOnWellKnown) {
            GetTransport()->StopListening(m_usWellKnownPort);
            m_bListeningOnWellKnown = false;
        }
        if (m_bListeningOnAnyPort) {
            GetTransport()->StopListening(m_usAnyPort);
            m_bListeningOnAnyPort = false;
        }
        m_oCallRegister.CancelPendingCalls();
    }

    bool ObjDupProtocol::IsListeningOnWellKnown() const { return m_bListeningOnWellKnown; }

    bool ObjDupProtocol::IsListeningOnAnyPort() const { return m_bListeningOnAnyPort; }

    bool ObjDupProtocol::IsListening(unsigned short *pusPort) const {
        bool bListening = IsListeningOnWellKnown() || IsListeningOnAnyPort();
        if (pusPort != 0 && bListening) {
            if (IsListeningOnWellKnown()) {
                *pusPort = m_usWellKnownPort;
            } else if (IsListeningOnAnyPort()) {
                *pusPort = m_usAnyPort;
            }
        }
        return bListening;
    }

    bool ObjDupProtocol::AddLocalURLs(Station *pStation) {
        if (NetZ::GetMode() != 1 || pStation->GetURLCount() == 1) {
            StationURLList *pURLs = ((NetZ *)GetInstanceType1Delegator())->GetLocalURLs();
            ScopedCS oCS(pURLs->m_cs);
            if (!pURLs->IsEmpty()) {
                if ((pURLs->m_lstURLs.begin()->GetType() & 2) == 2) {
                    pStation->m_oURLs.SetURL(0, pURLs->m_lstURLs.begin()->GetURL());
                }
            }
            for (qList<StationURL>::iterator it = pURLs->m_lstURLs.begin();
                 it != pURLs->m_lstURLs.end();
                 ++it) {
                pStation->m_oURLs.AddURL(*it);
            }
        }
        return true;
    }

    bool ObjDupProtocol::ParseMessage(Message *pMsg, bool bProcess, String *pTrace) {
        unsigned char ucType = 0;
        pMsg->Rewind();
        *pMsg >> ucType;
        return ParseSpecificMessage(pMsg, ucType, bProcess, pTrace);
    }

    bool ObjDupProtocol::ParseSpecificMessage(
        Message *pMsg, unsigned int uiType, bool bProcess, String *pTrace
    ) {
        bool bTrace = pTrace != 0;
        bool bResult = false;
        switch (uiType) {
        case 0x00:
            bResult = ParseJoinRequestMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x01:
            bResult = ParseJoinResponseMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x02:
            bResult = ParseUpdateMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x04:
            bResult = ParseDeleteMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x05:
            bResult = ParseActionMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x08:
            bResult = ParseCallOutcomeMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x0A:
            bResult = ParseRMCCallMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x0B:
            bResult = ParseRMCResponseMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x0D:
            bResult = ParseFetchRequestMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0xFF:
            bResult = ParseEOSMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x0F:
            bResult = ProcessBundleMessage(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x10:
            bResult = true;
            if (bTrace) {
                *pTrace = "DOPROTOCOL message";
            }
            if (bProcess) {
                bResult = ProcessDOProtocolMessage(pMsg);
            }
            break;
        case 0x14:
            bResult = ParseGetParticipantsRequest(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0x15:
            bResult = ParseGetParticipantsResponse(pMsg, bProcess, bTrace, pTrace);
            break;
        case 0xFE:
            bResult = true;
            break;
        }
        return bResult;
    }

}
