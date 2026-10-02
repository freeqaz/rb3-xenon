// Quazal NetZ - Session.cpp
// Retail .text 0x82A76B58..0x82A7ADD0: the Session functions in source order,
// then the template instantiations only this TU references.
// Built /Od /Oi- /Ob1 /EHs-c- /GR- (no EH state, no RTTI locators on the
// vtables at 0x8217DC00 / 0x8217DC48 / 0x8217DCBC).

#include "ObjDup/Session.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/DOOperation.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/SelectionIterator.h"
#include "Core/Scheduler.h"
#include "Core/SystemComponent.h"
#include "Core/SystemComponents.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "Platform/TraceLog.h"

struct XNKID {
    unsigned char ab[8];
};
struct XNKEY {
    unsigned char ab[16];
};

namespace Quazal {
    class SessionDiscoveryProtocol;

    // The objects this TU reaches through the type-4 instance (NetZ). Their
    // code lives in other TUs; only what Session calls is declared.
    class SessionDescription : public RootObject {
    public:
        void SetProductInfo(unsigned int);
        void SetSessionName(const char *);
        void SetURL(const char *);
        unsigned int GetSessionID() { return m_uiSessionID; }
        void SetSessionID(unsigned int ui) { m_uiSessionID = ui; }

        unsigned char unk0[0x14];
        unsigned int m_uiProductID; // 0x14
        unsigned int m_uiProductVersion; // 0x18
        unsigned int unk1c;
        unsigned int m_uiSessionID; // 0x20
        unsigned char unk24[0x1c];
        void (*m_pfUpdateCallback)(); // 0x40
    };

    class SessionDiscoveryTable : public RootObject {
    public:
        void RegisterProtocol(SessionDiscoveryProtocol *, bool);
        void UnregisterProtocol(SessionDiscoveryProtocol *);
        SessionDescription *GetLocalSessionDescription();
        void Publish();
        void Activate();

        unsigned char unk0[0x84];
        bool m_bIsMaster; // 0x84
        bool m_bActive; // 0x85
    };

    class NetZ {
    public:
        virtual ~NetZ();

        static NetZ *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (NetZ *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        SessionDiscoveryTable *GetSessionDiscoveryTable() { return m_pSessionDiscoveryTable; }
        void StartSessionServices();

        unsigned char unk4[0x28];
        SessionDiscoveryTable *m_pSessionDiscoveryTable; // 0x2c
        unsigned char unk30[0xc];
        void *m_p3c; // 0x3c
        unsigned char unk40[0x8];
        SystemComponent *m_pComponent; // 0x48
    };

    class ProductInfo {
    public:
        unsigned int GetVersion();
        unsigned int GetID();
    };

    class ProductFacade {
    public:
        static ProductInfo *GetProductInfo();
        static unsigned int GetProductType();
    };

    // The registered Xbox session keys (list at 0x82E10420).
    class XboxSessionKeys {
    public:
        static unsigned int GetNbKeys() { return s_lstKeys.size(); }
        static void Create();
        static void RegisterKey(const XNKID *, const XNKEY *);
        static const XNKID *GetKID();
        static const XNKEY *GetKey();

        static qList<XboxSessionKeys *> s_lstKeys;
    };

    class DOClassesTable : public RootObject {
    public:
        DOClassesTable();
        __declspec(noinline) static DOClassesTable *GetInstance() {
            if (s_pInstance == NULL) {
                s_pInstance = new ("../ObjDup/DOClassesTable.h", 0x27) DOClassesTable();
            }
            return s_pInstance;
        }
        void Seal();

        static DOClassesTable *s_pInstance;
        unsigned int unk0;
        unsigned int unk4;
    };

    class StationStateDS {
    public:
        void SetState(unsigned short);

        unsigned short m_usState; // 0x0
    };

    class Station : public RootDO {
    public:
        static DOHandle GetLocalStation();
                static Station *GetLocalInstance();
        static void SetLocalStationHandle(DOHandle);
        static unsigned int GetClassID() { return s_uiDOClassID; }
        static Station *CreateLocalStation(unsigned int);

        void SetProcessType(unsigned int);
        void InitStationInfo(void *);
        void InitURLs();
        void Connect();
        const char *GetStationURL(unsigned int);
        void GetURLs(qList<StationURL> *);
        unsigned short GetState() const { return m_dsState.m_usState; }

        static unsigned int s_uiDOClassID;

        bool m_bLocal; // 0x70
        unsigned char unk71[0x3f];
        StationStateDS m_dsState; // 0xb0
    };

    class PromotionReferee : public RootDO {
    public:
        static unsigned int GetClassID() { return s_uiDOClassID; }
        bool IsPromoting();

        static unsigned int s_uiDOClassID;
        static WKHandle s_wkhPromotionReferee;
    };

    class _DO_PromotionReferee {
    public:
        static PromotionReferee *CreateWellKnown(WKHandle &);
    };

    class _DO_SessionFactory {
    public:
        static Session *CreateWellKnown(WKHandle &);
    };

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        bool ListenOnWellKnown();
        bool StartToListen();
        void StopToListen();
        bool IsListening(unsigned short *) const;
    };

    class BundlingPolicy : public RootObject {
    public:
        virtual ~BundlingPolicy();
        virtual void SendToSelection(void *, void *, void *, unsigned int) = 0;
        virtual void Flush() = 0;
        virtual void AddStation(DOHandle) = 0;

        static BundlingPolicy *GetInstance() { return s_pInstance.GetValue(); }
        static PseudoGlobalVariable<BundlingPolicy *> s_pInstance;
    };

    class JobJoinSession : public RootObject {
    public:
        JobJoinSession(const qList<StationURL> &, unsigned int);
    };

    class SessionOperation;
    class JoinSessionOperation {
    public:
        void Approve();
        int GetOutcome() const { return m_iOutcome; }

        unsigned char unk0[0x20];
        int m_iOutcome; // 0x20
    };

    class XboxNetwork {
    public:
        static bool IsTerminating();
        static bool IsInitialized();
        static void Terminate(int);
    };

    class SystemError2 {
    public:
        static void SetLastError(unsigned int);
    };

    inline bool operator<(const Time &a, const Time &b) { return a.m_ui64Value < b.m_ui64Value; }
    inline bool operator!=(const Time &a, const Time &b) { return a.m_ui64Value != b.m_ui64Value; }

    inline bool IsReferencing(const DORef &r) { return r.GetDOPtr() != NULL; }
    inline DOHandle GetRefHandle(const DORef &r) { return r.m_hReferencedDO; }
    inline const char *GetURLString(const StationURL &url) { return url.GetURL(); }
    inline bool UseIsAllowed(const SystemComponent::Use &u) { return u.mComponentExists; }

    extern int XNetQosLookupKey(const XNKID *, int, int, int, int);

    WKHandle Session::s_wkhSession;
    unsigned int Session::s_uiDOClassID;
    DOHandle Session::s_hSession;
    void (*Session::s_pfApproveJoinSessionCallback)(JoinSessionOperation *);
    void (*Session::s_pfOperationBeginCallback)(SessionOperation *);
    void (*Session::s_pfOperationEndCallback)(SessionOperation *);
    unsigned int Session::s_uiJoinDelay;
    bool Session::s_bSessionMasterSignalsFaultsToAll;
    bool Session::s_bListenOnAnyPort;
    bool Session::s_bWaitingInitializedURLsToJoin;
    Time Session::s_tLastJoin;
    PseudoGlobalVariable<qList<void (*)()> > Session::s_lstWellKnownDOsFactories;

    Session::Session() {
        m_hFaultyMaster = DOHandle();
        m_hTerminationMaster = DOHandle();
    }

    Session::~Session() { ReleaseJoinReference(); }

    DOHandle Session::GetInstanceHandle() { return s_wkhSession; }

    Session *Session::GetInstance() {
        DORefTemplate<Session> ref(GetWKHandle());
        return ref.Get();
    }

    char *Session::GetSessionName() { return m_dsSessionInfo.GetSessionName(); }

    unsigned char Session::GetRole() {
        DORefTemplate<Session> ref(GetWKHandle());
        if (!ref.IsValid()) {
            return 0;
        }
        if (ref->IsADuplicationMaster()) {
            return 1;
        }
        return 2;
    }

    void SessionDescriptionUpdateCallback() {
        DORefTemplate<Session> ref(Session::GetWKHandle());
        if (ref.IsValid() && ref->IsADuplicationMaster()) {
            ref->UpdateSessionDescription();
        }
    }

    bool InvolvesLocalStation(const ChangeMasterStationOperation *pOp) {
        return pOp->GetNewMasterStation() == Station::GetLocalStation()
            || pOp->GetStation() == Station::GetLocalStation();
    }

    void Session::OperationBegin(DOOperation *pOp) {
        switch (pOp->GetType()) {
        case 6:
            if (((AddToStoreOperation *)pOp)->IsAMaster()) {
                m_dsSharedSessionDescription.Refresh();
            }
            break;
        case 0xd:
            if (InvolvesLocalStation(ChangeMasterStationOperation::DynamicCast(pOp))
                && !IsADuplicationMaster()) {
                DORefTemplate<Station> refMaster(m_refMasterStation.GetHandle());
                if (refMaster.IsValid()) {
                    if (refMaster->GetState() == 4 || refMaster->GetState() == 5) {
                        m_hFaultyMaster = refMaster.m_hReferencedDO;
                    }
                }
            }
            break;
        }
    }

    void Session::OperationEnd(DOOperation *pOp) {
        switch (pOp->GetType()) {
        case 6:
            if (IsADuplicationMaster()) {
                NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bIsMaster = true;
                NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bActive = true;
            } else {
                NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bIsMaster = false;
                NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bActive = true;
                m_dsSharedSessionDescription.Clear();
            }
            InitSessionDescription(true);
            GetLocalSessionDescription()->m_pfUpdateCallback = SessionDescriptionUpdateCallback;
            break;
        case 0xd:
            InitSessionDescription(true);
            if (InvolvesLocalStation(ChangeMasterStationOperation::DynamicCast(pOp))) {
                if (IsADuplicationMaster()) {
                    NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bIsMaster = true;
                    NetZ::GetInstance()->GetSessionDiscoveryTable()->Publish();
                    s_tLastJoin = Time::GetTime();
                } else {
                    NetZ::GetInstance()->GetSessionDiscoveryTable()->m_bIsMaster = false;
                }
            }
            break;
        }
    }

    bool Session::CreateSession(const char *szName, bool bListen) {
        int iStep = 0;
        if (ObjDupProtocol::GetInstance() == NULL) {
            return false;
        }
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (XboxSessionKeys::GetNbKeys() == 0) {
            XboxSessionKeys::Create();
        }
        if (bListen) {
            bool bListening = false;
            if (s_bListenOnAnyPort) {
                bListening = ObjDupProtocol::GetInstance()->StartToListen();
            } else {
                bListening = ObjDupProtocol::GetInstance()->ListenOnWellKnown();
            }
            if (!bListening) {
                SystemError2::SetLastError(4);
                CancelCreation(iStep);
                return false;
            }
        }
        iStep++;
        DOHandle hLocalStation;
        hLocalStation.SetDOClassID(Station::GetClassID());
        hLocalStation.SetDOID(DOID(1));
        Station::SetLocalStationHandle(hLocalStation);
        iStep++;
        Session *pSession = _DO_SessionFactory::CreateWellKnown(s_wkhSession);
        if (pSession == NULL) {
            SystemError2::SetLastError(4);
            CancelCreation(iStep);
            return false;
        }
        iStep++;
        pSession->m_dsSessionInfo.SetSessionName(szName);
        pSession->m_dsSessionInfo.GenerateSessionID();
        if (!pSession->Publish(-1)) {
            SystemError2::SetLastError(4);
            CancelCreation(iStep);
            return false;
        }
        iStep++;
        PromotionReferee *pReferee =
            _DO_PromotionReferee::CreateWellKnown(PromotionReferee::s_wkhPromotionReferee);
        if (!pReferee->Publish(-1)) {
            SystemError2::SetLastError(4);
            CancelCreation(iStep);
            return false;
        }
        iStep++;
        for (qList<void (*)()>::iterator it = s_lstWellKnownDOsFactories.GetValue().begin();
             it != s_lstWellKnownDOsFactories.GetValue().end();
             it++) {
            (*it)();
        }
        iStep++;
        if (!XboxNetwork::IsInitialized()) {
            XboxNetwork::Terminate(1);
            SystemError::SignalError(0, 0, 0xE003000A, 0);
            CancelCreation(iStep);
            return false;
        }
        iStep++;
        if (!pSession->CompleteCreation()) {
            CancelCreation(iStep);
            return false;
        }
        iStep++;
        DOClassesTable::GetInstance()->Seal();
        Station::GetLocalInstance()->SetProcessType(3);
        Station::GetLocalInstance()->m_bLocal = true;
        NetZ::GetInstance()->StartSessionServices();
        pSession->InitSessionDescription(bListen);
        NetZ::GetInstance()->GetSessionDiscoveryTable()->Activate();
        pSession->UpdateSessionDescription();
        ((SystemComponent *)(Core::GetInstance() == NULL
                                 ? NULL
                                 : Core::GetInstance()->m_pSystemComponents)
             ->unk24)
            ->Initialize();
        return true;
    }

    bool Session::SessionMasterSignalsFaultsToAll() { return s_bSessionMasterSignalsFaultsToAll; }

    void Session::CancelCreation(int iStep) {
        switch (iStep) {
        case 8:
        case 7:
        case 6:
        case 5:
        case 4:
        case 3: {
            Session *pSession = GetInstance();
            if (pSession) {
                pSession->SetSystemState(3);
            }
        }
        case 2:
        case 1:
        case 0:
            ObjDupProtocol::GetInstance()->StopToListen();
        }
    }

    bool Session::CompleteCreation() {
        if (BundlingPolicy::s_pInstance.GetValue(PseudoSingleton::GetCurrentContext())) {
            BundlingPolicy::s_pInstance.GetValue(PseudoSingleton::GetCurrentContext())->Flush();
        }
        Station *pStation = Station::CreateLocalStation(0);
        pStation->InitStationInfo(NetZ::GetInstance()->m_p3c);
        DOHandle hStation = pStation->GetHandle();
        pStation->m_dsState.SetState(2);
        pStation->InitURLs();
        pStation->Trace(4);
        pStation->Publish(-1);
        if (Station::GetLocalStation() != DOHandle()) {
        } else {
            Station::SetLocalStationHandle(hStation);
        }
        pStation->Connect();
        AddStation(hStation);
        SelectionIteratorTemplate<RootDO> it(0);
        while (!it.EndReached()) {
            if (it->m_refMasterStation.GetHandle() == pStation->GetHandle()) {
                it->AcquireReferenceToMaster();
            }
            it.Next(false);
        }
        return true;
    }

    bool Session::IsWaitingInitializedURLsToJoin() { return s_bWaitingInitializedURLsToJoin; }

    bool Session::JoinSessionImpl(CallContext *pContext, const qList<StationURL> &lstURLs) {
        if (lstURLs.empty()) {
            SystemError::SignalError(0, 0, 0xE0030012, 0);
            return false;
        }
        if (pContext == NULL) {
            CallContext oContext;
            if (!JoinSessionImpl(&oContext, lstURLs)) {
                return false;
            }
            oContext.Wait(120000);
            if (oContext.GetState() != CallContext::CallSuccess) {
                int iCode = oContext.unk20.m_iReturnCode;
                if (iCode == 0x8006000B) {
                    SystemError::SignalError(0, 0, 0xE0030012, 0);
                } else if (iCode == 0x8006000C) {
                    SystemError::SignalError(0, 0, 0xE0030011, 0);
                } else if (iCode == 0x8006000D) {
                    SystemError::SignalError(0, 0, 0xE0030011, 0);
                }
                return false;
            } else {
                return true;
            }
        }
        unsigned int uiRVCID = 0;
        for (qList<StationURL>::const_iterator it = lstURLs.begin(); it != lstURLs.end(); it++) {
            if ((*it).GetRVConnectionID() != 0) {
                if (uiRVCID != 0 && (*it).GetRVConnectionID() != uiRVCID) {
                    SystemError::SignalError(0, 0, 0xE0000016, 0);
                    return false;
                }
                uiRVCID = (*it).GetRVConnectionID();
            }
        }
        if (XboxSessionKeys::GetNbKeys() == 0) {
            for (qList<StationURL>::const_iterator it = lstURLs.begin(); it != lstURLs.end();
                 it++) {
                XNKID kid;
                XNKEY key;
                if ((*it).GetXNKid(&kid)) {
                    (*it).GetXNKey(&key);
                    XboxSessionKeys::RegisterKey(&kid, &key);
                    int iRes = XNetQosLookupKey(&kid, 0, 0, 0, 1);
                }
            }
        }
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (!pContext->FlagsAreValid()) {
            return false;
        }
        if (XboxNetwork::IsTerminating()) {
            SystemError::SignalError(0, 0, 0xE003000B, 0);
            pContext->SetStateImpl(CallContext::CallError, qResult(0x80010001), false);
            return false;
        }
        if (!ObjDupProtocol::GetInstance()->StartToListen()) {
            pContext->SetStateImpl(CallContext::CallError, qResult(0x80010001), false);
            return false;
        }
        JobJoinSession *pJob = new (__FILE__, 0x25F) JobJoinSession(lstURLs, pContext->unk30);
        Scheduler::GetInstance()->Queue((Job *)pJob, false);
        return true;
    }

    bool Session::JoinSession(CallContext *pContext, const StationURL &url) {
        qList<StationURL> lstURLs;
        lstURLs.push_back(url);
        return JoinSessionImpl(pContext, lstURLs);
    }

    void Session::ReleaseJoinReference() {
        if (IsReferencing(m_refJoin)) {
            m_refJoin.Release();
        }
    }

    void Session::AddStation(DOHandle hStation) {
        if (BundlingPolicy::GetInstance()) {
            BundlingPolicy::GetInstance()->AddStation(hStation);
        }
    }

    void Session::CallApproveJoinSessionCallback(JoinSessionOperation *pOp) {
        if (s_pfApproveJoinSessionCallback) {
            s_pfApproveJoinSessionCallback(pOp);
        }
        if (pOp->GetOutcome() == 0) {
            pOp->Approve();
        }
    }

    void Session::CallOperationBeginCallback(SessionOperation *pOp) {
        if (s_pfOperationBeginCallback) {
            s_pfOperationBeginCallback(pOp);
        }
    }

    void Session::CallOperationEndCallback(SessionOperation *pOp) {
        if (s_pfOperationEndCallback) {
            s_pfOperationEndCallback(pOp);
        }
    }

    bool Session::SetSystemState(unsigned char ucState) {
        m_dsSessionState.SetState(ucState);
        return UpdateImpl(&m_dsSessionState, Time::GetSessionTime());
    }

    unsigned char Session::GetSystemState() { return m_dsSessionState.GetState(); }

    void Session::RegisterSessionDiscovery(SessionDiscoveryProtocol *pProtocol, bool b) {
        NetZ::GetInstance()->GetSessionDiscoveryTable()->RegisterProtocol(pProtocol, b);
    }

    void Session::UnregisterSessionDiscovery(SessionDiscoveryProtocol *pProtocol) {
        NetZ::GetInstance()->GetSessionDiscoveryTable()->UnregisterProtocol(pProtocol);
    }

    void Session::RegisterWellKnownDOsFactory(void (*pfFactory)()) {
        s_lstWellKnownDOsFactories.GetValue().push_back(pfFactory);
    }

    void Session::UnregisterWellKnownDOsFactory(void (*pfFactory)()) {
        qList<void (*)()>::iterator it = s_lstWellKnownDOsFactories.GetValue().begin();
        while (it != s_lstWellKnownDOsFactories.GetValue().end()) {
            if (*it == pfFactory) {
                it = s_lstWellKnownDOsFactories.GetValue().erase(it);
            } else {
                ++it;
            }
        }
    }

    void Session::InitStaticSessionDescription(ProductInfo *pInfo) {
        GetLocalSessionDescription()->SetProductInfo(ProductFacade::GetProductType());
        if (pInfo) {
            unsigned int uiProductVersion = pInfo->GetVersion();
            unsigned int uiProductID = pInfo->GetID();
            SessionDescription *pDesc = GetLocalSessionDescription();
            pDesc->m_uiProductID = uiProductID;
            pDesc->m_uiProductVersion = uiProductVersion;
        } else {
            SessionDescription *pDesc = GetLocalSessionDescription();
            pDesc->m_uiProductID = 0;
            pDesc->m_uiProductVersion = 0;
        }
    }

    void Session::InitSessionDescription(bool bWithURL) {
        InitStaticSessionDescription(ProductFacade::GetProductInfo());
        GetLocalSessionDescription()->SetSessionName(GetSessionName());
        GetLocalSessionDescription()->SetSessionID(m_dsSessionInfo.GetSessionID());
        DORefTemplate<Station> refMaster(m_refMasterStation.GetHandle());
        if (refMaster.IsValid()) {
            refMaster->Trace(0x2000);
            StationURL url(refMaster->GetStationURL(0));
            if (bWithURL) {
                bool bValid = true;
                if (XboxSessionKeys::GetNbKeys() == 0) {
                    bValid = false;
                } else {
                    url.SetXNKid(XboxSessionKeys::GetKID());
                    url.SetXNKey(XboxSessionKeys::GetKey());
                }
                if (bValid) {
                    if (GetRefHandle(refMaster) == Station::GetLocalStation()) {
                        unsigned short usPort;
                        if (ObjDupProtocol::GetInstance()->IsListening(&usPort)) {
                            url.SetPortNumber(usPort);
                        }
                    }
                    if (url.IsValid()) {
                        GetLocalSessionDescription()->SetURL(GetURLString(url));
                    }
                }
            }
        }
    }

    void Session::UpdateSessionDescription() {
        m_dsSessionInfo.SetSessionID(GetLocalSessionDescription()->GetSessionID());
        m_dsSharedSessionDescription.Refresh();
        UpdateDataSet(&m_dsSharedSessionDescription);
    }

    SessionDescription *Session::GetLocalSessionDescription() {
        return NetZ::GetInstance()->GetSessionDiscoveryTable()->GetLocalSessionDescription();
    }

    bool Session::SynchronizeTermination(DOHandle hStation) {
        if (!IsASettledMaster()) {
            return false;
        }
        if (m_hFaultyMaster != DOHandle()) {
            DORefTemplate<Station> ref(m_hFaultyMaster);
            if (ref.IsValid()) {
                return false;
            }
            m_hFaultyMaster = DOHandle();
        }
        if (m_hTerminationMaster != DOHandle()) {
            DORefTemplate<Station> ref(m_hTerminationMaster);
            if (ref.IsValid()) {
                return false;
            }
            m_hTerminationMaster = DOHandle();
        }
        {
            DORefTemplate<PromotionReferee> refReferee(PromotionReferee::s_wkhPromotionReferee);
            if (!refReferee.IsValid()) {
                return true;
            }
            if (!refReferee->IsASettledMaster()) {
                refReferee->Trace(1);
                Trace(1);
            }
            if (refReferee->m_refMasterStation.GetHandle() == hStation
                && refReferee->IsPromoting()) {
                return false;
            }
        }
        if (m_refMasterStation.GetHandle() == hStation) {
            qList<DOHandle>::iterator it = m_lstTerminatingStations.begin();
            while (it != m_lstTerminatingStations.end()) {
                DORefTemplate<Station> ref(*it);
                if (!ref.IsValid()) {
                    it = m_lstTerminatingStations.erase(it);
                } else {
                    ref->Trace(0x1000);
                    return false;
                }
            }
            m_hTerminationMaster = hStation;
            return true;
        } else {
            m_lstTerminatingStations.push_back(hStation);
            return true;
        }
    }

    void Session::RetrieveURLs(DOHandle hStation, qList<StationURL> *pURLs) {
        DORefTemplate<Station> ref(hStation);
        if (ref.IsValid()) {
            ref->GetURLs(pURLs);
        }
    }

    bool Session::JoinIsAllowed() {
        SystemComponent::Use oUse(NetZ::GetInstance()->m_pComponent, NULL);
        if (!UseIsAllowed(oUse)) {
            return false;
        }
        if (GetInstance()->GetRole() != 1) {
            return false;
        }
        bool bTooSoon = s_tLastJoin != Time(0) && Time::GetTime() < s_tLastJoin + (int)s_uiJoinDelay;
        if (bTooSoon) {
            return false;
        }
        return true;
    }

    void Session::Trace(unsigned int uiFlags) const {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (TraceLog::GetInstance()->IsTraceEnabled(uiFlags)) {
            DuplicatedObject::Trace(uiFlags);
            TraceLog::ScopedIndent oIndent(2);
        }
    }

    bool _DO_Session::HasGlobalDOProperty() const { return true; }
    bool _DO_Session::IsABootstrapDO() const { return true; }
    bool _DO_Session::IsACoreDO() const { return true; }
}
