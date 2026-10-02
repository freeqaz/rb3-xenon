#pragma once
#include "Core/SystemComponent.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORef.h"
#include "Platform/qStd.h"
#include "Plugins/StationURL.h"

namespace Quazal {
    class DOOperation;
    class EndPoint;
    class Message;

    class Station;

    class JobChangeConnection;
    class JobConnectStation;
    class JobDisconnectStation;

    // .\StationManager.cpp (0x82AB7EF0..0x82ABAB58): the per-station
    // connection bookkeeping (bootstrap station URLs, dead stations, and the
    // connect/disconnect jobs queued per station).
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
        DOHandle GetInitialStation() const { return m_hInitialStation; }
        EndPoint *GetInitialEndPoint() const { return m_pInitialEndPoint; }
        void ClearInitialEndPoint();
        void ProcessCompletedJob(JobChangeConnection *);
        void ActivateJob(JobChangeConnection *);
        JobConnectStation *GetLatestConnectionJob(DOHandle) const;
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

}
