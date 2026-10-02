// Quazal NetZ - .\Transport\Interface\NATTraversalEngine.cpp
//
// Retail TU: .text 0x82B03480..0x82B07280. Built /Od (see objects.json).
//
// The classes this TU touches are declared here with their retail X360
// layouts rather than taken from the shared Quazal headers, which do not
// match them:
// - RootObject has an empty, user-declared destructor: retail's EH unwind
//   maps call it for every RootObject base (ScopedCS locals, the engine and
//   URLProbe at +8).
// - A class with a vfptr and an 8-byte-aligned member (Time) pads the vfptr
//   to 8 bytes, so the first base/member of NATTraversalEngine and URLProbe
//   sits at +8.
// - StationURL is 0x64 bytes, word-aligned, with no vfptr.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets.

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
        typedef typename std::list<T, MemAllocator<T> >::iterator iterator;
        typedef typename std::list<T, MemAllocator<T> >::const_iterator const_iterator;
        iterator begin() { return std::list<T, MemAllocator<T> >::begin(); }
        const_iterator begin() const { return std::list<T, MemAllocator<T> >::begin(); }
        iterator erase(iterator it) { return std::list<T, MemAllocator<T> >::erase(it); }
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
        qList<T> &GetList() { return mOList; }
        void PushBack(const T &t) {
            ScopedCS oCS(mCSList);
            mOList.push_back(t);
        }
        void PopBack() {
            ScopedCS oCS(mCSList);
            mOList.pop_back();
        }
        bool IsEmpty() const {
            ScopedCS oCS(mCSList);
            return mOList.empty();
        }

        mutable CriticalSection mCSList;
        qList<T> mOList;
    };

    class TimeInterval {
    public:
        operator int() const { return m_iValue; }
        int m_iValue;
    };

    class Time : public RootObject {
    public:
        Time() : m_ui64Value(0) {}
        Time(unsigned long long ui64Value) : m_ui64Value(ui64Value) {}
        ~Time() {}
        Time &operator=(const Time &);
        Time operator+(int) const;
        Time operator+(TimeInterval) const;
        bool operator>(const Time &t) const { return m_ui64Value > t.m_ui64Value; }
        bool operator<(const Time &t) const { return m_ui64Value < t.m_ui64Value; }
        static Time GetTime();

        unsigned long long m_ui64Value;
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        StationURL(const char *);
        StationURL(const StationURL &);
        ~StationURL();
        StationURL &operator=(const StationURL &);
        bool operator!=(const StationURL &) const;
        enum _URLType {
            Unknown = 0,
            prudp = 1,
            prudps = 2,
            udp = 3
        };
        _URLType GetURLType() const;
        unsigned int GetRVConnectionID() const;
        void SetRVConnectionID(unsigned int);
        unsigned int GetType() const;
        bool IsPublic() const { return (GetType() & 2) == 2; }

        unsigned int m_data[0x64 / 4]; // word-aligned, as the real class (pointer members)
    };

    class Buffer : public RootObject {
    public:
        Buffer(unsigned int);
        Buffer(const char *);
        virtual ~Buffer();
        virtual void AcquireRef();
        virtual void ReleaseRef();
        void AppendData(const void *, unsigned int, unsigned int);
        Buffer &operator+=(const Buffer &);
        unsigned char *GetContentPtr() const;
        unsigned int GetContentSize() const;

        char m_data[0x10];
    };

    class ByteStream : public RootObject {
    public:
        ByteStream();
        ByteStream(Buffer *);
        virtual ~ByteStream();
        void Append(const unsigned char *, unsigned int, unsigned int);
        void Append(const Time *);
        void Append(const unsigned char &uc) { Append(&uc, 1, 1); }
        bool Extract(unsigned char *, unsigned int, unsigned int);
        bool Extract(Time *);
        ByteStream &operator>>(Buffer &);
        Buffer *GetBuffer() const { return m_pBuffer; }

        Buffer *m_pBuffer; // 0x4
        char m_data[0x4];
    };

    class URLProbe : public RootObject {
    public:
        URLProbe(const StationURL &, int, bool);
        virtual ~URLProbe() {}
        bool UpdateIsNeeded(Time);
        void Refresh(int iLifetime) { m_tiExpiration = Time::GetTime() + iLifetime; }
        const StationURL &GetURL() const { return m_oURL; }
        bool IsProbeRequested() const { return m_bProbeRequested; }
        void SetProbeRequested(bool b) { m_bProbeRequested = b; }
        bool IsExpired(Time t) const { return m_tiExpiration < t; }
        void IncNbProbes() { m_uiNbProbes++; }

        StationURL m_oURL; // 0x08
        Time m_tiExpiration; // 0x70
        unsigned int m_uiPingTime; // 0x78
        Time m_tiLastProbe; // 0x80
        bool m_bProbeRequested; // 0x88
        unsigned int m_uiNbProbes; // 0x8c
    };

    class URLProbeList : public qProtectedList<URLProbe> {
    public:
        URLProbeList();
        virtual ~URLProbeList();
        void UpdateProbe(const StationURL &, Time);
        URLProbe *FindProbe(const StationURL &);
        void Trace(unsigned int);
    };

    class NATTraversalEngine;
    class RootTransport;

    class NATTraversalStream : public RootObject {
    public:
        NATTraversalStream(NATTraversalEngine *, RootTransport *);
        virtual ~NATTraversalStream();
        void SendMsg(const StationURL &, Buffer *);

        char m_data[0x14];
    };

    class NATRelayInterface : public RootObject {
    public:
        virtual ~NATRelayInterface();
        virtual void RequestProbeInitiationExt(
            const qList<StationURL> &, const StationURL &
        ) = 0;
        virtual void RequestProbeInitiation(const qList<StationURL> &) = 0;

        void SetEngine(NATTraversalEngine *pEngine) { m_pEngine = pEngine; }

        NATTraversalEngine *m_pEngine; // 0x4
    };

    class NATEchoInterface : public RootObject {
    public:
        virtual ~NATEchoInterface();
        virtual void Process(Buffer *, Buffer *, StationURL *) = 0;
    };

    class NATDirectInterface : public RootObject {
    public:
        virtual ~NATDirectInterface();
        virtual bool GetPendingData(unsigned char **, unsigned int *) = 0;
        virtual void Send(const StationURL &, unsigned char *, unsigned int) = 0;
        void SetURL(const StationURL &url) { m_oURL = url; }

        StationURL m_oURL; // 0x4
    };

    class Job : public RootObject {
    public:
        virtual ~Job();
    };

    class JobGetPublicURL : public Job {
    public:
        JobGetPublicURL(void *, NATTraversalEngine *);
        virtual void Func1();
        virtual void Func2();
        virtual void Func3();
        virtual void Func4();
        virtual void Func5();
        virtual void Func6();
        virtual void Func7();
        virtual void Func8();
        virtual void Func9();
        virtual void SetPublicURL();

        char m_data[0x44];
    };

    class URLRequester : public RootObject {
    public:
        bool IsReady();
        void *GetContext() const { return m_pContext; }
        char m_data[0x34];
        void *m_pContext; // 0x34
    };

    class Scheduler : public RootObject {
    public:
        void Queue(Job *, bool);
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

    class TraceLog : public RootObject {
    public:
        static TraceLog *GetInstance();
        TraceLog *GetOutput();
        void EnableFlag(unsigned int);
        void DisableFlag(unsigned int);
    };


    class Network : public RootObject {
    public:
        static Network *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(1, uiContext);
            Network *pNetwork = 0;
            if (pInstance != 0) {
                pNetwork = (Network *)pInstance->m_pDelegatorInstance;
            }
            return pNetwork;
        }
        qProtectedList<StationURL> *GetStationURLs();
    };

    class NATTraversalEngine : public RootObject {
    public:
        enum Msg {
            ProbeRequest = 0,
            ProbeReply = 1,
            Echo = 2
        };

        NATTraversalEngine();
        virtual ~NATTraversalEngine();
        virtual bool PrepareNATTraversal(const StationURL &);
        virtual bool PrepareNATTraversal(const qList<StationURL> &);
        virtual bool Initialize();
        virtual bool Terminate();

        void SetLocalCID(unsigned int);
        unsigned int GetLocalCID() const;
        bool StartStream(RootTransport *);
        bool StopStream();
        bool AddURLToProbe(const StationURL &, bool);
        void Execute();
        void SendProbe(Msg, const StationURL &, Time);
        void SendEcho();
        void ReceiveMessage(const StationURL &, const unsigned char *, unsigned int);
        bool GetPublicURL(int, StationURL *);
        bool RequestPublicURL(URLRequester *, int);
        void ClearPublicURLJob();
        void ReceiveProbe(Msg, const StationURL &, unsigned int, Time);
        bool GetUpdatedURL(const StationURL &, StationURL *);
        unsigned int GetURLPingTime(const StationURL &);
        bool RegisterDirect(int, NATDirectInterface *);
        bool UnregisterDirect(int);
        bool RegisterRelay(NATRelayInterface *);
        bool UnregisterRelay();
        bool RegisterEcho(NATEchoInterface *);
        bool UnregisterEcho();
        void Trace(unsigned int);

        static TimeInterval s_tiProbeLifetime;
        static TimeInterval s_tiFrequency;

        unsigned int m_uiLocalCID; // 0x08
        NATTraversalStream *m_pStream; // 0x0c
        URLProbeList m_oProbes; // 0x10
        Time m_tiNextExecute; // 0x30
        NATDirectInterface *m_pDirect; // 0x38
        NATRelayInterface *m_pRelay; // 0x3c
        NATEchoInterface *m_pEcho; // 0x40
        JobGetPublicURL *m_pJob; // 0x44
    };

    NATTraversalEngine::NATTraversalEngine() {
        m_pStream = 0;
        m_pDirect = 0;
        m_uiLocalCID = 0;
        m_pRelay = 0;
        m_pJob = 0;
        m_pEcho = 0;
    }

    NATTraversalEngine::~NATTraversalEngine() {
        while (!m_oProbes.IsEmpty()) {
            m_oProbes.PopBack();
        }
    }

    bool NATTraversalEngine::Initialize() { return true; }

    bool NATTraversalEngine::Terminate() { return true; }

    void NATTraversalEngine::SetLocalCID(unsigned int cid) { m_uiLocalCID = cid; }

    unsigned int NATTraversalEngine::GetLocalCID() const { return m_uiLocalCID; }

    bool NATTraversalEngine::StartStream(RootTransport *pTransport) {
        m_tiNextExecute = Time::GetTime() + s_tiFrequency;
        m_pStream = new (__FILE__, 0x40) NATTraversalStream(this, pTransport);
        return true;
    }

    bool NATTraversalEngine::StopStream() {
        delete m_pStream;
        return true;
    }

    bool NATTraversalEngine::PrepareNATTraversal(const StationURL &url) {
        {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            if (m_pRelay == 0) {
                return false;
            }
        }
        if (url.GetRVConnectionID() != 0) {
            AddURLToProbe(url, true);
            return true;
        } else {
            return false;
        }
    }

    bool NATTraversalEngine::PrepareNATTraversal(const qList<StationURL> &lstURLs) {
        {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            if (m_pRelay == 0) {
                return false;
            }
        }
        qList<StationURL>::const_iterator it = lstURLs.begin();
        while (it != lstURLs.end()) {
            PrepareNATTraversal(*it);
            ++it;
        }
        return true;
    }

    bool NATTraversalEngine::AddURLToProbe(const StationURL &url, bool bRequested) {
        {
            ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
            if (m_pRelay == 0) {
                return false;
            }
        }
        ScopedCS oListCS(m_oProbes.mCSList);
        URLProbe *pProbe = m_oProbes.FindProbe(url);
        if (pProbe == 0) {
            URLProbe oProbe(url, s_tiProbeLifetime, bRequested);
            m_oProbes.PushBack(oProbe);
            return true;
        } else {
            pProbe->Refresh(s_tiProbeLifetime);
            return false;
        }
    }

    void NATTraversalEngine::Execute() {
        qList<StationURL> lstRelayExt;
        qList<StationURL> lstRelay;
        if (Time::GetTime() > m_tiNextExecute) {
            ScopedCS oCS(m_oProbes.mCSList);
            m_tiNextExecute = Time::GetTime() + (int)s_tiFrequency;
            int iType = 0;
            qList<URLProbe>::iterator it = m_oProbes.GetList().begin();
            if (it != m_oProbes.GetList().end()) {
                iType = it->GetURL().GetURLType();
            }
            while (it != m_oProbes.GetList().end()) {
                if (it->IsProbeRequested() && it->GetURL().GetRVConnectionID() != 0) {
                    switch (it->GetURL().GetURLType()) {
                    case 3:
                        lstRelayExt.push_back(it->GetURL());
                        break;
                    case 1:
                        lstRelay.push_back(it->GetURL());
                        break;
                    }
                    it->SetProbeRequested(false);
                    ++it;
                } else if (it->IsExpired(Time::GetTime())) {
                    it = m_oProbes.GetList().erase(it);
                } else {
                    if (it->UpdateIsNeeded(Time::GetTime())) {
                        if (m_uiLocalCID != 0) {
                            SendProbe(ProbeRequest, it->GetURL(), Time::GetTime());
                            it->IncNbProbes();
                        }
                    }
                    ++it;
                }
            }
            if (m_pRelay != 0) {
                if (!lstRelay.empty()) {
                    m_pRelay->RequestProbeInitiation(lstRelay);
                }
                if (m_pDirect != 0 && !lstRelayExt.empty()) {
                    m_pRelay->RequestProbeInitiationExt(lstRelayExt, m_pDirect->m_oURL);
                }
            }
        }
    }

    void NATTraversalEngine::SendProbe(Msg msg, const StationURL &url, Time tiSent) {
        ByteStream oStream;
        oStream.Append((unsigned char)msg);
        oStream.Append((const unsigned char *)&m_uiLocalCID, 4, 1);
        oStream.Append(&tiSent);
        if (url.GetURLType() != 3) {
            m_pStream->SendMsg(url, oStream.GetBuffer());
        } else if (m_pDirect != 0) {
            unsigned char *pPendingData;
            unsigned int uiPendingSize;
            if (m_pDirect->GetPendingData(&pPendingData, &uiPendingSize)) {
                Buffer *pBuffer = new (__FILE__, 0xcc) Buffer(0x400);
                pBuffer->AppendData(pPendingData, uiPendingSize, -1);
                *pBuffer += *oStream.GetBuffer();
                m_pDirect->Send(url, pBuffer->GetContentPtr(), pBuffer->GetContentSize());
                pBuffer->ReleaseRef();
            } else {
                m_pDirect->Send(
                    url, oStream.GetBuffer()->GetContentPtr(), oStream.GetBuffer()->GetContentSize()
                );
            }
        }
    }

    void NATTraversalEngine::SendEcho() {
        if (m_pEcho != 0) {
            StationURL oTargetURL;
            Buffer oRequestBuffer(0x400);
            unsigned char *pPendingData = 0;
            unsigned int uiPendingSize = 0;
            if (m_pDirect->GetPendingData(&pPendingData, &uiPendingSize)) {
                oRequestBuffer.AppendData(pPendingData, uiPendingSize, -1);
            }
            unsigned char ucMsgType = Echo;
            oRequestBuffer.AppendData(&ucMsgType, 1, -1);
            Buffer *pAnswer = new (__FILE__, 0xe7) Buffer(0x400);
            m_pEcho->Process(&oRequestBuffer, pAnswer, &oTargetURL);
            if (oTargetURL.GetURLType() == 3) {
                m_pDirect->Send(oTargetURL, pAnswer->GetContentPtr(), pAnswer->GetContentSize());
            }
            pAnswer->ReleaseRef();
        }
    }

    void NATTraversalEngine::ReceiveMessage(
        const StationURL &url, const unsigned char *pData, unsigned int uiSize
    ) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (uiSize > 1) {
            Buffer oBuffer(0x400);
            oBuffer.AppendData(pData, uiSize, -1);
            ByteStream oStream(&oBuffer);
            unsigned char ucMsg;
            oStream.Extract(&ucMsg, 1, 1);
            switch (ucMsg) {
            case ProbeRequest:
            case ProbeReply: {
                unsigned int uiCID;
                if (oStream.Extract((unsigned char *)&uiCID, 4, 1)) {
                    Time tiSent;
                    if (oStream.Extract(&tiSent)) {
                        ReceiveProbe((Msg)ucMsg, url, uiCID, tiSent);
                    }
                }
                break;
            }
            case Echo: {
                Buffer oPayload(oBuffer.GetContentSize());
                oStream >> oPayload;
                StationURL oURL((const char *)oPayload.GetContentPtr());
                if (m_pJob != 0) {
                    oURL.SetRVConnectionID(m_uiLocalCID);
                    if (m_pDirect != 0) {
                        m_pDirect->SetURL(oURL);
                    }
                    m_pJob->SetPublicURL();
                }
                break;
            }
            }
        }
    }

    bool NATTraversalEngine::GetPublicURL(int iType, StationURL *pURL) {
        if (pURL == 0) {
            return false;
        }
        if (iType == 1) {
            ScopedCS oCS(Network::GetInstance()->GetStationURLs()->mCSList);
            qList<StationURL>::iterator itURL = Network::GetInstance()->GetStationURLs()->GetList().begin();
            while (itURL != Network::GetInstance()->GetStationURLs()->GetList().end()) {
                if (itURL->IsPublic()) {
                    *pURL = *itURL;
                    return true;
                }
                ++itURL;
            }
        }
        if (iType == 3 && m_pDirect != 0) {
            *pURL = m_pDirect->m_oURL;
            return true;
        }
        return false;
    }

    bool NATTraversalEngine::RequestPublicURL(URLRequester *pOwner, int iType) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pJob != 0) {
            return false;
        }
        if (iType != 3) {
            return false;
        }
        if (m_pDirect == 0) {
            return false;
        }
        if (!pOwner->IsReady()) {
            return false;
        }
        m_pJob = new (__FILE__, 0x15b) JobGetPublicURL(pOwner->GetContext(), this);
        Scheduler::GetInstance()->Queue(m_pJob, false);
        return true;
    }

    void NATTraversalEngine::ClearPublicURLJob() {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        m_pJob = 0;
    }

    void NATTraversalEngine::ReceiveProbe(
        Msg msg, const StationURL &url, unsigned int uiCID, Time tiSent
    ) {
        ScopedCS oCS(m_oProbes.mCSList);
        if (uiCID != 0) {
            StationURL oURL(url);
            oURL.SetRVConnectionID(uiCID);
            switch (msg) {
            case ProbeRequest:
                m_oProbes.UpdateProbe(oURL, 0);
                if (m_uiLocalCID != 0) {
                    Buffer oBuffer(0x400);
                    SendProbe(ProbeReply, oURL, tiSent);
                }
                break;
            case ProbeReply:
                m_oProbes.UpdateProbe(oURL, tiSent);
                break;
            }
        }
    }

    bool NATTraversalEngine::GetUpdatedURL(const StationURL &url, StationURL *pURL) {
        ScopedCS oCS(m_oProbes.mCSList);
        URLProbe *pProbe = m_oProbes.FindProbe(url);
        if (pProbe != 0 && pProbe->m_oURL != url) {
            *pURL = pProbe->m_oURL;
            return true;
        }
        return false;
    }

    unsigned int NATTraversalEngine::GetURLPingTime(const StationURL &url) {
        ScopedCS oCS(m_oProbes.mCSList);
        URLProbe *pProbe = m_oProbes.FindProbe(url);
        if (pProbe != 0) {
            return pProbe->m_uiPingTime;
        }
        return -1;
    }

    bool NATTraversalEngine::RegisterDirect(int iType, NATDirectInterface *pDirect) {
        if (iType != 3 || m_pDirect != 0) {
            return false;
        }
        m_pDirect = pDirect;
        return true;
    }

    bool NATTraversalEngine::UnregisterDirect(int iType) {
        if (iType != 3 || m_pDirect == 0) {
            return false;
        }
        m_pDirect = 0;
        return true;
    }

    bool NATTraversalEngine::RegisterRelay(NATRelayInterface *pRelay) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pRelay != 0) {
            return false;
        }
        m_pRelay = pRelay;
        m_pRelay->SetEngine(this);
        StartStream(0);
        return true;
    }

    bool NATTraversalEngine::UnregisterRelay() {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pRelay == 0) {
            return false;
        }
        StopStream();
        m_pRelay->SetEngine(0);
        m_pRelay = 0;
        return true;
    }

    bool NATTraversalEngine::RegisterEcho(NATEchoInterface *pEcho) {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pEcho != 0) {
            return false;
        }
        m_pEcho = pEcho;
        return true;
    }

    bool NATTraversalEngine::UnregisterEcho() {
        ScopedCS oCS(Scheduler::GetInstance()->m_csSystemLock);
        if (m_pEcho == 0) {
            return false;
        }
        m_pEcho = 0;
        return true;
    }

    void NATTraversalEngine::Trace(unsigned int uiFlags) {
        TraceLog::GetInstance()->GetOutput()->EnableFlag(2);
        m_oProbes.Trace(uiFlags);
        TraceLog::GetInstance()->GetOutput()->DisableFlag(2);
    }

}
