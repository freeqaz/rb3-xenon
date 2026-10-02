// Quazal NetZ - .\StationManager.cpp
//
// The retail TU is 0x82AB7EF0..0x82ABAB58: the StationManager system
// component (the per-station connection bookkeeping: the bootstrap station
// URLs, the dead-station list and the queue of connect/disconnect jobs) and
// the container helpers it instantiates. It is built /Od /Ob1 with EH off and
// no RTTI (the vtable at 0x821815C0 has no locator), so the helpers the classes
// below define in the class body are expanded in place and the ones defined
// out of line are called. /Ob1 declines a few of the in-class ones
// (DORefTemplate::IsValid, the DDL list Add/Extract templates); those are
// called out of line and their frames are still reserved in the caller, which
// is where retail's otherwise unexplained stack gaps come from.
//
// MemAllocator here has its constructors and destructor
// (RB3_QUAZAL_MEMALLOCATOR_CTORS + RB3_QUAZAL_MEMALLOCATOR_DTOR): with them
// /Ob1 stops at the out-of-line _Rb_tree_base constructor (0x82B4D060) in the
// map members' construction and at the list(alloc) constructor for a local
// qList, as retail does.
//
// The surrounding NetZ classes are declared here only as far as this TU uses
// them; their members are defined in other TUs. At /Od the local names set the
// stack layout, so some were chosen to reproduce retail's frames.

#include "Core/NetZ.h"
#include "Core/Scheduler.h"
#include "Core/SystemComponent.h"
#include "ObjDup/CallRegister.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/DOCoreTypes.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/DuplicatedObject.h"
#include "ObjDup/SelectionIterator.h"
#include "ObjDup/Station.h"
#include "ObjDup/StationManager.h"
#include "ObjDup/JobChangeConnection.h"
#include "Platform/String.h"
#include "Platform/qStd.h"
#include "Plugins/Message.h"
#include "Plugins/StationURL.h"

namespace Quazal {

    class EndPoint;

    ByteStream &operator>>(ByteStream &, DOHandle &);

    template <class T>
    inline ByteStream &operator<<(ByteStream &bs, const T &t) {
        bs.Append((const unsigned char *)&t, sizeof(T), true);
        return bs;
    }

    // The DDL marshalling of a list: a 32-bit count, then the elements.
    template <class T, class TDDL>
    inline void AddList(Message *pStream, const qList<T> &lst) {
        unsigned int uiCount = lst.size();
        pStream->Append((const unsigned char *)&uiCount, 4, true);
        typename qList<T>::const_iterator it = lst.begin();
        for (; it != lst.end(); it++) {
            TDDL::Add(pStream, *it);
        }
    }

    template <class T, class TDDL>
    inline void ExtractList(Message *pStream, qList<T> &lst) {
        lst.clear();
        unsigned int uiCount;
        pStream->Extract((unsigned char *)&uiCount, 4, true);
        for (unsigned int i = 0; i < uiCount; i++) {
            T t;
            TDDL::Extract(pStream, &t);
            lst.push_back(t);
        }
    }

    StationManager::StationManager()
        : SystemComponent("StationManager") {
        Initialize();
        m_pInitialEndPoint = NULL;
    }

    StationManager::~StationManager() {}

    StationManager *StationManager::GetInstance() {
        return NetZ::GetInstance()->GetStationManager();
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
        *pMsg << (unsigned short)it.Count();
        it.GotoStart();
        while (!it.EndReached()) {
            qList<StationURL> lstURLs;
            it.GetDOPtr()->GetStationURLs(&lstURLs);
            *pMsg << it.GetCurrentHandle();
            AddList<StationURL, _Type_stationurl>(pMsg, lstURLs);
            it.Next(false);
        }
    }

    bool StationManager::ExtractBootstrapStationURLs(Message *pMsg) {
        unsigned short usCount;
        pMsg->Extract((unsigned char *)&usCount, 2, true);
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
        DOHandle hStation = pJob->m_refStation.m_hReferencedDO;
        qMap<DOHandle, ConnectionJobs *>::iterator it = m_mapConnectionJobs.find(hStation);
        ConnectionJobs *pJobs = NULL;
        if (it != m_mapConnectionJobs.end()) {
            pJobs = it->second;
        } else {
            pJobs = new (__FILE__, 0x81) ConnectionJobs();
            m_mapConnectionJobs[hStation] = pJobs;
        }
        if (pJobs->GetPendingJob() != NULL) {
            pJobs->GetPendingJob()->ReleaseRef();
            pJobs->m_pPendingJob = pJob;
        } else if (pJobs->GetCurrentJob() != NULL) {
            pJobs->GetCurrentJob()->m_bCancelRequested = true;
            pJobs->m_pPendingJob = pJob;
        } else {
            pJobs->m_pCurrentJob = pJob;
            Scheduler::GetInstance()->Queue(pJob, false);
        }
    }

    JobConnectStation *StationManager::GetLatestConnectionJob(DOHandle hStation) const {
        JobConnectStation *pJob = (JobConnectStation *)GetLatestJob(hStation);
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
            return pJob->GetTargetConnectionState();
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
            while (it != m_mapConnectionJobs.end()) {
                if (it->second->GetCurrentJob()->GetType() == 1) {
                    DOHandle hStation = it->first;
                    CallRegister::GetInstanceRef().SignalRelevantFetchContextes(
                        hStation, DOCallContext::CallCancelled
                    );
                }
                ++it;
            }
            SelectionIteratorTemplate<Station> itStation(1);
            while (!itStation.EndReached()) {
                if (itStation.GetDOPtr()->FlagIsSet(0x10)) {
                    CallRegister::GetInstanceRef().SignalRelevantFetchContextes(
                        *itStation, DOCallContext::CallCancelled
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
        if (pLatest != NULL && pLatest->GetTargetConnectionState() == 0) {
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
