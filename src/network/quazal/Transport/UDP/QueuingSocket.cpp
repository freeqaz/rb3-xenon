// Quazal NetZ - .\Transport\UDP\QueuingSocket.cpp
//
// Retail TU: .text 0x82B39328..0x82B3B918, compiled /Od /Oi- /Ob1 /GR- (see
// objects.json). Its .rdata is the seven __FILE__ strings, then the EH tables
// in .text order, with the emulation-queue vtables in between; no vtable has a
// complete-object locator, so the TU is built without RTTI.
//
// The TU's own functions end at 0x82B3A850. The rest is the COMDAT tail this
// TU instantiates for the network-emulation queues that QueuingSocket holds by
// value (two EmulationChannels at +0x138/+0x160): their virtual functions, the
// timed-item list and its STLport helpers.
//
// The classes are declared here with the layouts retail uses rather than taken
// from the shared Quazal headers.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets, and
// every allocation passes __FILE__/__LINE__, so #line reproduces retail's lines.

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

    class Time {
    public:
        __declspec(noinline) Time() : m_t(0) {}
        ~Time() {}
        Time &operator=(const Time &);
        Time &operator=(unsigned __int64);
        operator unsigned __int64() const;
        Time operator+(unsigned int) const;
        bool operator<(const Time &o) const { return m_t < o.m_t; }
        bool operator>(const Time &o) const { return m_t > o.m_t; }
        bool operator<=(Time o) const { return m_t <= o.m_t; }
        static Time GetTime();

        unsigned __int64 m_t;
    };

    class RandomNumberGenerator {
    public:
        static float GetRealRandomNumber(float);
        static unsigned int GetRandomNumber(unsigned int);
    };

    class InetAddress : public RootObject {
    public:
        InetAddress();
        ~InetAddress();
        InetAddress &operator=(const InetAddress &);
        unsigned short GetPortNumber() const;

        unsigned int m_storage[0x20];
    };

    class Buffer : public RootObject {
    public:
        Buffer(unsigned int);
        virtual ~Buffer();
        virtual void AcquireRef();
        virtual void ReleaseRef();
        unsigned char *GetContentPtr() const;
        unsigned int GetContentSize() const;
        unsigned int GetSize() const;
        void SetContentSize(unsigned int);
        bool AppendData(const void *, unsigned int, unsigned int);

        char m_pad4[0x10];
    };

    class ByteStream : public RootObject {
    public:
        ByteStream();
        ByteStream(Buffer *);
        ~ByteStream();
        void Clear();
        bool Append(const unsigned char *, unsigned int, unsigned int);
        ByteStream &operator<<(const Buffer &);
        ByteStream &operator<<(const unsigned short &us) {
            Append((const unsigned char *)&us, sizeof(us), 1);
            return *this;
        }
        Buffer *GetBuffer() { return m_pBuffer; }
        unsigned int GetLength() { return m_pBuffer->GetContentSize(); }

        bool m_bErrorHasOccurred; // 0x0
        Buffer *m_pBuffer; // 0x4
        unsigned int m_uiPosition; // 0x8
    };

    class Packet : public RootObject {
    public:
        virtual ~Packet();
        virtual void AcquireRef();
        virtual void ReleaseRef();
        bool IsValid();
        void Pack(ByteStream *);
        bool IsBundlableWith(Packet *);
        bool HasFlag(unsigned char flag) { return (m_byTypeFlags & flag) != 0; }

        char m_pad4[0x8 - 0x4];
        Packet *m_pNext; // 0x8
        char m_padC[0x12 - 0xc];
        unsigned char m_byTypeFlags; // 0x12
        char m_pad13[0x28 - 0x13];
        InetAddress m_oSource; // 0x28
    };

    class PacketIn : public Packet {
    public:
        PacketIn();
        bool Unpack(ByteStream *, unsigned int *);
        void SetLocalPort(unsigned short usPort) { m_usLocalPort = usPort; }

        char m_padA8[0xbc - 0xa8];
        unsigned short m_usLocalPort; // 0xbc
    };

    template <class T>
    class qChain : public RootObject {
    public:
        class iterator {
        public:
            iterator(const T &link) : mLink(link) {}
            iterator(const iterator &it) : mLink(it.mLink) {}
            bool operator!=(const iterator &it) const { return !(mLink == it.mLink); }
            const T &operator*() const { return mLink; }

            T mLink; // 0x0
        };

        iterator begin() { return mItFirst; }
        iterator end() { return mItEnd; }

        iterator mItFirst; // 0x0
        iterator mItLast; // 0x4
        iterator mItEnd; // 0x8
        unsigned long mNBLinks; // 0xc
    };

    class PacketQueue : public qChain<Packet *> {
    public:
        void Push(Packet *);
        iterator Erase(iterator);
    };

    class EmulationDevice {
    public:
        unsigned int GetLatency();
        unsigned int GetJitter();
        unsigned int GetBandwidth();
        float GetPacketDropProbability();
    };

    // One emulated packet: the data and where it goes.
    class EmulationItem {
    public:
        EmulationItem(const EmulationItem &o) {
            m_pBuffer = o.GetBuffer();
            m_pBuffer->AcquireRef();
            m_oAddress = o.m_oAddress;
        }
        Buffer *GetBuffer() const { return m_pBuffer; }
        ~EmulationItem() { m_pBuffer->ReleaseRef(); }

        Buffer *m_pBuffer; // 0x0
        InetAddress m_oAddress; // 0x4
    };

    // An item waiting in a queue until its release time.
    class TimedEmulationItem : public EmulationItem {
    public:
        TimedEmulationItem(const EmulationItem &oItem, Time tRelease) : EmulationItem(oItem) {
            m_tRelease = tRelease;
        }
        Time GetReleaseTime() const { return m_tRelease; }

        Time m_tRelease; // 0x88
    };

    class EmulationQueue : public RootObject {
    public:
        EmulationQueue(EmulationDevice *pDevice);
        virtual ~EmulationQueue() {
            while (!m_lstItems.empty()) {
                m_lstItems.pop_front();
            }
        }
        virtual void Queue(EmulationItem &oItem, Time tRelease) {
            std::list<TimedEmulationItem, MemAllocator<TimedEmulationItem> >::reverse_iterator it =
                m_lstItems.rbegin();
            while (bool bMoveOn = it != m_lstItems.rend() && (*it).GetReleaseTime() > tRelease) {
                ++it;
            }
            m_lstItems.insert(it.base(), TimedEmulationItem(oItem, tRelease));
        }
        virtual bool IsReady(Time tNow) {
            return !m_lstItems.empty() && m_lstItems.front().m_tRelease <= tNow;
        }
        virtual EmulationItem Front(Time tNow) { return m_lstItems.front(); }
        virtual void Remove(const EmulationItem &oItem) { m_lstItems.pop_front(); }

        std::list<TimedEmulationItem, MemAllocator<TimedEmulationItem> > m_lstItems; // 0x4
        EmulationDevice *m_pDevice; // 0xc
    };

    // Limits the rate: each item delays the next by its size over the bandwidth.
    class BandwidthEmulationQueue : public EmulationQueue {
    public:
        virtual ~BandwidthEmulationQueue() {}
        virtual void Queue(EmulationItem &oItem, Time tRelease) {
            if (m_tNextFree < tRelease)
                m_tNextFree = tRelease;
            EmulationQueue::Queue(oItem, m_tNextFree);
            if (m_pDevice->GetBandwidth() != -1) {
                m_tNextFree = oItem.GetBuffer()->GetContentSize() * 1000 * 8
                        / m_pDevice->GetBandwidth()
                    + (unsigned __int64)m_tNextFree;
            }
        }

        Time m_tNextFree; // 0x10
    };

    // Adds latency and jitter, and drops some items.
    class LatencyEmulationQueue : public EmulationQueue {
    public:
        virtual ~LatencyEmulationQueue() {}
        virtual void Queue(EmulationItem &oItem, Time tRelease) {
            float rDropProbability = m_pDevice->GetPacketDropProbability();
            if (RandomNumberGenerator::GetRealRandomNumber(1.0f) >= rDropProbability) {
                unsigned int uiJitter = m_pDevice->GetJitter();
                unsigned int uiLatency = m_pDevice->GetLatency();
                if (uiJitter > 0) {
                    EmulationQueue::Queue(
                        oItem,
                        tRelease + uiLatency + RandomNumberGenerator::GetRandomNumber(uiJitter)
                    );
                } else {
                    EmulationQueue::Queue(oItem, tRelease + uiLatency);
                }
            }
        }
    };

    class EmulationChannel : public RootObject {
    public:
        EmulationChannel(EmulationDevice *pDevice);
        bool Queue(Buffer *, InetAddress *);
        bool Dequeue(Buffer **, InetAddress *);
        void Release(Buffer *);

        BandwidthEmulationQueue m_oBandwidthQueue; // 0x0
        LatencyEmulationQueue m_oLatencyQueue; // 0x18
    };

    union UserContextStorage {
        unsigned int m_uiValue;
        float m_dValue;
        unsigned char m_bValue;
        void *m_pPointer;
    };

    class UserContext : public RootObject {
    public:
        UserContext() { m_uContextStorage.m_uiValue = 0; }
        UserContext(void *pPointer) { m_uContextStorage.m_pPointer = pPointer; }
        ~UserContext() {}
        void *GetPointer() const {
            const UserContextStorage &oStorage = m_uContextStorage;
            return oStorage.m_pPointer;
        }

        UserContextStorage m_uContextStorage;
    };

    class IOCompletionContext {
    public:
        void Reset() {
            if (sizeof(void *) == 4) {
                m_pData = 0;
                m_uiSize = 0;
            } else {
                m_uiUnk34 = 0;
                m_uiUnk30 = 0;
            }
        }

        void ClearPending() { m_bPending = false; }
        void ResetTransferred() { m_uiTransferred = 0; }
        UserContext &GetUserContext() { return m_oUserContext; }

        unsigned char *m_pData; // 0x0
        unsigned int m_uiSize; // 0x4
        unsigned int m_uiTransferred; // 0x8
        char m_padC[0x14 - 0xc];
        bool m_bPending; // 0x14
        UserContext m_oUserContext; // 0x18
        char m_pad1C[0x30 - 0x1c];
        unsigned int m_uiUnk30; // 0x30
        unsigned int m_uiUnk34; // 0x34
    };

    class IOCompletionNotifier {
    public:
        IOCompletionContext *CreateIOCompletionContext();
        void DeleteIOCompletionContext(IOCompletionContext *);
        bool WaitForIOCompletion(IOCompletionContext *, unsigned int);
    };

    class Socket {
    public:
        Socket(unsigned int);
        ~Socket();
        bool Bind(InetAddress *);
        InetAddress *GetAddress();
        void Close();
        int Send(unsigned char *, unsigned int, InetAddress *, IOCompletionContext *);
        int Recv(unsigned char *, unsigned int, InetAddress *, IOCompletionContext *);
        unsigned int GetIOResult(IOCompletionContext *);
        bool IsVDP() { return m_bVDP; }

        int m_iState; // 0x0
        char m_pad4[0x98 - 0x4];
        bool m_bVDP; // 0x98
        char m_pad99[0xa0 - 0x99];
    };

    class ProfilingCounters {
    public:
        void Add(unsigned int, unsigned int);
    };

    class RootTransport {
    public:
        char m_pad0[0x18];
        ProfilingCounters m_oCounters; // 0x18
        char m_pad19[0x498 - 0x19];
        EmulationDevice m_oOutputDevice; // 0x498
        char m_pad499[0x4b4 - 0x499];
        EmulationDevice m_oInputDevice; // 0x4b4
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    class InstanceTable {
    public:
        void *GetInstanceFromVector(unsigned int, unsigned int);
    };

    class InstanceControl {
    public:
        static InstanceTable s_oInstanceTable;
        char m_pad0[0x8];
        void *m_pDelegatorInstance; // 0x8
    };

    class TransportDelegator {
    public:
        static TransportDelegator *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(1, uiContext);
            TransportDelegator *pDelegator = inst ? (TransportDelegator *)inst->m_pDelegatorInstance : 0;
            return pDelegator;
        }
        RootTransport *GetTransport() { return m_pTransport; }
        char m_pad[0x4c];
        RootTransport *m_pTransport; // 0x4c
    };

    class QueuingSocket : public Socket {
    public:
        QueuingSocket(IOCompletionNotifier *, unsigned int, RootTransport *);
        ~QueuingSocket();
        void CreateContext(IOCompletionContext **);
        void DeleteContext(IOCompletionContext *);
        bool Bind(InetAddress *, unsigned short *);
        bool Send(Buffer *, InetAddress *);
        bool Queue(Buffer *, InetAddress *);
        bool Flush();
        bool SendBuffer(Buffer *, InetAddress *);
        Buffer *CreateBufferFromPacketQueue(PacketQueue *, unsigned int);
        int CompleteSend();
        void Recv(unsigned int);
        PacketIn *ExtractPacket(const InetAddress *, Buffer *);
        unsigned int FillPacketQueueFromBuffer(Buffer *, const InetAddress *, PacketQueue *);
        Buffer *CompleteBufferRecv();
        int GetSendQueueSize();
        int GetRecvQueueSize();
        int GetNbPendingIOs();

        static RootTransport *GetTransport() {
            TransportDelegator *pDelegator = TransportDelegator::GetInstance();
            if (pDelegator == 0)
                return 0;
            else
                return pDelegator->GetTransport();
        }

        IOCompletionContext *m_pRecvContext; // 0xa0
        IOCompletionContext *m_pSendContext; // 0xa4
        IOCompletionNotifier *m_pNotifier; // 0xa8
        bool m_bSendPending; // 0xac
        bool m_bRecvPending; // 0xad
        InetAddress m_oRecvAddress; // 0xb0
        unsigned int m_uiUnk130; // 0x130
        EmulationChannel m_oOutputChannel; // 0x138
        EmulationChannel m_oInputChannel; // 0x160
    };

}

using namespace Quazal;

QueuingSocket::QueuingSocket(
    IOCompletionNotifier *pNotifier, unsigned int uiFlags, RootTransport *pTransport
)
    : Socket(uiFlags), m_oOutputChannel(&pTransport->m_oOutputDevice),
      m_oInputChannel(&pTransport->m_oInputDevice) {
    m_bSendPending = false;
    m_bRecvPending = false;
    m_pNotifier = pNotifier;
    m_uiUnk130 = 1;
    CreateContext(&m_pSendContext);
    CreateContext(&m_pRecvContext);
}

QueuingSocket::~QueuingSocket() {
    Close();
    DeleteContext(m_pSendContext);
    DeleteContext(m_pRecvContext);
}

void QueuingSocket::CreateContext(IOCompletionContext **ppContext) {
    *ppContext = m_pNotifier->CreateIOCompletionContext();
}

void QueuingSocket::DeleteContext(IOCompletionContext *pContext) {
    if (pContext) {
        if (!m_pNotifier->WaitForIOCompletion(pContext, 1000)) {
            // Retail tests the result and does nothing with it (a trace that
            // is compiled out): the test costs a register and no code.
        }
        Buffer *pBuffer = (Buffer *)pContext->m_oUserContext.m_uContextStorage.m_pPointer;
        if (pBuffer)
            pBuffer->ReleaseRef();
        m_pNotifier->DeleteIOCompletionContext(pContext);
    }
}

bool QueuingSocket::Bind(InetAddress *pAddress, unsigned short *pusPort) {
    if (!Socket::Bind(pAddress))
        return false;
    *pusPort = pAddress->GetPortNumber();
    return true;
}

bool QueuingSocket::Send(Buffer *pBuffer, InetAddress *pAddress) {
    if (pBuffer) {
        bool bResult = SendBuffer(pBuffer, pAddress);
        pBuffer->ReleaseRef();
        return bResult;
    }
    return false;
}

bool QueuingSocket::Queue(Buffer *pBuffer, InetAddress *pAddress) {
    if (pBuffer) {
        m_oOutputChannel.Queue(pBuffer, pAddress);
        pBuffer->ReleaseRef();
        return true;
    }
    return false;
}

bool QueuingSocket::Flush() {
    if (!m_bSendPending) {
        InetAddress oAddress;
        Buffer *pBuffer;
        if (m_oOutputChannel.Dequeue(&pBuffer, &oAddress)) {
            bool bResult = SendBuffer(pBuffer, &oAddress);
            m_oOutputChannel.Release(pBuffer);
            return bResult;
        }
    }
    return false;
}

bool QueuingSocket::SendBuffer(Buffer *pBuffer, InetAddress *pAddress) {
    bool bResult = false;
    if (pBuffer && pBuffer->GetContentSize()) {
        m_pSendContext->GetUserContext() = pBuffer;
        pBuffer->AcquireRef();
        bResult = Socket::Send(
                      pBuffer->GetContentPtr(), pBuffer->GetContentSize(), pAddress, m_pSendContext
                  )
            != 0;
        m_bSendPending = true;
    }
    return bResult;
}

Buffer *QueuingSocket::CreateBufferFromPacketQueue(PacketQueue *pQueue, unsigned int uiMaxSize) {
    PacketQueue::iterator it = pQueue->begin();
    Packet *pCurrentPacket = *it;
    ByteStream *pStream = 0;
    ByteStream *pVDPVoiceStream = 0;
    unsigned int uiNbPackets = 0;
    if (pCurrentPacket && pCurrentPacket->IsValid()) {
#line 167
        pStream = new (__FILE__, __LINE__) ByteStream();
        if (IsVDP())
            *pStream << (unsigned short)0;
        ByteStream oPacketStream;
        while (it != pQueue->end()) {
            oPacketStream.Clear();
            pCurrentPacket->Pack(&oPacketStream);
            unsigned int uiVoiceSize = 0;
            if (pVDPVoiceStream)
                uiVoiceSize = pVDPVoiceStream->GetLength();
            if (pStream->GetLength() + oPacketStream.GetLength() + uiVoiceSize <= uiMaxSize) {
                if (IsVDP() && pCurrentPacket->HasFlag(0x80)) {
                    if (!pVDPVoiceStream)
#line 188
                        pVDPVoiceStream = new (__FILE__, __LINE__) ByteStream();
                    *pVDPVoiceStream << *oPacketStream.GetBuffer();
                } else {
                    *pStream << *oPacketStream.GetBuffer();
                }
                pCurrentPacket->AcquireRef();
                it.mLink = *pQueue->Erase(it);
                uiNbPackets++;
                while (it != pQueue->end() && !(*it)->IsBundlableWith(pCurrentPacket)) {
                    it.mLink = (*it)->m_pNext;
                }
                pCurrentPacket->ReleaseRef();
                pCurrentPacket = *it;
            } else {
                it = pQueue->end();
            }
        }
        if (IsVDP()) {
            unsigned short usSize = pStream->GetLength() - 2;
            pStream->GetBuffer()->AppendData(&usSize, 2, 0);
        }
        if (pVDPVoiceStream) {
            *pStream << *pVDPVoiceStream->GetBuffer();
            delete pVDPVoiceStream;
        }
    }
    Buffer *pBuffer = 0;
    if (pStream && pStream->GetBuffer()) {
        GetTransport()->m_oCounters.Add(0, (pStream->GetLength() + 28) * 8);
        GetTransport()->m_oCounters.Add(2, 1);
        pBuffer = pStream->GetBuffer();
        pBuffer->AcquireRef();
        delete pStream;
    }
    return pBuffer;
}

int QueuingSocket::CompleteSend() {
    int iResult = 0;
    Buffer *pSentBuffer = (Buffer *)m_pSendContext->m_oUserContext.GetPointer();
    m_pSendContext->Reset();
    m_pSendContext->GetUserContext() = (void *)0;
    if (pSentBuffer)
        pSentBuffer->ReleaseRef();
    m_pSendContext->ClearPending();
    m_bSendPending = false;
    return iResult;
}

void QueuingSocket::Recv(unsigned int uiSize) {
    if (m_bRecvPending)
        return;
#line 264
    Buffer *pBuffer = new (__FILE__, __LINE__) Buffer(uiSize);
    m_bRecvPending = true;
    m_pRecvContext->ResetTransferred();
    m_pRecvContext->GetUserContext() = pBuffer;
    Socket::Recv(pBuffer->GetContentPtr(), pBuffer->GetSize(), &m_oRecvAddress, m_pRecvContext);
}

PacketIn *QueuingSocket::ExtractPacket(const InetAddress *pAddress, Buffer *pBuffer) {
#line 276
    PacketIn *pPacket = new (__FILE__, __LINE__) PacketIn();
    pPacket->m_oSource = *pAddress;
    ByteStream oStream(pBuffer);
    unsigned int uiLength;
    if (!pPacket->Unpack(&oStream, &uiLength)) {
        pPacket->ReleaseRef();
        pPacket = 0;
    } else {
        unsigned int uiRemaining = pBuffer->GetContentSize() - uiLength;
        pBuffer->AppendData(pBuffer->GetContentPtr() + uiLength, uiRemaining, 0);
        pBuffer->SetContentSize(uiRemaining);
    }
    return pPacket;
}

unsigned int QueuingSocket::FillPacketQueueFromBuffer(
    Buffer *pBuffer, const InetAddress *pAddress, PacketQueue *pQueue
) {
    unsigned int uiNbPackets = 0;
    bool bEmpty = false;
    if (pBuffer) {
        GetTransport()->m_oCounters.Add(1, (pBuffer->GetContentSize() + 28) * 8);
        GetTransport()->m_oCounters.Add(3, 1);
    }
    if (IsVDP()) {
        if (pBuffer->GetContentSize() > 2) {
            unsigned int uiSize = pBuffer->GetContentSize() - 2;
            pBuffer->AppendData(pBuffer->GetContentPtr() + 2, uiSize, 0);
            pBuffer->SetContentSize(uiSize);
        } else {
            bEmpty = true;
        }
    }
    while (!bEmpty) {
        PacketIn *pPacket = ExtractPacket(pAddress, pBuffer);
        if (pPacket) {
            uiNbPackets++;
            pPacket->SetLocalPort(GetAddress()->GetPortNumber());
            pQueue->Push(pPacket);
        } else {
            bEmpty = true;
        }
    }
    return uiNbPackets;
}

Buffer *QueuingSocket::CompleteBufferRecv() {
    bool bValid = false;
    unsigned int uiSize = 0;
    Buffer *pBuffer = (Buffer *)m_pRecvContext->m_oUserContext.GetPointer();
    m_pRecvContext->Reset();
    m_pRecvContext->GetUserContext() = (void *)0;
    if (pBuffer) {
        unsigned int uiResult = 0;
        if (m_iState != 3) {
            uiResult = GetIOResult(m_pRecvContext);
            uiSize = m_pRecvContext->m_uiTransferred;
            if (uiResult == 0)
                pBuffer->SetContentSize(uiSize);
            if ((m_iState == 0 || m_iState == 1) && uiResult == 0 && uiSize > 0)
                bValid = true;
        }
    }
    m_pRecvContext->ClearPending();
    m_bRecvPending = false;
    if (bValid)
        return pBuffer;
    else {
        pBuffer->ReleaseRef();
        return 0;
    }
}

int QueuingSocket::GetSendQueueSize() { return 0; }
int QueuingSocket::GetRecvQueueSize() { return 0; }
int QueuingSocket::GetNbPendingIOs() { return 0; }
