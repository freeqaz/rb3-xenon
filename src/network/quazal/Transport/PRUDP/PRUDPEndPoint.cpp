// Quazal NetZ - Transport/PRUDP/PRUDPEndPoint.cpp
//
// Retail TU 0x82B31AB8..0x82B358E8, compiled /Od /Ob1 /Oi-: every function in
// it stores its arguments to home slots and reloads them per use, and inline
// helpers (ScopedCS, Time, LogicalClock, the pending-operation record) are
// expanded in place with their `this` spilled to a temporary.
//
// Local variable NAMES are load-bearing: /Od lays a scope's locals out by a
// walk over the symbol table's hash buckets, so renaming a local moves its
// stack slot. Line numbers are load-bearing too: every allocation passes
// __FILE__/__LINE__, so each one sits under a #line that reproduces retail's.

#include "Platform/Result.h"
#include "Platform/RootObject.h"
#include "Platform/ScopedCS.h"

extern "C" int abs(int);

namespace Quazal {

    class Time {
    public:
        Time() : m_t(0) {}
        ~Time() {}
        Time &operator=(const Time &);
        operator unsigned __int64() const;
        __int64 operator-(const Time &) const;
        static Time GetTime();

        unsigned __int64 m_t;
    };

    class LogicalClock {
    public:
        LogicalClock() : m_v(0) {}
        LogicalClock(unsigned short v) : m_v(v) {}
        LogicalClock(int v) : m_v(v) {}
        LogicalClock(const LogicalClock &o) : m_v(o.m_v) {}
        ~LogicalClock() {}
        LogicalClock &operator=(const LogicalClock &o) {
            m_v = o.m_v;
            return *this;
        }
        LogicalClock &operator++() {
            m_v++;
            return *this;
        }
        LogicalClock operator+(unsigned short) const;
        int operator-(const LogicalClock &) const;

        unsigned short m_v;
    };

    class UserContext {
    public:
        UserContext() : m_v(0) {}
        ~UserContext() {}

        unsigned int m_v;
    };

    class RefCounted {
    public:
        virtual ~RefCounted();
        virtual RefCounted *AcquireRef();
        virtual void ReleaseRef();
    };

    class Buffer : public RootObject {
    public:
        Buffer(unsigned int);
        virtual ~Buffer();
        virtual Buffer *AcquireRef();
        virtual void ReleaseRef();
        unsigned char *GetContentPtr();
        unsigned int GetContentSize();
        void AppendData(const unsigned char *, unsigned int, unsigned int);
        void AppendData(Buffer *);

        char unk4[0x14 - 0x4];
    };

    class Timeout {
    public:
        void SetExpirationDelay(unsigned int);
        void SetRTO(unsigned int);
        bool IsExpired();
    };

    class PRUDPEndPoint;

    class InetAddressRef {
    public:
        void Set(void *);
        char unk0[4];
    };

    class Packet : public RootObject {
    public:
        virtual ~Packet();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        unsigned char GetType() { return m_ucFlags & 7; }
        bool HasFlag(unsigned char ucFlag) { return (m_ucFlags & ucFlag) != 0; }
        void SetDestination(void *pAddress) { m_oDest.Set(pAddress); }
        void SetSignature(unsigned int uiSignature) { m_uiSignature = uiSignature; }
        void SetSessionID(unsigned char ucSessionID) { m_ucSessionID = ucSessionID; }
        void SetSequenceID(LogicalClock oID) { m_oSequenceID = oID; }
        LogicalClock GetSequenceID();
        Time GetTimeStamp();

        char unk8[0x12 - 0x8];
        unsigned char m_ucFlags;    // 0x12
        unsigned char m_ucSessionID; // 0x13
        unsigned int m_uiSignature;  // 0x14
        LogicalClock m_oSequenceID;  // 0x18
        char unk1a[0x20 - 0x1a];
        unsigned char m_ucFragmentID; // 0x20
        Buffer *m_pPayload;           // 0x24
        InetAddressRef m_oDest;       // 0x28
        char unk2c[0xa8 - 0x2c];
        Time m_tTimeStamp;            // 0xa8
    };

    class PacketOut : public Packet {
    public:
        PacketOut(PRUDPEndPoint *, unsigned char, unsigned char, Buffer *);

        Timeout *GetTimeout() { return m_pTimeout; }
        unsigned short GetNbSends() { return m_usNbSends; }
        void SetReliable(bool bReliable) { m_bReliable = bReliable; }

        char unkb0[0xc0 - 0xb0];
        Timeout *m_pTimeout;        // 0xc0
        char unkc4[0xc8 - 0xc4];
        unsigned short m_usNbSends; // 0xc8
        char unkca[0xd8 - 0xca];
        bool m_bReliable;           // 0xd8
        char unkd9[0xe0 - 0xd9];
    };

    class PacketIn : public Packet {};

    class StreamSettings {
    public:
        char unk0[0x50];
        unsigned int GetMaxRetransmission();
        unsigned int GetKeepAliveTimeout();
        unsigned int GetMaxSilenceTime();
        unsigned int GetWindowSize();
        float GetExtraRetransmitTimeoutMultiplier();
        unsigned int GetExtraRetransmitTimeoutTrigger();
        float GetRetransmitTimeoutMultiplier();
        unsigned int GetInitialRTT();
        unsigned int GetMaxWindowMapSize();
        unsigned int GetPingTimeout();
        bool GetSendKeepAlive();
    };

    class Counters {
    public:
        void Increment(unsigned int, unsigned int);
    };

    class TransportStats {
    public:
        char unk0[0x18];
        Counters m_oCounters; // 0x18
    };

    class TimeoutManager {
    public:
        void SchedulePacketTimeout(PacketOut *);
        void CancelPacketTimeout(PacketOut *);
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    struct InstanceEntry {
        char unk0[0xc];
        void *m_pInstance; // 0xc
    };

    class InstanceTable {
    public:
        InstanceEntry *Find(unsigned int, unsigned int);
    };

    class InstanceControl {
    public:
        static void *GetInstance(unsigned int uiType) {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceEntry *pEntry = s_oInstanceTable.Find(uiType, uiContext);
            if (pEntry == 0)
                return 0;
            else
                return pEntry->m_pInstance;
        }

        static InstanceTable s_oInstanceTable;
    };

    template <class T> class PseudoGlobalVariable {
    public:
        T &GetValue() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            if (uiContext == 0)
                return m_oDefault;
            return m_pContexts[uiContext];
        }

        char unk0[0x8];
        T *m_pContexts; // 0x8
        T m_oDefault;   // 0x10
        char unk60[0xb0 - 0x10 - sizeof(T)];
    };

    extern PseudoGlobalVariable<StreamSettings> s_oStreamSettings[];

    class ConnectionOrientedStream {
    public:
        StreamSettings *GetSettings() { return &s_oStreamSettings[m_eType].GetValue(); }
        void Send(unsigned short, unsigned char, PacketOut *);
        void EndPointDisconnected(PRUDPEndPoint *);

        char unk0[0x4];
        unsigned int m_eType;              // 0x4
        char unk8[0xc - 0x8];
        TransportStats *m_pStats;          // 0xc
        char unk10[0x14 - 0x10];
        TimeoutManager m_oTimeoutManager;  // 0x14
    };

    class StationURL;

    class EndPointEventHandler {
    public:
        virtual void Unk0();
        virtual void OnDataReceived(class EndPoint *, Buffer *);
        virtual void OnDisconnected(class EndPoint *, unsigned int);
    };

    class EndPointAddress {
    public:
        void *GetAddress();
        unsigned char GetPortType();
    };

    class EndPoint;
    typedef void (*pfCompletion)(EndPoint *, qResult, const UserContext *);

    class EndPoint {
    public:
        enum _ConnectionState {
            NotConnected = 0,
            Connecting = 1,
            Connected = 2,
            Disconnecting = 3,
            Faulty = 4
        };

        EndPoint(ConnectionOrientedStream *, const StationURL *);

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
        virtual _ConnectionState GetConnectionState() = 0;
        virtual bool SetConnectionState(_ConnectionState) = 0;
        virtual void Unk16();
        virtual void SignalEvent(unsigned int);
        virtual unsigned int GetRTT() = 0;
        virtual unsigned int GetRTTAverage() = 0;
        virtual qResult _Connect(Buffer *, Buffer *, pfCompletion, const UserContext &, unsigned int) = 0;
        virtual qResult _Disconnect(pfCompletion, const UserContext &, unsigned int) = 0;
        virtual qResult _Send(Buffer *, unsigned int) = 0;
        virtual ~EndPoint();
        virtual void SignalFault(unsigned int, bool) = 0;

        ConnectionOrientedStream *GetStream() { return m_pStream; }

        ConnectionOrientedStream *m_pStream; // 0x4
        EndPointAddress m_oAddress;         // 0x8
        char unkc[0x6c - 0xc];
        EndPointEventHandler *m_pHandler;   // 0x6c
        char unk70[0x80 - 0x70];
    };

    void *RbTreeIncrement(void *);

    struct WindowIteratorBase {
        WindowIteratorBase(void *pNode) : m_pNode(pNode) {}
        void *m_pNode;
    };

    struct WindowIterator : public WindowIteratorBase {
        WindowIterator(const WindowIterator &o) : WindowIteratorBase(o.m_pNode) {}
        WindowIterator &operator++() {
            m_pNode = RbTreeIncrement(m_pNode);
            return *this;
        }
    };

    class SlidingWindow : public RootObject {
    public:
        WindowIterator Begin();
        PacketOut *GetPacket(WindowIterator);
        SlidingWindow(unsigned short);
        ~SlidingWindow();
        unsigned int GetNbPacketsInWindow();
        bool IsEmpty();
        void Purge();
        PacketOut *GetNextToSend();
        bool Push(PacketOut *);
        bool IsFull();
        PacketOut *Acknowledge(const LogicalClock &);
        void AcknowledgeUpTo(const LogicalClock &);
        void GetFirstPacketIterator(void *);
        PacketOut *GetPacket(void *);
        void Clear();

        char unk0[0x2c];
    };

    class PacketDispatchQueue : public RootObject {
    public:
        PacketDispatchQueue();
        ~PacketDispatchQueue();
        void Purge();
        void Queue(PacketIn *);
        PacketIn *GetNextToDispatch();
        void Dispatched(PacketIn *);

        char unk0[0x20];
    };

    class RTT {
    public:
        RTT(unsigned int);
        ~RTT();
        void Adjust(unsigned int);

        unsigned int m_uiSmoothedAvg; // 0x0
        unsigned int m_uiSmoothedVar; // 0x4
        unsigned int m_uiLast;        // 0x8
    };

    class ProfilingUnit {
    public:
        ProfilingUnit(const char *, unsigned int);
        ~ProfilingUnit();

        __int64 unk0[9];
    };

    class WaitLoop {
    public:
        WaitLoop(unsigned int, unsigned int);
        virtual ~WaitLoop();
        void Start();
        bool CheckExpiration(const char *, unsigned int, const char *);

        char unk4[0x30 - 0x4];
    };

    class Scheduler {
    public:
        static void Dispatch();
    };

    CriticalSection *GetSystemLock();

    struct PendingOperation {
        PendingOperation() {
            m_pBuffer = 0;
            m_pData = 0;
            m_pfCallback = 0;
        }
        ~PendingOperation() { Reset(); }
        void Set(Buffer *pBuffer, Buffer *pData, pfCompletion pfCallback, const UserContext &oContext) {
            if (pBuffer)
                m_pBuffer = pBuffer->AcquireRef();
            else
                m_pBuffer = 0;
            m_pData = pData;
            m_pfCallback = pfCallback;
            m_oContext = oContext;
        }
        void Reset();
        bool IsPending() { return m_pfCallback != 0; }
        void Complete(EndPoint *pEndPoint, qResult oResult) {
            pfCompletion pfCallback = m_pfCallback;
            Reset();
            pfCallback(pEndPoint, oResult, &m_oContext);
        }

        Buffer *m_pData;       // 0x0
        Buffer *m_pBuffer;     // 0x4
        pfCompletion m_pfCallback; // 0x8
        UserContext m_oContext; // 0xc
    };

    class PRUDPEndPoint : public EndPoint {
    public:
        PRUDPEndPoint(ConnectionOrientedStream *, const StationURL *);
        virtual ~PRUDPEndPoint();

        virtual bool IsNotConnected() { return m_eState == NotConnected; }
        virtual bool IsConnecting() { return m_eState == Connecting; }
        virtual bool IsDisconnecting() { return m_eState == Disconnecting; }
        virtual bool IsFaulty() { return m_eState == Faulty; }
        virtual bool IsConnected() { return m_eState == Connected; }
        virtual bool PeerIsConnected() { return m_bPeerConnected; }
        virtual bool PeerIsDisconnected() { return !m_bPeerConnected; }
        virtual void SetKeepAliveTimeout(unsigned int);
        virtual unsigned int GetKeepAliveTimeout() { return m_uiKeepAliveTimeout; }
        virtual unsigned int GetMaxSilenceTime() { return m_uiMaxSilenceTime; }
        virtual void SetPeerConnected() {
            m_bPeerConnected = true;
            SignalEvent(0x2000000);
        }
        virtual void SetPeerDisconnected() {
            m_bPeerConnected = false;
            SignalEvent(0x2000000);
        }
        virtual _ConnectionState GetConnectionState() { return m_eState; }
        virtual bool SetConnectionState(_ConnectionState);
        virtual unsigned int GetRTT() {
            unsigned int rtt = m_oRTT.m_uiLast;
            return rtt;
        }
        virtual unsigned int GetRTTAverage() { return m_oRTT.m_uiSmoothedAvg >> 3; }
        virtual qResult _Connect(Buffer *, Buffer *, pfCompletion, const UserContext &, unsigned int);
        virtual qResult _Disconnect(pfCompletion, const UserContext &, unsigned int);
        virtual qResult _Send(Buffer *, unsigned int);
        virtual void SignalFault(unsigned int, bool);

        unsigned int GetNbPacketsInWindow();
        bool IsWindowEmpty();
        void SendOnStream(unsigned char ucPort, PacketOut *pPacket) {
            GetStream()->Send(m_usConnectionID, ucPort, pPacket);
        }
        qResult Frag(Buffer *, unsigned int, unsigned int, unsigned char, bool);
        bool Send(PacketOut *);
        void SendNextPackets();
        void SendPacket(PacketOut *);
        bool Defrag(PacketIn *);
        void DispatchData(Buffer *);
        void ProcessData(PacketIn *, Time);
        void SignalFaultEvent(unsigned int);
        void ServiceIncomingPacket(PacketIn *);
        void PacketAcknowledged(PacketIn *);
        void ServiceTimeout(PacketOut *);
        void CancelTimeout(PacketOut *);
        void StartKeepAlive();
        void StopKeepAlive();
        void TimeToPing();
        struct TransportInfo {
            char unk0[8];
            unsigned int m_uiMaxPacketSize; // 0x8
        };
        struct TransportContext {
            TransportInfo *GetInfo() { return m_pInfo; }
            char unk0[0x4c];
            TransportInfo *m_pInfo; // 0x4c
        };
        static TransportContext *GetTransportContext() {
            return (TransportContext *)InstanceControl::GetInstance(1);
        }
        static TransportInfo *GetTransportInfo() {
            TransportContext *pContext = GetTransportContext();
            if (pContext == 0)
                return 0;
            else
                return pContext->GetInfo();
        }
        static unsigned short GetHeaderSize();
        Time GetLastReceptionTime(PacketIn *);
        Time GetLastSendTime(PacketOut *);

        unsigned short m_usConnectionID;          // 0x80
        PacketDispatchQueue *m_pDispatchQueue;    // 0x84
        SlidingWindow *m_pSlidingWindow;          // 0x88
        PacketOut *m_pKeepAlivePacket;            // 0x8c
        PacketOut *m_pConnectPacket;              // 0x90
        bool m_bUnk94;                            // 0x94
        unsigned int m_uiMaxSilenceTime;          // 0x98
        unsigned int m_uiKeepAliveTimeout;        // 0x9c
        _ConnectionState m_eState;                // 0xa0
        bool m_bPeerConnected;                    // 0xa4
        unsigned int m_uiFaultReason;             // 0xa8
        RTT m_oRTT;                               // 0xac
        Time m_tLastSend;                         // 0xb8
        Time m_tLastReception;                    // 0xc0
        LogicalClock m_oNextSequenceID;           // 0xc8
        LogicalClock m_oNextExpectedSequenceID;   // 0xca
        Buffer *m_pDefragBuffer;                  // 0xcc
        unsigned char m_ucNextFragmentID;         // 0xd0
        unsigned int m_uiSignature;               // 0xd4
        unsigned char m_ucPeerSessionID;          // 0xd8
        unsigned char m_ucSessionID;              // 0xd9
        ProfilingUnit m_oSendProfiling;           // 0xe0
        PendingOperation m_oPendingOperation;     // 0x128
    };

}


namespace Quazal {

    unsigned int PRUDPEndPoint::GetNbPacketsInWindow() {
        return m_pSlidingWindow->GetNbPacketsInWindow();
    }

    bool PRUDPEndPoint::IsWindowEmpty() { return m_pSlidingWindow->IsEmpty(); }

    void PRUDPEndPoint::SetKeepAliveTimeout(unsigned int uiTimeout) {
        ScopedCS oCS(*GetSystemLock());
        m_uiKeepAliveTimeout = uiTimeout;
        if (m_pKeepAlivePacket)
            StopKeepAlive();
        StartKeepAlive();
    }

#line 88 ".\\Transport\\PRUDP\\PRUDPEndPoint.cpp"
    PRUDPEndPoint::PRUDPEndPoint(ConnectionOrientedStream *pStream, const StationURL *pURL)
        : EndPoint(pStream, pURL), m_usConnectionID(0),
          m_oRTT(pStream->GetSettings()->GetInitialRTT()), m_oSendProfiling("Send", 1000) {
        m_eState = NotConnected;
        SetPeerDisconnected();
#line 91
        m_pDispatchQueue = new (__FILE__, __LINE__) PacketDispatchQueue();
#line 92
        m_pSlidingWindow = new (__FILE__, __LINE__) SlidingWindow(GetStream()->GetSettings()->GetWindowSize());
        m_oNextExpectedSequenceID = m_oNextSequenceID = 0;
        m_pKeepAlivePacket = 0;
        m_uiFaultReason = 0;
#line 98
        m_pConnectPacket = new (__FILE__, __LINE__) PacketOut(this, 0, 0x20, 0);
        m_pDefragBuffer = 0;
        m_ucNextFragmentID = 1;
        m_uiSignature = 0;
        m_ucSessionID = (unsigned char)(unsigned __int64)Time::GetTime();
        if (m_ucSessionID == 0)
            m_ucSessionID++;
        m_ucPeerSessionID = 0;
        m_bUnk94 = false;
        m_uiMaxSilenceTime = GetStream()->GetSettings()->GetMaxSilenceTime();
        m_uiKeepAliveTimeout = GetStream()->GetSettings()->GetKeepAliveTimeout();
    }

    PRUDPEndPoint::~PRUDPEndPoint() {
        StopKeepAlive();
        if (m_pConnectPacket) {
            GetStream()->m_oTimeoutManager.CancelPacketTimeout(m_pConnectPacket);
            m_pConnectPacket->ReleaseRef();
            m_pConnectPacket = 0;
        }
        WindowIterator it = m_pSlidingWindow->Begin();
        PacketOut *pPacket = m_pSlidingWindow->GetPacket(it);
        while (pPacket) {
            CancelTimeout(pPacket);
            ++it;
            pPacket = m_pSlidingWindow->GetPacket(it);
        }
        m_pSlidingWindow->Clear();
        m_pSlidingWindow->Purge();
        m_pDispatchQueue->Purge();
        if (m_pDispatchQueue)
            delete m_pDispatchQueue;
        if (m_pSlidingWindow)
            delete m_pSlidingWindow;
        if (m_pDefragBuffer)
            m_pDefragBuffer->ReleaseRef();
    }

}

namespace Quazal {

#define WAIT_WHILE(oWait, cond)                                                                    \
    while (cond) {                                                                                 \
        Scheduler::Dispatch();                                                                     \
        if (!oWait.CheckExpiration(__FILE__, __LINE__, #cond))                                     \
            break;                                                                                 \
    }

    qResult PRUDPEndPoint::_Connect(
        Buffer *pConnectData, Buffer *pAuthData, pfCompletion pfCallback,
        const UserContext &oContext, unsigned int uiTimeout
    ) {
        if (uiTimeout == (unsigned int)-1)
            uiTimeout = 10000000;
        if (!PeerIsConnected() && m_eState != NotConnected)
            return 0x80050002;
        if (IsConnecting() || IsConnected())
            return 0x80050002;
        m_bUnk94 = false;
        SetConnectionState(Connecting);
        m_oPendingOperation.Set(pConnectData, pAuthData, pfCallback, oContext);
        m_pConnectPacket->GetTimeout()->SetExpirationDelay(uiTimeout);
        m_pConnectPacket->SetDestination(m_oAddress.GetAddress());
        SendPacket(m_pConnectPacket);
        if (pfCallback == 0) {
            WaitLoop oWait(50, uiTimeout);
            oWait.Start();
#line 189
            WAIT_WHILE(oWait, IsConnecting())
            m_oPendingOperation.Reset();
            if (!IsConnected()) {
                SetConnectionState(NotConnected);
                return 0x80050008;
            }
        }
        return 0x10001;
    }

    qResult PRUDPEndPoint::_Disconnect(
        pfCompletion pfCallback, const UserContext &oContext, unsigned int uiTimeout
    ) {
        if (!IsConnected())
            return 0x80050002;
        SetConnectionState(Disconnecting);
        m_oPendingOperation.Set(0, 0, pfCallback, oContext);
#line 214
        PacketOut *pPacket = new (__FILE__, __LINE__) PacketOut(this, 3, 0x30, 0);
        Timeout *pTimeout = pPacket->GetTimeout();
        pTimeout->SetExpirationDelay(uiTimeout);
        pTimeout->SetRTO(500);
        GetStream()->m_oTimeoutManager.SchedulePacketTimeout(pPacket);
        if (!Send(pPacket)) {
            pPacket->ReleaseRef();
            return 0x80050007;
        }
        pPacket->ReleaseRef();
        if (pfCallback == 0) {
            WaitLoop oWait(50, uiTimeout);
            oWait.Start();
#line 233
            WAIT_WHILE(oWait, IsDisconnecting())
            m_oPendingOperation.Reset();
            if (!IsNotConnected()) {
                SetConnectionState(NotConnected);
                return 0x80050008;
            }
        }
        return 0x10001;
    }

    qResult PRUDPEndPoint::Frag(
        Buffer *pBuffer, unsigned int uiBufferSize, unsigned int uiFragmentSize,
        unsigned char ucFlags, bool bReliable
    ) {
        unsigned int uiPos = 0;
        unsigned char ucFragmentID = 1;
        while (pBuffer->GetContentSize() > uiPos) {
#line 254
            Buffer *pFragment = new (__FILE__, __LINE__) Buffer(uiBufferSize);
            unsigned int uiRemaining = pBuffer->GetContentSize() - uiPos;
            pFragment->AppendData(
                pBuffer->GetContentPtr() + uiPos,
                uiRemaining > uiFragmentSize ? uiFragmentSize : uiRemaining, -1
            );
#line 259
            PacketOut *pPacket = new (__FILE__, __LINE__) PacketOut(this, 2, ucFlags, pFragment);
            pPacket->m_bReliable = bReliable;
            uiPos += uiFragmentSize;
            if (pBuffer->GetContentSize() <= uiPos) {
                pPacket->m_ucFragmentID = 0;
            } else {
                pPacket->m_ucFragmentID = ucFragmentID;
                ucFragmentID++;
                if (ucFragmentID == 0)
                    ucFragmentID++;
            }
            if (!Send(pPacket)) {
                pPacket->ReleaseRef();
                pFragment->ReleaseRef();
                return 0x80050007;
            }
            pPacket->ReleaseRef();
            pFragment->ReleaseRef();
        }
        return 0x10001;
    }

    qResult PRUDPEndPoint::_Send(Buffer *pBuffer, unsigned int uiFlags) {
        if (!IsConnected() && !PeerIsConnected())
            return 0x80050002;
        unsigned int uiPacketSize = GetTransportInfo()->m_uiMaxPacketSize;
        unsigned int uiPayloadSize = uiPacketSize - (GetHeaderSize() + 8);
        unsigned char ucFlags = 0;
        if (uiFlags & 1)
            ucFlags |= 0x30;
        if (uiFlags & 2)
            ucFlags |= 0x80;
        if ((uiFlags & 1) && pBuffer->GetContentSize() > uiPayloadSize) {
            return Frag(pBuffer, uiPacketSize, uiPayloadSize, ucFlags, (uiFlags & 8) != 0);
        } else if (pBuffer->GetContentSize() > uiPayloadSize) {
            return 0x8001000a;
        } else {
#line 311
            PacketOut *pPacket = new (__FILE__, __LINE__) PacketOut(this, 2, ucFlags, pBuffer);
            pPacket->SetReliable((uiFlags & 8) != 0);
            pPacket->m_ucFragmentID = 0;
            if (!Send(pPacket)) {
                pPacket->ReleaseRef();
                return 0x80050007;
            }
            pPacket->ReleaseRef();
        }
        return 0x10001;
    }

    bool PRUDPEndPoint::Send(PacketOut *pPacket) {
        pPacket->SetDestination(m_oAddress.GetAddress());
        pPacket->SetSignature(m_uiSignature);
        pPacket->SetSessionID(m_ucSessionID);
        if (pPacket->HasFlag(0x10)) {
            GetStream()->m_pStats->m_oCounters.Increment(6, 1);
            if (pPacket->GetNbSends() == 0 && !m_pSlidingWindow->Push(pPacket)) {
                SignalFault(2, false);
                return false;
            }
            SendNextPackets();
        } else {
            if (!pPacket->HasFlag(8)) {
                GetStream()->m_pStats->m_oCounters.Increment(5, 1);
                if (pPacket->GetType() == 2) {
                    pPacket->SetSequenceID(m_oNextSequenceID);
                    ++m_oNextSequenceID;
                }
            }
            SendOnStream(m_oAddress.GetPortType(), pPacket);
        }
        return true;
    }

    void PRUDPEndPoint::SendNextPackets() {
        PacketOut *pPacket = m_pSlidingWindow->GetNextToSend();
        if (pPacket) {
            m_tLastSend = Time::GetTime();
            SendPacket(pPacket);
        }
    }

    void PRUDPEndPoint::CancelTimeout(PacketOut *pPacket) {
        GetStream()->m_oTimeoutManager.CancelPacketTimeout(pPacket);
    }

    void PRUDPEndPoint::StartKeepAlive() {
        if (m_pKeepAlivePacket == 0) {
#line 790
            m_pKeepAlivePacket = new (__FILE__, __LINE__) PacketOut(this, 4, 0x20, 0);
            if (GetKeepAliveTimeout() == 0 || GetKeepAliveTimeout() == (unsigned int)-1)
                m_pKeepAlivePacket->GetTimeout()->SetRTO(1000);
            else
                m_pKeepAlivePacket->GetTimeout()->SetRTO(GetKeepAliveTimeout());
            GetStream()->m_oTimeoutManager.SchedulePacketTimeout(m_pKeepAlivePacket);
        }
    }

    void PRUDPEndPoint::StopKeepAlive() {
        if (m_pKeepAlivePacket) {
            GetStream()->m_oTimeoutManager.CancelPacketTimeout(m_pKeepAlivePacket);
            m_pKeepAlivePacket->ReleaseRef();
            m_pKeepAlivePacket = 0;
        }
    }

    void PRUDPEndPoint::TimeToPing() {
        Time tCurrentTime = Time::GetTime();
        if (m_uiFaultReason != 0) {
            SignalFaultEvent(m_uiFaultReason);
            return;
        }
        if (tCurrentTime - m_tLastReception > GetMaxSilenceTime()) {
            SignalFaultEvent(2);
            return;
        }
        StreamSettings *pStreamSettings = GetStream()->GetSettings();
        if (tCurrentTime - m_tLastSend > GetKeepAliveTimeout() && pStreamSettings->GetSendKeepAlive()) {
            m_pKeepAlivePacket->SetSequenceID(m_pKeepAlivePacket->GetSequenceID() + 1);
            if (GetKeepAliveTimeout() != 0 && GetKeepAliveTimeout() != (unsigned int)-1)
                Send(m_pKeepAlivePacket);
        }
        GetStream()->m_oTimeoutManager.SchedulePacketTimeout(m_pKeepAlivePacket);
    }

    bool PRUDPEndPoint::SetConnectionState(_ConnectionState eState) {
        _ConnectionState eOldState = m_eState;
        SignalEvent(0x4000000);
        m_eState = eState;
        if (m_oPendingOperation.IsPending()) {
            qResult oResult = 0x80050007;
            if (eOldState == Connecting) {
                SignalEvent(0x1000000);
                oResult = eState == Connected ? qResult(0x10001) : qResult(0x80050007);
            }
            if (eOldState == Disconnecting) {
                oResult = eState == NotConnected ? qResult(0x10001) : qResult(0x80050007);
            }
            m_oPendingOperation.Complete(this, oResult);
        }
        SignalEvent(0x2000000);
        return true;
    }

    void PendingOperation::Reset() {
        if (m_pBuffer)
            m_pBuffer->ReleaseRef();
        m_pBuffer = 0;
        m_pData = 0;
        m_pfCallback = 0;
    }

    Time Packet::GetTimeStamp() { return m_tTimeStamp; }

    LogicalClock LogicalClock::operator+(unsigned short usDelta) const {
        return LogicalClock((unsigned short)(m_v + usDelta));
    }

    int LogicalClock::operator-(const LogicalClock &o) const {
        int iThis = m_v;
        int iOther = o.m_v;
        if (abs(iThis - iOther) < 0x8000)
            return iThis - iOther;
        else if (iThis < iOther)
            return iThis + 0x10000 - iOther;
        else
            return iThis - (iOther + 0x10000);
    }

}

