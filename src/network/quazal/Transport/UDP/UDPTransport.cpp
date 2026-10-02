// Quazal NetZ - .\Transport\UDP\UDPTransport.cpp
//
// Retail TU: .text 0x82B15808..0x82B1A518 (the ctor's EH prefix up to the
// Socket TU, whose ctor 0x82B1A518 is called by QueuingSocket's). Built /Od
// (see objects.json). The TU's own functions end at FindSocket; the rest is
// the COMDAT tail it instantiates (PacketQueue helpers, the ObjectThread and
// TransportJob members, and the sorted socket vector).
//
// The classes this TU touches are declared here with the layouts its code
// reads, rather than taken from the shared Quazal headers.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets.

#include <vector>
#include <list>
#include <algorithm>

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
        size_type max_size() const {
            size_type n = size_type(-1) / sizeof(T);
            return n > 0 ? n : 1;
        }

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
    };

    template <class T>
    class qVector : public std::vector<T, MemAllocator<T> >, public RootObject {
    public:
        typedef typename std::vector<T, MemAllocator<T> >::iterator iterator;
        iterator begin() { return std::vector<T, MemAllocator<T> >::begin(); }
        iterator end() { return std::vector<T, MemAllocator<T> >::end(); }
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

    class String : public RootObject {
    public:
        String(const char *);
        ~String();
        char *m_szContent;
    };

    class DebugString {
    public:
        DebugString(const String &) {}
    };

    class Time : public RootObject {
    public:
        ~Time() {}
        Time &operator=(const Time &);
        static Time GetTime();

        unsigned long long m_ui64Value;
    };

    class qResult {
    public:
        qResult(const int &);
        int m_iReturnCode;
    };

    class Job;

    class Scheduler : public RootObject {
    public:
        void RegisterSpecialDispatchJob(Job *);
        void UnregisterSpecialDispatchJob(Job *);
        static Scheduler *GetInstance();
        static CriticalSection *GetSystemLock() { return &GetInstance()->m_csSystemLock; }

        char m_data[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
    };

    class InstantiationContext : public RootObject {
    public:
        unsigned int GetInstance(unsigned int);
    };

    class SystemError : public RootObject {
    public:
        static void SignalError(const char *, unsigned int, unsigned int, unsigned int);
        static unsigned int GetCurrentThreadID();
        static bool IsMainThread();
        static void SetThreadPriority(unsigned int);
    };

    class InstanceTable : public RootObject {
    public:
        __declspec(noinline) unsigned int GetInstanceFromVector(unsigned int ui, unsigned int idx) {
            if (idx == 0) {
                return m_oDefaultContext.GetInstance(ui);
            } else if (idx >= m_pvContextVector->size()) {
                SystemError::SignalError(0, 0, 0xe0000003, 0);
                return -1;
            } else {
                return (*m_pvContextVector)[idx]->GetInstance(ui);
            }
        }
        InstantiationContext m_oDefaultContext;
        std::vector<InstantiationContext *> *m_pvContextVector;
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

    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = pInstance ? (Core *)pInstance->m_pDelegatorInstance : 0;
            return pCore;
        }
        Scheduler *GetScheduler() const { return m_pScheduler; }
        static void AcquireInstance();
        static void ReleaseInstance();

        static bool s_bUsesThreads;

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

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void AcquireRef();
        virtual void ReleaseRef();
        unsigned int m_uiRefCount;
    };

    class Job : public RefCountedObject {
    public:
        Job(const DebugString &);
        virtual ~Job();
        virtual void Func0C();
        virtual void Execute() = 0;

        char m_data[0x20];
        Time m_tDeadline; // 0x28
        char m_data30[0x8];
    };

    class ObjectThreadRoot : public RootObject {
    public:
        ObjectThreadRoot(const String &);
        virtual ~ObjectThreadRoot();
        virtual void CallObjectMethod() = 0;
        void Launch();
        bool Wait(unsigned int);
        void MethodStarted();
        bool IsRunning() const { return m_bRunning; }
        static void Sleep(unsigned int);

        char m_data[0x10];
        bool m_bRunning; // 0x14
    };

    template <class T, class P>
    class ObjectThread : public ObjectThreadRoot {
    public:
        typedef void (T::*Method)(P);
        ObjectThread(const String &strName) : ObjectThreadRoot(strName) {
            m_pObject = 0;
            m_pfMethod = 0;
        }
        virtual ~ObjectThread() {}
        void Launch(T *pObject, Method pfMethod, P oParam) {
            m_pObject = pObject;
            m_pfMethod = pfMethod;
            m_oParam = oParam;
            m_bAutoDelete = true;
            ObjectThreadRoot::Launch();
        }
        virtual void CallObjectMethod() {
            T *pObject = m_pObject;
            Method pfn = m_pfMethod;
            P pParam = m_oParam;
            MethodStarted();
            (pObject->*pfn)(pParam);
        }

        Method m_pfMethod; // 0x18
        T *m_pObject; // 0x20
        P m_oParam; // 0x24
        bool m_bAutoDelete; // 0x28
    };

    class InetAddress : public RootObject {
    public:
        InetAddress();
        InetAddress(const InetAddress &);
        ~InetAddress();
        InetAddress &operator=(const InetAddress &);
        void SetPortNumber(unsigned short);
        unsigned short GetPortNumber() const;

        unsigned long long m_data[0x80 / 8];
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        ~StationURL();
        enum _URLType {
            Unknown = 0,
            prudp = 1,
            prudps = 2,
            udp = 3
        };
        _URLType GetURLType() const;
        bool SetInetAddress(const InetAddress *);
        InetAddress *GetInetAddress() const;
        void SetPortNumber(unsigned short);
        bool IsValid() const;

        unsigned int m_data[0x64 / 4];
    };

    class Buffer : public RefCountedObject {
    public:
    };

    class Stream {
    public:
        enum Type {
            Unknown = 0
        };
    };

    class VirtualPort {
    public:
        VirtualPort(Stream::Type eType, unsigned char ucPort) { m_ucValue = (unsigned char)((eType << 4) | ucPort); }
        ~VirtualPort() {}
        operator unsigned char() const { return m_ucValue; }
        unsigned char m_ucValue;
    };

    class Packet : public RefCountedObject {
    public:
        virtual void SetFlag(unsigned int);
        const InetAddress &GetDestination() const { return m_oDestination; }
        void SetDestination(const InetAddress *pAddr) { m_oDestination = *pAddr; }
        void SetSourceVPort(const VirtualPort &oVPort) { m_ucSourceVPort = oVPort; }
        void SetDestinationVPort(const VirtualPort &oVPort) { m_ucDestinationVPort = oVPort; }
        void SetTimestamp(Time t) { m_tTimestamp = t; }

        char m_pad08[0x10 - 8];
        unsigned char m_ucSourceVPort; // 0x10
        unsigned char m_ucDestinationVPort; // 0x11
        char m_pad12[0x28 - 0x12];
        InetAddress m_oDestination; // 0x28
        Time m_tTimestamp; // 0xa8
        char m_padB0[0xbc - 0xb0];
        unsigned short m_usLocalPort; // 0xbc
    };

    class PacketOut : public Packet {
    public:
        PacketOut(unsigned char, unsigned char, unsigned int, Buffer *);
        char m_padC0[0xe0 - 0xc0];
    };

    class WaterMark : public RootObject {
    public:
        WaterMark(const char *, bool, unsigned int);
        unsigned int GetValue();
        char m_data[0x30];
    };

    template <class T>
    class qChain : public RootObject {
    public:
        class iterator {
        public:
            iterator(const T &link) : mLink(link) {}
            iterator(const iterator &it) : mLink(it.mLink) {}
            iterator &operator=(const iterator &it) {
                mLink = it.mLink;
                return *this;
            }
            bool operator!=(const iterator &it) const { return !(mLink == it.mLink); }
            T operator*() const { return mLink; }

            T mLink; // 0x0
        };

        qChain() : mItFirst(0), mItLast(0), mItEnd(0), mNBLinks(0) {}
        ~qChain();
        iterator begin() { return mItFirst; }
        iterator end() { return mItEnd; }

        iterator mItFirst; // 0x0
        iterator mItLast; // 0x4
        iterator mItEnd; // 0x8
        unsigned long mNBLinks; // 0xc
    };

    class PacketQueue : public qChain<Packet *> {
    public:
        PacketQueue(const char *szName) : m_oWaterMark(szName, true, 60000) {}
        ~PacketQueue();
        void Purge();
        void Push(Packet *);
        iterator Erase(iterator);
        unsigned long GetSize() const { return mNBLinks; }
        bool IsEmpty() const { return GetSize() == 0; }
        Packet *Front() const { return mItFirst.mLink; }

        WaterMark m_oWaterMark; // 0x10
    };

    class ProtectedPacketQueue : public PacketQueue {
    public:
        ProtectedPacketQueue(const char *szName, CriticalSection *pCS) : PacketQueue(szName) {
            m_pCS = pCS;
        }
        ~ProtectedPacketQueue() { Purge(); }
        void Purge() {
            ScopedCS oCS(*m_pCS);
            PacketQueue::Purge();
        }
        void Push(Packet *pPacket) {
            ScopedCS oCS(*m_pCS);
            PacketQueue::Push(pPacket);
        }
        iterator Erase(iterator it) {
            ScopedCS oCS(*m_pCS);
            return PacketQueue::Erase(it);
        }

        CriticalSection *m_pCS; // 0x40
    };

    class ReceivedPacketQueue : public RootObject {
    public:
        void Push(Buffer *, const InetAddress *);
        bool Pop(Buffer **, InetAddress *);
        void Release(Buffer *);
    };

    class IOCompletionContext : public RootObject {
    public:
        enum State {
            Idle = 0,
            Pending = 1,
            Completed = 2
        };
        bool IsCompleted() const { return m_ucState == Completed; }
        char m_data[0x14];
        unsigned char m_ucState; // 0x14
    };

    class IOCompletionNotifier : public RootObject {
    public:
        IOCompletionNotifier();
        ~IOCompletionNotifier();
        bool Wait(unsigned int);
        unsigned int m_data[0x3c / 4];
    };

    class InterfaceTable : public RootObject {
    public:
        InterfaceTable();
        ~InterfaceTable();
        unsigned int m_data[0x8 / 4];
    };

    class RootTransport;
    class Router;
    class BandwidthCounter;

    class Socket : public RootObject {
    public:
        bool Open(bool);
        void Close();
        InetAddress *GetLocalAddress();
    };

    class QueuingSocket : public Socket {
    public:
        QueuingSocket(IOCompletionNotifier *, unsigned int, RootTransport *);
        ~QueuingSocket();
        bool Bind(InetAddress *, unsigned short *);
        void Recv(BandwidthCounter *);
        Buffer *GetReceivedBuffer();
        void *FilterIncoming(Buffer *, const InetAddress *, PacketQueue *);
        void SendCompleted();
        Buffer *PrepareOutgoing(ProtectedPacketQueue *, void *);
        bool SendTo(Buffer *, InetAddress *);
        bool SendToEmulated(Buffer *, InetAddress *);
        bool FlushEmulated();
        void SetBandwidthCounter(BandwidthCounter *pCounter) { m_pBandwidthCounter = pCounter; }
        unsigned int GetBufferSize() const { return m_uiBufferSize; }
        unsigned int GetRefCount() const { return m_uiRefCount; }
        bool IsSendPending() const { return m_bSendPending; }

        char m_pad00[0x8c];
        unsigned int m_uiBufferSize; // 0x8c
        char m_pad90[0x94 - 0x90];
        BandwidthCounter *m_pBandwidthCounter; // 0x94
        char m_pad98[0xa0 - 0x98];
        IOCompletionContext *m_pRecvContext; // 0xa0
        IOCompletionContext *m_pSendContext; // 0xa4
        char m_padA8[0xac - 0xa8];
        bool m_bSendPending; // 0xac
        char m_padAD[0xb0 - 0xad];
        InetAddress m_oFromAddress; // 0xb0
        unsigned int m_uiRefCount; // 0x130
        char m_pad134[0x160 - 0x134];
        ReceivedPacketQueue m_oReceivedQueue; // 0x160
        char m_pad161[0x188 - 0x161];
    };

    class Router : public RootObject {
    public:
        Router();
        ~Router();
        void SetTransport(RootTransport *);
        void *GetRoutingTable();
        bool IsRouted(InetAddress *);
        void Route(Buffer *, InetAddress *, unsigned short);
        unsigned long long m_data[0x80 / 8];
    };

    class VirtualNATDevice : public RootObject {
    public:
        virtual ~VirtualNATDevice();
        virtual bool Send(StationURL &);
    };

    class BandwidthCounter : public RootObject {
    public:
    };

    class PacketDispatcher : public RootObject {
    public:
        void Dispatch(Packet *);
        void Flush();
        char m_data[0x14];
    };

    class NetworkInterfaces {
    public:
        virtual void Unk00();
        virtual void GetLocalURLs(qList<StationURL> *);
        static NetworkInterfaces *GetInstance() { return s_pInstance; }
        static NetworkInterfaces *s_pInstance;
    };

    class Network : public RootObject {
    public:
        static Network *GetInstance();
        void AddLocalURL(StationURL &);
        void RemoveLocalURL(StationURL &);
        CriticalSection *GetLock();
    };

    class HighResolutionChrono : public RootObject {
    public:
        HighResolutionChrono();
        ~HighResolutionChrono();
        unsigned long long m_data[0x10 / 8];
    };

    class ProfilingUnit : public RootObject {
    public:
        ProfilingUnit(const char *, unsigned int);
        ~ProfilingUnit();
        unsigned long long m_data[0x48 / 8];
    };

    class ProfilingScope : public RootObject {
    public:
        ProfilingScope(ProfilingUnit *pUnit) {
            m_pUnit = pUnit;
            m_bStopped = false;
        }
        ~ProfilingScope() {
            if (!m_bStopped) {
                Stop();
            }
        }
        void Stop();

        ProfilingUnit *m_pUnit; // 0x0
        char m_pad04[4];
        HighResolutionChrono m_oChrono; // 0x8
        bool m_bStopped; // 0x18
    };

    class Inet : public RootObject {
    public:
        static bool Initialize();
        static void Terminate();
    };

    class RootTransport : public RootObject {
    public:
        RootTransport();
        virtual ~RootTransport();
        virtual qResult Initialize() = 0;
        virtual bool StartListen(unsigned short, unsigned short *, bool, unsigned int) = 0;
        virtual bool StopListen(unsigned short) = 0;
        virtual bool StopListen() = 0;
        virtual unsigned int GetNbListeningPorts() = 0;
        virtual bool Send(unsigned short, Stream::Type, unsigned char, unsigned char, PacketOut *, bool) = 0;
        virtual qResult Send(StationURL *, Buffer *) = 0;
        virtual bool Receive(unsigned short, Buffer *, const InetAddress *) = 0;
        virtual Router *GetRouter() = 0;
        virtual void Func28();
        virtual unsigned int GetPacketQueueSize() const = 0;
        virtual unsigned int GetEstimatedPacketQueueMemoryUsage() const = 0;
        virtual void StopTransportThreadImpl() = 0;

        bool ReceiveBuffer(Buffer *, InetAddress *);
        void DetachVNATDevice(VirtualNATDevice *);
        BandwidthCounter *GetBandwidthCounter() const { return m_pBandwidthCounter; }
        bool IsSendBuffered() const { return m_bSendBuffered; }
        bool IsEmulationEnabled() const { return m_bEmulationEnabled; }
        bool UsesReceiveQueue() const { return m_bUsesReceiveQueue; }
        VirtualNATDevice *GetVNATDevice() const { return m_pVNATDevice; }
        unsigned short GetDefaultPort() const { return m_usDefaultPort; }

        char m_pad04[0x8 - 0x4];
        BandwidthCounter *m_pBandwidthCounter; // 0x8
        char m_pad0C[0x10 - 0xc];
        bool m_bSendBuffered; // 0x10
        char m_pad11[0x49c - 0x11];
        bool m_bEmulationEnabled; // 0x49c
        char m_pad49D[0x4b8 - 0x49d];
        bool m_bUsesReceiveQueue; // 0x4b8
        char m_pad4B9[0x4d0 - 0x4b9];
        PacketDispatcher m_oDispatcher; // 0x4d0
        VirtualNATDevice *m_pVNATDevice; // 0x4e4
        unsigned short m_usDefaultPort; // 0x4e8
        char m_pad4EA[0x4f8 - 0x4ea];
    };

    template <class K, class V>
    class qSortedVector : public qVector<std::pair<K, V> > {
    public:
        typedef std::pair<K, V> value_type;
        typedef std::vector<value_type, MemAllocator<value_type> > base_vector;
        typedef typename qVector<value_type>::iterator iterator;

        struct KeyCompare {
            bool operator()(const value_type &a, const K &b) const { return a.first < b; }
            bool operator()(const K &a, const value_type &b) const { return a < b.first; }
        };

        qSortedVector() { this->reserve(2); }

        std::pair<iterator, bool> insert(const value_type &oValue);
        iterator find(const K &key) {
            KeyCompare oCompare;
            iterator it = std::lower_bound(base_vector::begin(), base_vector::end(), key, oCompare);
            if (it != base_vector::end() && oCompare(key, *it)) {
                it = base_vector::end();
            }
            return it;
        }
        unsigned int erase(const K &key);
    };

    template <class K, class V>
    std::pair<typename qSortedVector<K, V>::iterator, bool> qSortedVector<K, V>::insert(const value_type &oValue) {
        bool bInserted = false;
        KeyCompare oCompare;
        iterator it = std::lower_bound(base_vector::begin(), base_vector::end(), oValue.first, oCompare);
        if (it == base_vector::end() || oCompare(oValue.first, *it)) {
            it = qVector<value_type>::insert(it, oValue);
            bInserted = true;
        }
        return std::make_pair(it, bInserted);
    }

    template <class K, class V>
    unsigned int qSortedVector<K, V>::erase(const K &key) {
        iterator it = find(key);
        if (it != base_vector::end()) {
            qVector<value_type>::erase(it);
            return 1;
        } else {
            return 0;
        }
    }

    class UDPTransport : public RootTransport {
    public:
        class TransportJob : public Job {
        public:
            TransportJob(UDPTransport *pTransport, const String &strName) : Job(strName) {
                m_pTransport = pTransport;
            }
            virtual ~TransportJob() {}
            virtual void Execute() { m_pTransport->TransportJobMethod(); }

            UDPTransport *m_pTransport; // 0x38
        };

        UDPTransport();
        virtual ~UDPTransport();
        virtual qResult Initialize();
        virtual bool StartListen(unsigned short, unsigned short *, bool, unsigned int);
        virtual bool StopListen(unsigned short);
        virtual bool StopListen();
        virtual unsigned int GetNbListeningPorts();
        virtual bool Send(unsigned short, Stream::Type, unsigned char, unsigned char, PacketOut *, bool);
        virtual qResult Send(StationURL *, Buffer *);
        virtual bool Receive(unsigned short, Buffer *, const InetAddress *);
        virtual Router *GetRouter();
        virtual unsigned int GetPacketQueueSize() const;
        virtual unsigned int GetEstimatedPacketQueueMemoryUsage() const;
        virtual void StopTransportThreadImpl() { m_bStopping = true; }

        void StartEventListener();
        bool BindSocket(unsigned short, unsigned short *, unsigned int);
        bool Receive(QueuingSocket *, Buffer *, const InetAddress *);
        void ServiceIOCompletions();
        void TransportThread(void *);
        void TransportJobMethod();
        void DeliverOutgoing();
        void DispatchIncoming();
        QueuingSocket *FindSocket(unsigned short);

        static WaterMark s_wmPacketQueue;

        ObjectThread<UDPTransport, void *> *m_pThread; // 0x4f8
        TransportJob *m_pTransportJob; // 0x4fc
        qSortedVector<unsigned short, QueuingSocket *> m_vSockets; // 0x500
        IOCompletionNotifier m_oIOCompletionNotifier; // 0x510
        bool m_bStopping; // 0x54c
        InterfaceTable m_oInterfaceTable; // 0x550
        PacketQueue m_oIncomingQueue; // 0x558
        ProtectedPacketQueue m_oOutgoingQueue; // 0x598
        ProfilingUnit m_oProfilingUnit; // 0x5e0
        unsigned int m_uiThreadID; // 0x628
        Router m_oRouter; // 0x630
    };

    unsigned int g_uiTransportWaitTime;
    unsigned int g_uiTransportSleepTime;

    UDPTransport::UDPTransport()
        : m_oIncomingQueue("UDP Incoming Queue Size"),
          m_oOutgoingQueue("UDP Outgoing Queue Size", Scheduler::GetSystemLock()),
          m_oProfilingUnit("Transport Job", 1000) {
        Core::AcquireInstance();
        m_bStopping = false;
        m_pThread = 0;
        m_pTransportJob = 0;
        m_uiThreadID = SystemError::GetCurrentThreadID();
        m_oRouter.SetTransport(this);
    }

    UDPTransport::~UDPTransport() {
        m_bStopping = true;
        if (m_pThread != 0 && m_pThread->IsRunning()) {
            m_pThread->Wait(0xFFFFFFFF);
            delete m_pThread;
        }
        if (m_pTransportJob != 0) {
            if (Scheduler::GetInstance() != 0) {
                Scheduler::GetInstance()->UnregisterSpecialDispatchJob(m_pTransportJob);
                m_pTransportJob->ReleaseRef();
            }
        }
        if (GetVNATDevice() != 0) {
            DetachVNATDevice(GetVNATDevice());
        }
        StopListen();
        m_oOutgoingQueue.Purge();
        m_oIncomingQueue.Purge();
        Inet::Terminate();
        Core::ReleaseInstance();
    }

    qResult UDPTransport::Initialize() {
        if (!Inet::Initialize()) {
            return 0x8001000C;
        }
        StartEventListener();
        return 0x10001;
    }

    void UDPTransport::StartEventListener() {
        if (Core::s_bUsesThreads) {
            if (m_pThread == 0) {
                m_pThread = new (__FILE__, 0x77) ObjectThread<UDPTransport, void *>("Transport Thread");
                m_pThread->Launch(this, &UDPTransport::TransportThread, 0);
            }
        } else {
            if (m_pTransportJob == 0 && Scheduler::GetInstance() != 0) {
                m_pTransportJob = new (__FILE__, 0x7d) TransportJob(this, "UDPTransport::TransportJob");
                m_pTransportJob->AcquireRef();
                Scheduler::GetInstance()->RegisterSpecialDispatchJob(m_pTransportJob);
            }
        }
    }

    bool UDPTransport::BindSocket(unsigned short usPort, unsigned short *pusBoundPort, unsigned int uiBufferSize) {
        QueuingSocket *pSocket = new (__FILE__, 0x8b) QueuingSocket(&m_oIOCompletionNotifier, uiBufferSize, this);
        pSocket->m_pBandwidthCounter = GetBandwidthCounter();
        if (!pSocket->Open(true)) {
            delete pSocket;
            return false;
        }
        InetAddress oAddress;
        oAddress.SetPortNumber(usPort);
        if (!pSocket->Bind(&oAddress, pusBoundPort)) {
            pSocket->Close();
            delete pSocket;
            *pusBoundPort = 0;
            return false;
        }
        m_vSockets.insert(std::make_pair(*pusBoundPort, pSocket));
        pSocket->Recv(GetBandwidthCounter());
        return true;
    }

    bool UDPTransport::StartListen(
        unsigned short usPort, unsigned short *pusNewPort, bool bAddURLs, unsigned int uiBufferSize
    ) {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        bool bSetDefault = false;
        unsigned short usRequestedPort = usPort;
        unsigned short usNewPort = 0;
        if (usPort == 0 && !m_vSockets.empty()
            && m_vSockets.begin()->second->GetBufferSize() == uiBufferSize) {
            usRequestedPort = m_vSockets.begin()->first;
        }
        if (usNewPort == 0) {
            QueuingSocket *pSocket = FindSocket(usRequestedPort);
            if (pSocket != 0) {
                pSocket->m_uiRefCount++;
                usNewPort = pSocket->GetLocalAddress()->GetPortNumber();
            }
        }
        if (usNewPort == 0) {
            if (BindSocket(usRequestedPort, &usNewPort, uiBufferSize)) {
                if (bAddURLs) {
                    bSetDefault = true;
                    qList<StationURL> lstURLs;
                    NetworkInterfaces::GetInstance()->GetLocalURLs(&lstURLs);
                    while (!lstURLs.empty()) {
                        lstURLs.front().SetPortNumber(usNewPort);
                        Network::GetInstance()->AddLocalURL(lstURLs.front());
                        lstURLs.pop_front();
                    }
                }
            } else {
                return false;
            }
        }
        if (pusNewPort != 0) {
            *pusNewPort = usNewPort;
        }
        if (bSetDefault) {
            m_usDefaultPort = usNewPort;
        }
        return true;
    }

    bool UDPTransport::StopListen() {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        while (!m_vSockets.empty()) {
            if (!StopListen(m_vSockets.begin()->first)) {
                return false;
            }
        }
        return true;
    }

    bool UDPTransport::StopListen(unsigned short usPort) {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        QueuingSocket *pSocket = FindSocket(usPort);
        if (pSocket != 0) {
            if (pSocket->GetRefCount() == 1) {
                m_vSockets.erase(usPort);
                pSocket->Close();
                delete pSocket;
                if (Network::GetInstance() != 0) {
                    qList<StationURL> lstURLs;
                    NetworkInterfaces::GetInstance()->GetLocalURLs(&lstURLs);
                    while (!lstURLs.empty()) {
                        lstURLs.front().SetPortNumber(usPort);
                        ScopedCS oNetCS(*Network::GetInstance()->GetLock());
                        Network::GetInstance()->RemoveLocalURL(lstURLs.front());
                        lstURLs.pop_front();
                    }
                }
            } else {
                pSocket->m_uiRefCount--;
            }
            return true;
        } else {
            return false;
        }
    }

    unsigned int UDPTransport::GetNbListeningPorts() {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        return m_vSockets.size();
    }

    bool UDPTransport::Send(
        unsigned short usPort,
        Stream::Type eType,
        unsigned char ucSourcePort,
        unsigned char ucDestinationPort,
        PacketOut *pPacket,
        bool bNoDelivery
    ) {
        if (ucSourcePort == 0) {
            return false;
        }
        ScopedCS oCS(*Scheduler::GetSystemLock());
        bool bSent = false;
        if (GetVNATDevice() != 0) {
            StationURL oURL;
            oURL.SetInetAddress(&pPacket->m_oDestination);
            bSent = GetVNATDevice()->Send(oURL);
            pPacket->SetDestination(oURL.GetInetAddress());
        }
        if (bSent) {
            return true;
        }
        pPacket->SetTimestamp(Time::GetTime());
        pPacket->SetSourceVPort(VirtualPort(eType, ucSourcePort));
        pPacket->SetDestinationVPort(VirtualPort(eType, ucDestinationPort));
        pPacket->m_usLocalPort = usPort;
        m_oOutgoingQueue.Push(pPacket);
        pPacket->SetFlag(0x4000000);
        if (IsSendBuffered() && !bNoDelivery) {
            DeliverOutgoing();
        }
        return true;
    }

    qResult UDPTransport::Send(StationURL *pURL, Buffer *pBuffer) {
        if (pURL->GetURLType() != StationURL::udp) {
            return 0x80050003;
        }
        if (!pURL->IsValid()) {
            return 0x80050003;
        }
        PacketOut *pPacket;
        if (pBuffer != 0) {
            pPacket = new (__FILE__, 0x166) PacketOut(0, 7, 0, pBuffer);
            pPacket->SetDestination(pURL->GetInetAddress());
        } else {
            return 0x8001000A;
        }
        pPacket->m_usLocalPort = GetDefaultPort();
        m_oOutgoingQueue.Push(pPacket);
        pPacket->SetFlag(0x4000000);
        pPacket->ReleaseRef();
        return 0x10001;
    }

    bool UDPTransport::Receive(QueuingSocket *pSocket, Buffer *pBuffer, const InetAddress *pFrom) {
        bool bResult = true;
        if (pSocket->FilterIncoming(pBuffer, pFrom, &m_oIncomingQueue) == 0) {
            InetAddress oFrom(*pFrom);
            bResult = !ReceiveBuffer(pBuffer, &oFrom);
        }
        return bResult;
    }

    bool UDPTransport::Receive(unsigned short usPort, Buffer *pBuffer, const InetAddress *pFrom) {
        bool bResult = false;
        QueuingSocket *pSocket = FindSocket(usPort);
        if (pSocket != 0) {
            bResult = Receive(pSocket, pBuffer, pFrom);
        }
        return bResult;
    }

    Router *UDPTransport::GetRouter() { return &m_oRouter; }

    void UDPTransport::ServiceIOCompletions() {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        qSortedVector<unsigned short, QueuingSocket *>::iterator itSocket;
        for (itSocket = m_vSockets.begin(); itSocket != m_vSockets.end(); ++itSocket) {
            QueuingSocket *pSocket = itSocket->second;
            if (pSocket->m_pRecvContext->IsCompleted()) {
                Buffer *pBuffer = pSocket->GetReceivedBuffer();
                if (pBuffer != 0) {
                    if (!UsesReceiveQueue()) {
                        Receive(pSocket, pBuffer, &pSocket->m_oFromAddress);
                    } else {
                        pSocket->m_oReceivedQueue.Push(pBuffer, &pSocket->m_oFromAddress);
                    }
                    pBuffer->ReleaseRef();
                }
                pSocket->Recv(GetBandwidthCounter());
            }
            if (UsesReceiveQueue()) {
                Buffer *pQueued;
                {
                    InetAddress oFrom;
                    while (pSocket->m_oReceivedQueue.Pop(&pQueued, &oFrom)) {
                        Receive(pSocket, pQueued, &oFrom);
                        pSocket->m_oReceivedQueue.Release(pQueued);
                    }
                }
            }
            DispatchIncoming();
            if (pSocket->m_pSendContext->IsCompleted()) {
                pSocket->SendCompleted();
                DeliverOutgoing();
            }
        }
    }

    void UDPTransport::TransportThread(void *) {
        if (SystemError::IsMainThread()) {
            SystemError::SetThreadPriority(m_uiThreadID);
        }
        while (!m_bStopping) {
            if (m_vSockets.empty()) {
                ObjectThreadRoot::Sleep(50);
            } else {
                bool bService = false;
                unsigned int uiWaitTime = g_uiTransportWaitTime;
                if (m_oIOCompletionNotifier.Wait(uiWaitTime)) {
                    bService = true;
                } else if (g_uiTransportSleepTime > 0) {
                    ObjectThreadRoot::Sleep(g_uiTransportSleepTime);
                }
                if (UsesReceiveQueue() || IsEmulationEnabled()) {
                    bService = true;
                }
                if (bService) {
                    ServiceIOCompletions();
                }
                m_oDispatcher.Flush();
                DispatchIncoming();
                DeliverOutgoing();
            }
        }
    }

    void UDPTransport::TransportJobMethod() {
        if (m_vSockets.empty()) {
            return;
        }
        ProfilingScope oScope(&m_oProfilingUnit);
        DispatchIncoming();
        DeliverOutgoing();
        m_oDispatcher.Flush();
        if (UsesReceiveQueue() || IsEmulationEnabled()) {
            ServiceIOCompletions();
            DispatchIncoming();
            DeliverOutgoing();
            m_oDispatcher.Flush();
        }
        while (m_oIOCompletionNotifier.Wait(0)) {
            ServiceIOCompletions();
            DispatchIncoming();
            DeliverOutgoing();
            m_oDispatcher.Flush();
        }
    }

    void UDPTransport::DeliverOutgoing() {
        bool bStop = false;
        ScopedCS oCS(*m_oOutgoingQueue.m_pCS);
        while (!m_oOutgoingQueue.IsEmpty() && !bStop) {
            PacketOut *pPacket = (PacketOut *)m_oOutgoingQueue.Front();
            unsigned short usSrcPort = pPacket->m_usLocalPort;
            if (QueuingSocket *pSocket = FindSocket(usSrcPort)) {
                if (!IsEmulationEnabled() && pSocket->IsSendPending()) {
                    bStop = true;
                } else {
                    InetAddress oDestination(pPacket->GetDestination());
                    Buffer *pBuffer = pSocket->PrepareOutgoing(&m_oOutgoingQueue, m_oRouter.GetRoutingTable());
                    if (pBuffer == 0) {
                        bStop = true;
                    } else if (m_oRouter.IsRouted(&oDestination)) {
                        m_oRouter.Route(pBuffer, &oDestination, usSrcPort);
                    } else if (!IsEmulationEnabled()) {
                        if (!pSocket->SendTo(pBuffer, &oDestination)) {
                            bStop = true;
                        }
                    } else {
                        if (!pSocket->SendToEmulated(pBuffer, &oDestination)) {
                            bStop = true;
                        }
                    }
                }
            } else {
                m_oOutgoingQueue.Erase(PacketQueue::iterator(pPacket));
            }
        }
        if (IsEmulationEnabled()) {
            for (qSortedVector<unsigned short, QueuingSocket *>::iterator it = m_vSockets.begin();
                 it != m_vSockets.end();
                 ++it) {
                QueuingSocket *pSocket = it->second;
                while (pSocket->FlushEmulated()) {
                }
            }
        }
    }

    void UDPTransport::DispatchIncoming() {
        PacketQueue::iterator it = m_oIncomingQueue.begin();
        while (it != m_oIncomingQueue.end()) {
            Packet *pPacket = *it;
            pPacket->SetTimestamp(Time::GetTime());
            m_oDispatcher.Dispatch(pPacket);
            it = m_oIncomingQueue.Erase(it);
        }
    }

    QueuingSocket *UDPTransport::FindSocket(unsigned short usPort) {
        ScopedCS oCS(*Scheduler::GetSystemLock());
        qSortedVector<unsigned short, QueuingSocket *>::iterator it = m_vSockets.find(usPort);
        if (it == m_vSockets.end()) {
            return 0;
        } else {
            return it->second;
        }
    }

    unsigned int UDPTransport::GetPacketQueueSize() const { return s_wmPacketQueue.GetValue(); }

    unsigned int UDPTransport::GetEstimatedPacketQueueMemoryUsage() const {
        return s_wmPacketQueue.GetValue() * 0x50c;
    }

}
