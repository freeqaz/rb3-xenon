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
        LogicalClock(unsigned int v) : m_v(v) {}
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
    };

    class Timeout {
    public:
        void SetRTO(unsigned int);
        void SetRelativeExpirationTime(unsigned int);
        bool IsExpired();
    };

    class PRUDPEndPoint;

    class Packet : public RootObject {
    public:
        virtual ~Packet();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        unsigned char GetType() { return m_ucFlags & 7; }

        char unk4[0x12 - 0x4];
        unsigned char m_ucFlags;    // 0x12
        unsigned char m_ucSessionID; // 0x13
        unsigned int m_uiSignature;  // 0x14
        LogicalClock m_oSequenceID;  // 0x18
        char unk1a[0x20 - 0x1a];
        unsigned char m_ucFragmentID; // 0x20
        Buffer *m_pPayload;           // 0x24
        char m_oDest[0x4];            // 0x28
    };

    class InetAddressRef {
    public:
        void Set(void *);
    };

    class PacketOut : public Packet {
    public:
        PacketOut(PRUDPEndPoint *, unsigned char, unsigned char, Buffer *);

        char unk2c[0xc0 - 0x2c];
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
        virtual void SetConnectionState(_ConnectionState) = 0;
        virtual void Unk16();
        virtual void SignalEvent(unsigned int);
        virtual unsigned int GetRTT() = 0;
        virtual unsigned int GetRTTAverage() = 0;
        virtual qResult _Connect(Buffer *, Buffer *, void *, const UserContext &, unsigned int) = 0;
        virtual qResult _Disconnect(void *, const UserContext &, unsigned int) = 0;
        virtual qResult _Send(Buffer *, unsigned int) = 0;
        virtual ~EndPoint();

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
        virtual ~WaitLoop() {}
        void Start();
        bool CheckExpiration(const char *, unsigned int, const char *);

        char unk4[0x24 - 0x4];
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
        void Set(Buffer *pData, Buffer *pBuffer, void *pfCallback, const UserContext &oContext) {
            if (pBuffer)
                m_pBuffer = pBuffer->AcquireRef();
            else
                m_pBuffer = 0;
            m_pData = pData;
            m_pfCallback = pfCallback;
            m_oContext = oContext;
        }
        void Reset();

        Buffer *m_pData;       // 0x0
        Buffer *m_pBuffer;     // 0x4
        void *m_pfCallback;    // 0x8
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
        virtual void SetConnectionState(_ConnectionState);
        virtual unsigned int GetRTT() {
            unsigned int rtt = m_oRTT.m_uiLast;
            return rtt;
        }
        virtual unsigned int GetRTTAverage() { return m_oRTT.m_uiSmoothedAvg >> 3; }
        virtual qResult _Connect(Buffer *, Buffer *, void *, const UserContext &, unsigned int);
        virtual qResult _Disconnect(void *, const UserContext &, unsigned int);
        virtual qResult _Send(Buffer *, unsigned int);
        virtual void SignalFault(unsigned int, bool);

        unsigned int GetNbPacketsInWindow();
        bool IsWindowEmpty();
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
