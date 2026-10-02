#pragma once
#include "Core/CallContext.h"
#include "Core/PseudoGlobalVariable.h"
#include "Plugins/StationURL.h"
#include "Platform/Time.h"
#include "SessionDDL.h"
#include "ObjDup/WKHandle.h"

namespace Quazal {
    class SessionOperation;
    class JoinSessionOperation;
    class SessionDiscoveryProtocol;
    class SessionDescription;
    class ProductInfo;

    // Retail layout: _DO_Session's 0x600 bytes, then the members below
    // (ctor 0x82A76B58).
    class Session : public _DO_Session {
    public:
        Session();
        virtual ~Session();
        virtual void OperationBegin(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual void Trace(unsigned int) const;

        // Retail source order (0x82A76B58..0x82A799F8).
        static DOHandle GetInstanceHandle();
        static Session *GetInstance();
        char *GetSessionName();
        static unsigned char GetRole();
        static bool CreateSession(const char *, bool);
        static bool SessionMasterSignalsFaultsToAll();
        static void CancelCreation(int);
        bool CompleteCreation();
        static bool IsWaitingInitializedURLsToJoin();
        static bool JoinSessionImpl(CallContext *, const qList<StationURL> &);
        static bool JoinSession(CallContext *, const StationURL &);
        void ReleaseJoinReference();
        void AddStation(DOHandle);
        static void CallApproveJoinSessionCallback(JoinSessionOperation *);
        static void CallOperationBeginCallback(SessionOperation *);
        static void CallOperationEndCallback(SessionOperation *);
        bool SetSystemState(unsigned char);
        unsigned char GetSystemState();
        static void RegisterSessionDiscovery(SessionDiscoveryProtocol *, bool);
        static void UnregisterSessionDiscovery(SessionDiscoveryProtocol *);
        static void RegisterWellKnownDOsFactory(void (*)());
        static void UnregisterWellKnownDOsFactory(void (*)());
        static void InitStaticSessionDescription(ProductInfo *);
        void InitSessionDescription(bool);
        void UpdateSessionDescription();
        static SessionDescription *GetLocalSessionDescription();
        bool SynchronizeTermination(DOHandle);
        void RetrieveURLs(DOHandle, qList<StationURL> *);
        static bool JoinIsAllowed();
        // JobConnectStation's callees (retail 0x82A76D78 / 0x82A92C38).
        bool RetrieveURLs(class DOCallContext *, const DOHandle &, qList<StationURL> *);

        bool UpdateDataSet(DataSet *pDS) { return UpdateImpl(pDS, Time::GetSessionTime()); }

        // DuplicatedObject inlines the well-known handle read (0x82E1050C).
        static DOHandle GetWKHandle() { return s_wkhSession; }
        static unsigned int GetClassID() { return s_uiDOClassID; }

        static WKHandle s_wkhSession;
        static unsigned int s_uiDOClassID;
        static DOHandle s_hSession;

        static void (*s_pfApproveJoinSessionCallback)(JoinSessionOperation *);
        static void (*s_pfOperationBeginCallback)(SessionOperation *);
        static void (*s_pfOperationEndCallback)(SessionOperation *);
        static unsigned int s_uiJoinDelay;
        static bool s_bSessionMasterSignalsFaultsToAll;
        static bool s_bListenOnAnyPort;
        static bool s_bWaitingInitializedURLsToJoin;
        static Time s_tLastJoin;
        static PseudoGlobalVariable<qList<void (*)()> > s_lstWellKnownDOsFactories;

        DORef m_refJoin; // 0x600
        qList<DOHandle> m_lstTerminatingStations; // 0x60c
        DOHandle m_hFaultyMaster; // 0x614
        DOHandle m_hTerminationMaster; // 0x618
    };
}
