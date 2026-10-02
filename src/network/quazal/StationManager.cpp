// Quazal NetZ - .\StationManager.cpp
//
// The retail TU is 0x82AB7EF0..0x82ABAB58: the StationManager system
// component (the per-station connection bookkeeping: the bootstrap station
// URLs, the dead-station list and the queue of connect/disconnect jobs) and
// the container helpers it instantiates. It is built /Od /Ob1 with EH off and
// no RTTI, so every helper the classes below define in the class body is
// expanded in place, while the ones defined out of line are called.
//
// The surrounding NetZ classes are declared here only as far as this TU uses
// them; their members are defined in other TUs.

#include "Platform/qStd.h"
#include "Platform/SystemError.h"

namespace Quazal {

    class InstantiationContext : public RootObject {
    public:
        unsigned int GetInstance(unsigned int);

        char m_pad0[0x30];
    };

    class InstanceTable : public RootObject {
    public:
        unsigned int GetInstanceFromVector(unsigned int ui, unsigned int idx) {
            if (idx == 0) {
                return m_oDefaultContext.GetInstance(ui);
            } else if (idx >= m_pvContextVector->size()) {
                SystemError::SignalError(0, 0, 0xe0000003, 0);
                return -1;
            } else {
                return (*m_pvContextVector)[idx]->GetInstance(ui);
            }
        }

        InstantiationContext m_oDefaultContext; // 0x0
        qVector<InstantiationContext *> *m_pvContextVector; // 0x30
    };

    class InstanceControl : public RootObject {
    public:
        static InstanceTable s_oInstanceTable;
    };

    class PseudoSingleton : public InstanceControl {
    public:
        static unsigned int GetCurrentContext();
    };

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent;
    };

    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}
        DOHandle &operator=(const DOHandle &h) {
            mValue = h.mValue;
            return *this;
        }

        unsigned int GetID() const {
            unsigned int uiID = mValue & 0x3FFFFF;
            return uiID;
        }
        bool operator<(const DOHandle &h) const { return mValue < h.mValue; }
        bool operator==(const DOHandle &h) const { return mValue == h.mValue; }
        bool operator!=(const DOHandle &h) const { return mValue != h.mValue; }

        unsigned int mValue; // 0x0
    };

    class DOID : public RootObject {
    public:
        DOID(unsigned int ui = 0) : m_uiValue(ui) {}
        DOID(const DOID &o) : m_uiValue(o.m_uiValue) {}

        unsigned int m_uiValue; // 0x0
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        StationURL(const StationURL &);
        ~StationURL();
        StationURL &operator=(const StationURL &);

        char m_pad[0x64];
    };

    class ByteStream : public RootObject {
    public:
        void Append(const void *, unsigned int, bool);
        void Extract(void *, unsigned int, bool);
        ByteStream &operator>>(DOHandle &);
    };

    class Message : public ByteStream {
    public:
    };

    // The DDL marshalling of a station URL.
    class _Type_stationurl {
    public:
        static void Add(ByteStream *, const StationURL &);
        static bool Extract(ByteStream *, StationURL *);
    };

    // The DDL marshalling of a list: a 32-bit count, then the elements.
    template <class T, class TDDL>
    inline void AddList(ByteStream *pStream, const qList<T> &lst) {
        unsigned int uiCount = lst.size();
        pStream->Append(&uiCount, 4, true);
        typename qList<T>::const_iterator it = lst.begin();
        for (; it != lst.end(); it++) {
            TDDL::Add(pStream, *it);
        }
    }

    template <class T, class TDDL>
    inline void ExtractList(ByteStream *pStream, qList<T> &lst) {
        lst.clear();
        unsigned int uiCount;
        pStream->Extract(&uiCount, 4, true);
        for (unsigned int i = 0; i < uiCount; i++) {
            T t;
            TDDL::Extract(pStream, &t);
            lst.push_back(t);
        }
    }

    class EndPoint;

    class SystemComponent : public RootObject {
    public:
        enum _State {
            Uninitialized = 0x1,
            Initializing = 0x2,
            Ready = 0x4,
            ReadyInUse = 0x8,
            TerminatingInUse = 0x10,
            Terminating = 0x20,
            Terminated = 0x40,
            Faulty = 0x80,
            Unknown = 0x100,
            Invalid = -1
        };

        class Use : public RootObject {
        public:
            Use(SystemComponent *, const char *);
            ~Use();
            bool ComponentExists() const { return mComponentExists; }

            SystemComponent *mComponent; // 0x0
            const char *mName; // 0x4
            bool mComponentExists; // 0x8
        };

        SystemComponent(const String &);
        virtual ~SystemComponent();
        virtual RootObject *AcquireRef();
        virtual void ReleaseRef();
        static const char *type() { return "SystemComponent"; }

        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const { return type() == str; }
        virtual void EnforceDeclareSysComponentMacro() = 0;
        virtual void TraceImpl(unsigned int) const;
        virtual void StateTransition(_State);

        _State Initialize();

        unsigned short m_ui16RefCount; // 0x4
        char m_pad8[0x10];
    };

    class DuplicatedObject : public RootObject {
    public:
        DOHandle GetHandle() const {
            unsigned int uiID = m_dohMyself.GetID();
            if (uiID == 0) {
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return DOHandle(0);
            } else {
                return m_dohMyself;
            }
        }
        bool FlagIsSet(unsigned short f) const { return (m_uiFlags & f) == f; }
        bool IsDeleted() const { return !FlagIsSet(1); }

        char m_pad0[0x20];
        unsigned short m_uiFlags; // 0x20
        char m_pad22[0x26];
        DOHandle m_dohMyself; // 0x48
    };

    class Station : public DuplicatedObject {
    public:
        static DOHandle GetLocalStation();
        bool IsConnected() const;
        bool IsFaulty() const;
        void GetStationURLs(qList<StationURL> *);
        unsigned short GetState() const { return m_usState; }

        char m_pad4c[0x64];
        unsigned short m_usState; // 0xb0
    };

    class DORef : public RootObject {
    public:
        DORef(DOHandle);
        ~DORef();
        DOHandle GetReferencedHandle() const { return m_hReferencedDO; }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        ~DORefTemplate() {}

        bool IsValid() const;
        T *operator->() const {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)m_poReferencedDO;
            }
        }
    };

    class SelectionCursor : public RootObject {
    public:
        SelectionCursor(unsigned int ui = 0) : m_uiValue(ui) {}
        SelectionCursor(const SelectionCursor &);

        unsigned int m_uiValue; // 0x0
    };

    inline bool operator==(SelectionCursor a, unsigned int b) { return a.m_uiValue == b; }

    class SelectionPosition : public RootObject {
    public:
        bool EndReached() const {
            unsigned int uiEnd = 0;
            SelectionCursor oCursor(m_oCursor);
            return oCursor.m_uiValue == uiEnd;
        }

        unsigned int unk0; // 0x0
        SelectionCursor m_oCursor; // 0x4
    };

    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(bool, bool);
        ~SelectionIterator();
        void Next(bool);
        void GotoStart();
        void InitFilter();
        unsigned int Count();
        bool EndReached() const { return m_oPosition.EndReached(); }
        unsigned int GetCurrentHandle() const { return m_oPosition.m_oCursor.m_uiValue; }

        void *m_pSelection; // 0x0
        SelectionPosition m_oPosition; // 0x4
        unsigned char unkC[0x18];
    };

    template <class T>
    class SelectionIteratorTemplate : public SelectionIterator {
    public:
        SelectionIteratorTemplate() : SelectionIterator(1 - 1 == 0, 1 == 0) {
            InitFilter();
            GotoStart();
        }
        SelectionIteratorTemplate(int iMode);
        T *GetDOPtr();
    };

    class Job : public RootObject {
    public:
        virtual ~Job();
        virtual void _v1();
        virtual void Cancel();
        virtual void _v3();
        virtual void _v4();
        virtual void _v5();
        virtual void _v6();
        virtual void _v7();
        virtual void _v8();
        virtual void _v9();
        virtual void _v10();
        virtual int GetState();
        virtual int GetTargetState();
        virtual void Trace(unsigned int);
    };

    class JobChangeConnection : public Job {
    public:
        char m_pad4[0x5C];
        bool m_bCancelRequested; // 0x60
        char m_pad61[3];
        DORef m_refStation; // 0x64
    };

    class JobConnectStation : public JobChangeConnection {
    public:
        JobConnectStation(DOHandle);
        char m_pad70[0x160];
    };

    class JobDisconnectStation : public JobChangeConnection {
    public:
        JobDisconnectStation(Station *);
        char m_pad70[0x10];
    };

    class Scheduler;

    class InstanceControlRef : public RootObject {
    public:
        char m_pad0[0xC];
        void *m_pDelegatorInstance; // 0xc
    };

    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControlRef *inst = (InstanceControlRef *)InstanceControl::s_oInstanceTable
                                           .GetInstanceFromVector(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : 0;
            return pCore;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    class Scheduler {
    public:
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (!inst)
                return 0;
            else
                return inst->GetScheduler();
        }
        void Queue(Job *, bool);
    };

    class DOCallContext {
    public:
        enum _Outcome {
            CallCancelled = 0x80060003
        };
    };

    class CallRegister {
    public:
        static CallRegister *GetInstanceRef();
        void SignalRelevantFetchContextes(DOHandle, DOCallContext::_Outcome);
    };

    class StationManager;

    // The type-4 component: the NetZ core object of the current context.
    class NetZCore {
    public:
        static NetZCore *GetInstance(unsigned int uiContext) {
            return (NetZCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static NetZCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        StationManager *GetStationManager() { return m_pStationManager; }

        char m_pad0[0x20];
        StationManager *m_pStationManager; // 0x20
    };

    class StationManager : public SystemComponent {
    public:

        class ConnectionJobs : public RootObject {
        public:
            ConnectionJobs() : m_pCurrentJob(NULL), m_pPendingJob(NULL) {}
            JobChangeConnection *GetCurrentJob() { return m_pCurrentJob; }
            JobChangeConnection *GetPendingJob() { return m_pPendingJob; }

            JobChangeConnection *m_pCurrentJob; // 0x0
            JobChangeConnection *m_pPendingJob; // 0x4
        };

        typedef qMap<DOHandle, qList<StationURL> > StationURLMap;
        typedef qMap<DOHandle, ConnectionJobs *> ConnectionJobsMap;

        static const char *type() { return "StationManager"; }

        StationManager();
        virtual ~StationManager();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const {
            return type() == str || SystemComponent::IsAKindOf(str);
        }
        virtual void EnforceDeclareSysComponentMacro() {}
        virtual void StateTransition(_State);

        static StationManager *GetInstance();
        void AddStationURLs(DOHandle, const qList<StationURL> &);
        bool RetrieveStationURLs(DOHandle, qList<StationURL> *);
        void AddBootstrapStationURLs(Message *);
        bool ExtractBootstrapStationURLs(Message *);
        void AddDeadStation(DOHandle);
        bool StationIsDead(DOHandle);
        void SetInitialConnectionPoint(DOHandle, EndPoint *);
        void ClearInitialEndPoint();
        void ProcessCompletedJob(JobChangeConnection *);
        void ActivateJob(JobChangeConnection *);
        JobChangeConnection *GetLatestConnectionJob(DOHandle) const;
        JobChangeConnection *GetLatestJob(DOHandle) const;
        int GetTargetConnectionState(DOHandle);
        bool ConnectionIsPossible(DOHandle);
        JobChangeConnection *DisconnectStation(Station *);
        int ConnectStation(DOHandle);
        void TraceState(DOHandle, unsigned int);

        StationURLMap m_mapStationURLs; // 0x18
        ConnectionJobsMap m_mapConnectionJobs; // 0x34
        DOHandle m_hInitialStation; // 0x50
        EndPoint *m_pInitialEndPoint; // 0x54
        qList<DOHandle> m_lstDeadStations; // 0x58
    };

    StationManager::StationManager()
        : SystemComponent("StationManager") {
        Initialize();
        m_pInitialEndPoint = NULL;
    }

    StationManager::~StationManager() {}

    StationManager *StationManager::GetInstance() {
        return NetZCore::GetInstance()->GetStationManager();
    }

    void StationManager::AddStationURLs(DOHandle hStation, const qList<StationURL> &lstURLs) {
        m_mapStationURLs[hStation] = lstURLs;
    }

    bool StationManager::RetrieveStationURLs(DOHandle hStation, qList<StationURL> *pURLs) {
        StationURLMap::iterator it = m_mapStationURLs.find(hStation);
        if (it == m_mapStationURLs.end()) {
            return false;
        }
        *pURLs = it->second;
        return true;
    }

    void StationManager::AddBootstrapStationURLs(Message *pMsg) {
        SelectionIteratorTemplate<Station> it(1);
        unsigned short usCount = it.Count();
        pMsg->Append(&usCount, 2, true);
        it.GotoStart();
        while (!it.EndReached()) {
            qList<StationURL> lstURLs;
            it.GetDOPtr()->GetStationURLs(&lstURLs);
            unsigned int uiHandle = it.GetCurrentHandle();
            pMsg->Append(&uiHandle, 4, true);
            AddList<StationURL, _Type_stationurl>(pMsg, lstURLs);
            it.Next(false);
        }
    }

    bool StationManager::ExtractBootstrapStationURLs(Message *pMsg) {
        unsigned short usCount;
        pMsg->Extract(&usCount, 2, true);
        for (unsigned short i = 0; i < usCount; i++) {
            DOHandle hStation;
            qList<StationURL> lstURLs;
            *pMsg >> hStation;
            ExtractList<StationURL, _Type_stationurl>(pMsg, lstURLs);
            AddStationURLs(hStation, lstURLs);
        }
        return true;
    }

    void StationManager::AddDeadStation(DOHandle hStation) {
        m_lstDeadStations.push_back(hStation);
    }

    bool StationManager::StationIsDead(DOHandle hStation) {
        return std::find(m_lstDeadStations.begin(), m_lstDeadStations.end(), hStation)
            != m_lstDeadStations.end();
    }

    void StationManager::SetInitialConnectionPoint(DOHandle hStation, EndPoint *pEndPoint) {
        m_pInitialEndPoint = pEndPoint;
        m_hInitialStation = hStation;
    }

    void StationManager::ClearInitialEndPoint() { m_pInitialEndPoint = NULL; }

    void StationManager::ProcessCompletedJob(JobChangeConnection *pJob) {
        DOHandle hStation = pJob->m_refStation.m_hReferencedDO;
        qMap<DOHandle, ConnectionJobs *>::iterator it = m_mapConnectionJobs.find(hStation);
        ConnectionJobs *pJobs = it->second;
        JobChangeConnection *pPending;
        if (pJob == pJobs->GetCurrentJob()) {
            pJobs->m_pCurrentJob = NULL;
            if (pJobs->GetPendingJob() != NULL) {
                pPending = pJobs->m_pPendingJob;
                pJobs->m_pPendingJob = NULL;
                pJobs->m_pCurrentJob = pPending;
                Scheduler::GetInstance()->Queue(pPending, false);
            } else {
                delete pJobs;
                m_mapConnectionJobs.erase(it);
            }
        } else {
            pJobs->m_pPendingJob = NULL;
        }
    }

    void StationManager::ActivateJob(JobChangeConnection *pJob) {
        DOHandle hStation = pJob->m_refStation.GetReferencedHandle();
        qMap<DOHandle, ConnectionJobs *>::iterator it = m_mapConnectionJobs.find(hStation);
        ConnectionJobs *pJobs = NULL;
        if (it != m_mapConnectionJobs.end()) {
            pJobs = it->second;
        } else {
            pJobs = new (__FILE__, 0x81) ConnectionJobs();
            m_mapConnectionJobs[hStation] = pJobs;
        }
        if (pJobs->GetPendingJob() != NULL) {
            pJobs->GetPendingJob()->Cancel();
            pJobs->m_pPendingJob = pJob;
        } else if (pJobs->GetCurrentJob() != NULL) {
            pJobs->GetCurrentJob()->m_bCancelRequested = true;
            pJobs->m_pPendingJob = pJob;
        } else {
            pJobs->m_pCurrentJob = pJob;
            Scheduler::GetInstance()->Queue(pJob, false);
        }
    }

    JobChangeConnection *StationManager::GetLatestConnectionJob(DOHandle hStation) const {
        JobChangeConnection *pJob = GetLatestJob(hStation);
        return pJob;
    }

    JobChangeConnection *StationManager::GetLatestJob(DOHandle hStation) const {
        qMap<DOHandle, ConnectionJobs *>::const_iterator it = m_mapConnectionJobs.find(hStation);
        if (it == m_mapConnectionJobs.end()) {
            return NULL;
        } else {
            ConnectionJobs *pJobs = it->second;
            if (pJobs->GetPendingJob() != NULL) {
                return pJobs->m_pPendingJob;
            } else {
                return pJobs->m_pCurrentJob;
            }
        }
    }

    int StationManager::GetTargetConnectionState(DOHandle hStation) {
        JobChangeConnection *pJob = GetLatestJob(hStation);
        if (pJob != NULL) {
            return pJob->GetTargetState();
        } else {
            DORefTemplate<Station> refStation(hStation);
            if (refStation.IsValid()) {
                if (refStation->IsDeleted()) {
                    return 0;
                } else if (refStation->IsConnected()) {
                    return 2;
                } else {
                    return 0;
                }
            } else {
                return 0;
            }
        }
    }

    void StationManager::StateTransition(_State eState) {
        if (eState == Terminating) {
            ConnectionJobsMap::iterator it = m_mapConnectionJobs.begin();
            DOHandle hStation;
            while (it != m_mapConnectionJobs.end()) {
                if (it->second->GetCurrentJob()->GetState() == 1) {
                    hStation = it->first;
                    CallRegister::GetInstanceRef()->SignalRelevantFetchContextes(
                        hStation, DOCallContext::CallCancelled
                    );
                }
                ++it;
            }
            SelectionIteratorTemplate<Station> itStation;
            while (!itStation.EndReached()) {
                if (itStation.GetDOPtr()->FlagIsSet(0x10)) {
                    DOHandle hStation = itStation.m_oPosition.m_oCursor.m_uiValue;
                    CallRegister::GetInstanceRef()->SignalRelevantFetchContextes(
                        hStation, DOCallContext::CallCancelled
                    );
                }
                itStation.Next(false);
            }
        }
    }

    bool StationManager::ConnectionIsPossible(DOHandle hStation) {
        DORefTemplate<Station> refStation(hStation);
        if (refStation.IsValid()) {
            if (refStation->GetState() == 4) {
                return false;
            }
            if (refStation->GetState() == 5) {
                return false;
            }
            if (refStation->IsFaulty()) {
                return false;
            }
        }
        return true;
    }

    JobChangeConnection *StationManager::DisconnectStation(Station *pStation) {
        JobChangeConnection *pLatest = GetLatestJob(pStation->GetHandle());
        if (pLatest != NULL && pLatest->GetTargetState() == 0) {
            return pLatest;
        }
        JobChangeConnection *pJob = new (__FILE__, 0x11F) JobDisconnectStation(pStation);
        ActivateJob(pJob);
        return pJob;
    }

    int StationManager::ConnectStation(DOHandle hStation) {
        SystemComponent::Use oUse(this, NULL);
        if (!oUse.ComponentExists()) {
            return 1;
        }
        if (!ConnectionIsPossible(hStation)) {
            return 1;
        }
        if (hStation == Station::GetLocalStation()) {
            return 0;
        }
        int iState = GetTargetConnectionState(hStation);
        if (iState == 2) {
            JobChangeConnection *pLatest = GetLatestJob(hStation);
            if (pLatest != NULL) {
                return 2;
            } else {
                return 0;
            }
        }
        DORefTemplate<Station> oRef(hStation);
        if (oRef.IsValid()
            && (oRef->GetState() == 4 || oRef->GetState() == 5)) {
            return 1;
        }
        JobChangeConnection *pJob = new (__FILE__, 0x158) JobConnectStation(hStation);
        ActivateJob(pJob);
        return 2;
    }

    void StationManager::TraceState(DOHandle hStation, unsigned int uiFlags) {
        qMap<DOHandle, ConnectionJobs *>::iterator it = m_mapConnectionJobs.find(hStation);
        if (it == m_mapConnectionJobs.end()) {
            return;
        }
        if (it->second->GetCurrentJob() != NULL) {
            it->second->GetCurrentJob()->Trace(uiFlags);
        }
        if (it->second->GetPendingJob() != NULL) {
            it->second->GetPendingJob()->Trace(uiFlags);
        }
    }

}
