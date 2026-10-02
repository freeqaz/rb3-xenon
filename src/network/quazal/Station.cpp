// Quazal NetZ - .\Station.cpp
//
// The retail TU is 0x82A7B860..0x82A7E2E0: the Station methods in source
// order, the inline _DO_Station destructor, the free GetLocalStationHandle(),
// qResult::Equals(const bool &), and the PseudoGlobalVariable<DOHandle>
// instantiation of Station::s_hLocalStation. It is built /Od /Ob1 with EH and
// RTTI off (no function has unwind state, and neither vtable is preceded by a
// complete-object locator).
//
// Classes this TU only calls into are declared here as far as it uses them;
// their members are defined in other TUs.

#include "ObjDup/Station.h"
#include "ObjDup/DOClass.h"
#include "Core/Scheduler.h"
#include "Core/NetZ.h"
#include "Core/SystemComponent.h"
#include "Core/PseudoGlobalVariable.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "Platform/TraceLog.h"
#include "Platform/StringStream.h"
#include "Platform/String.h"
#include "ObjDup/DOOperation.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/SelectionIterator.h"
#include "Plugins/StreamSettings.h"
#include "Core/Job.h"
#include "Platform/qStd.h"

namespace Quazal {

    class EndPoint : public RootObject {
    public:
        virtual ~EndPoint();
        virtual void Unk1();
        virtual void Unk2();
        virtual bool IsFaulty();
        virtual bool IsConnected();
        virtual void Unk5();
        virtual void Unk6();
        virtual void Unk7();
        virtual void Unk8();
        virtual void Unk9();
        virtual void Unk10();
        virtual void Unk11();
        virtual void Unk12();
        virtual void Unk13();
        virtual void Unk14();
        virtual void Unk15();
        virtual void Unk16();
        virtual void Trace(unsigned int);
        virtual void Unk18();
        virtual void Unk19();
        virtual void Unk20();
        virtual void Unk21();
        virtual void Unk22();
        virtual void Unk23();
        virtual void SignalFault(unsigned int, bool);
    };

    class StationURL : public RootObject {
    public:
        StationURL(const char *);
        ~StationURL();
        unsigned char m_pad[0x64];
    };

    class StationManager : public RootObject {
    public:
        static StationManager *GetInstance();
        void TraceState(DOHandle, unsigned int);
        void AddDeadStation(DOHandle);
        void DisconnectStation(Station *);
        void ClearInitialEndPoint();
        void StationIsUp(DOHandle);

        unsigned char m_pad[0x50];
        DOHandle m_hInitialStation; // 0x50
    };

    class JobListenOnWellKnown {
    public:
        static void Activate();
    };

    class CallRegister {
    public:
        static CallRegister *GetInstanceRef();
        void QueueCancelCallToStation(DOHandle);
    };

    class PromotionReferee {
    public:
        static void ProcessLeavingStation(DOHandle);
    };

    class ObjDupProtocol {
    public:
        static ObjDupProtocol *GetInstance();
        bool AddLocalURLs(Station *);
        bool ParseMessage(Message *, bool, String *);
        void QueueMessageFromLocalStation(Message *);
        qResult Send(EndPoint *, Message *, unsigned int);
    };

    class BundlingPolicy {
    public:
        static BundlingPolicy *GetInstance();
        bool IsEnabled() const;
        bool FlagIsSet(unsigned int) const;
    };

    class SystemErrorTrace {
    public:
    };

    class SessionOperation : public DOOperation {
    public:
        void Begin();
        void End();
    };

    class LeaveSessionOperation : public SessionOperation {
    public:
        LeaveSessionOperation(Station *);
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);
    };

    class JoinSessionOperation : public DOOperation {
    public:
        static JoinSessionOperation *DynamicCast(DOOperation *pOp) {
            if (pOp && pOp->GetType() == 14)
                return (JoinSessionOperation *)pOp;
            else
                return 0;
        }
        bool IsJoining() { return m_bIsJoining; }

        unsigned char m_pad20[0xc];
        bool m_bIsJoining; // 0x2c
    };

    class StationConnectionManager {
    public:
        static StationConnectionManager *GetInstance();
        bool RoutingIsEnabled() { return m_bRoutingEnabled; }
        unsigned char m_pad[0xbd];
        bool m_bRoutingEnabled; // 0xbd
    };

    class Router {
    public:
        void EnableRouting(bool);
    };

    class Transport {
    public:
        virtual ~Transport();
        virtual void Unk1();
        virtual void Unk2();
        virtual void Unk3();
        virtual void Unk4();
        virtual void Unk5();
        virtual void Unk6();
        virtual void Unk7();
        virtual void Unk8();
        virtual Router *GetRouter();
    };

    class NetZInstance {
    public:
        unsigned char m_pad[0x4c];
        Transport *m_pTransport; // 0x4c
    };

    class Session : public DuplicatedObject {
    public:
        DOHandle GetMasterStation() const {
            return DOHandle(m_refMasterStation.m_hReferencedDO.mValue);
        }
        static Session *GetInstance();
        static bool IsCreated();
        static unsigned char GetRole();
    };

    class RMCContext : public RootObject {
    public:
        RMCContext(DOHandle, bool);
        ~RMCContext();
        void ClearFlag(unsigned int);
        void SetFlag(unsigned int);
        unsigned char m_pad[0xe0];
    };

    class _DOC_Station {
    public:
        static void CallSignalAsFaulty(Station *, RMCContext *, const unsigned int &);
    };

    class JobProcessFault : public Job {
    public:
        JobProcessFault(DOHandle);
        virtual void Execute();
        bool FaultProcessingShouldStop();
        unsigned char m_pad[0x58 - sizeof(Job)];
    };

    StreamSettings *GetStreamSettingsForContext(int);

    namespace {
        void *GetInstanceType1Delegator();
    }

    inline Transport *GetTransport() {
        NetZInstance *pNetZ = (NetZInstance *)GetInstanceType1Delegator();
        if (pNetZ == 0) {
            return 0;
        } else {
            return pNetZ->m_pTransport;
        }
    }

    PseudoGlobalVariable<DOHandle> s_hLocalStation;

    Station::Station()
        : m_oFaultFlag(), m_uiBundleCount(0) {
        m_pThis = this;
        m_oStationInfo.m_hObserver = DOHandle();
        m_pEndPoint = 0;
        m_bAtEOS = false;
        SetStationSpecialRelevance();
    }

    Station::~Station() {
        if (IsConnected()) {
            Trace(1);
            StationManager::GetInstance()->TraceState(GetHandle(), 1);
        }
        JobListenOnWellKnown::Activate();
        if (GetHandle() != GetLocalStation() && (GetState() == 4 || GetState() == 5)) {
            StationManager::GetInstance()->AddDeadStation(GetHandle());
        }
    }

    void Station::ReleaseOwnReference() { ReleaseReferenceToMaster(); }

    void Station::AcquireOwnReference() { AcquireReferenceToMaster(); }

    void Station::Trace(unsigned int uiFlags) const {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (TraceLog::GetInstance()->IsTraceEnabled(uiFlags)) {
            DuplicatedObject::Trace(uiFlags);
            TraceLog::ScopedIndent indent(2);
            {
                StringStream ss;
                ss << "URLs:";
                for (int i = 0; i < 5; i++) {
                    if (*m_oConnectionInfo.GetURL(i) != '\0') {
                        ss << " " << m_oConnectionInfo.GetURL(i);
                    }
                }
            }
            if (m_pEndPoint) {
                m_pEndPoint->Trace(uiFlags);
            }
        }
    }

    void Station::SetState(_State eState) {
        m_oState.Set(eState);
        if (eState == 3) {
            NetZ::GetInstance()->GetComponent48()->Initialize();
        }
        if (IsADuplica()) {
            return;
        }
        bool bUpdated = UpdateImpl(&m_oState, Time::GetSessionTime());
    }

    void Station::SetAtEOS() { m_bAtEOS = true; }

    void Station::ClearAtEOS() { m_bAtEOS = false; }

    bool Station::AtEOS() { return m_bAtEOS; }

    void Station::LeaveSessionNormally() { SetState((_State)4); }

    void Station::MarkAsDisconnected() {
        SetState((_State)5);
        DeleteMainRef();
    }

    bool Station::IsAPeer() { return !IsLocal(); }

    bool Station::IsLocal() { return GetLocalStation() == GetHandle(); }

    bool Station::DiscoversGlobalObject(DuplicatedObject *pDO) {
        switch (GetState()) {
        case 3:
            return true;
        case 1:
            return pDO->IsAWellKnownDO() || pDO->IsABootstrapDO();
        default:
            return false;
        }
    }

    bool Station::IsConnected() const {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (m_pEndPoint != NULL && m_pEndPoint->IsConnected()) {
            return true;
        } else {
            return false;
        }
    }

    bool Station::IsFaulty() const {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (m_pEndPoint != NULL && m_pEndPoint->IsFaulty()) {
            return true;
        }
        return false;
    }

    EndPoint *Station::ReleaseConnection() {
        FlushBundle(true);
        EndPoint *pEndPoint = m_pEndPoint;
        m_pEndPoint = NULL;
        return pEndPoint;
    }

    void Station::SetConnection(EndPoint *pEndPoint) { m_pEndPoint = pEndPoint; }

    EndPoint *Station::GetEndPoint() { return m_pEndPoint; }

    bool Station::ApproveEmigration(unsigned int) { return false; }

    bool Station::ApproveFaultRecovery() { return false; }

    bool Station::ValidOperation(DOOperation *pOp) {
        if (GetState() == 4 || GetState() == 5) {
            switch (pOp->GetType()) {
            case 6:
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return false;
            case 14:
                if (JoinSessionOperation::DynamicCast(pOp)->IsJoining()) {
                    SystemError::SignalError(0, 0, 0xE000000E, 0);
                    return false;
                }
                break;
            }
        }
        return DuplicatedObject::ValidOperation(pOp);
    }

    void Station::OperationEnd(DOOperation *pOp) {
        switch (pOp->GetType()) {
        case 5:
            if (IsADuplicationMaster()) {
                MarkAsDisconnected();
            }
            break;
        case 13:
            if (((DOOperation *)pOp)->CallsBackOnDataSet() && IsLocal()) {
                InitLocalStationInfo();
            }
            break;
        case 6:
            IsADuplica();
            IsAPeer();
            break;
        case 9: {
            if (FlagIsSet(0x10) && GetState() != 4 && GetState() != 5) {
                SetState((_State)4);
            }
            LeaveSessionOperation *pLeave = NULL;
            if (IsADuplicationMaster()) {
                pLeave = new (__FILE__, 0x158) LeaveSessionOperation(this);
                pLeave->Begin();
            }
            if (IsAPeer()) {
                StationManager::GetInstance()->DisconnectStation(this);
            }
            if (pLeave) {
                pLeave->End();
                delete pLeave;
            }
            break;
        }
        }
    }

    void Station::ReleaseSystemReferences() {
        CallRegister::GetInstanceRef()->QueueCancelCallToStation(GetHandle());
        PromotionReferee::ProcessLeavingStation(GetHandle());
        DuplicatedObject::RemoveAllDuplicasOnLeavingStation(GetHandle());
        if (StationManager::GetInstance()->m_hInitialStation == GetHandle()) {
            StationManager::GetInstance()->ClearInitialEndPoint();
        }
    }

    unsigned int Station::GetInputLatency() { return m_oConnectionInfo.m_uiInputLatency; }

    unsigned int Station::GetOutputLatency() { return m_oConnectionInfo.m_uiOutputLatency; }

    void Station::SetLocalStation(DOHandle hStation) { s_hLocalStation.SetValue(hStation); }

    DOHandle Station::GetLocalStation() { return s_hLocalStation.GetValue(); }

    Station *Station::GetLocalInstance() {
        DORefTemplate<Station> ref(GetLocalStation());
        return ref.Get();
    }

    void Station::InitIdentification(StationIdentification *pIdentification) {
        m_oIdentification = *pIdentification;
    }

    DOHandle Station::ConvertIDToDOHandle(unsigned int uiID) {
        DOHandle hStation;
        hStation.SetDOClassID(_DO_Station::s_uiClassID);
        hStation.SetDOID(DOID(uiID));
        return hStation;
    }

    unsigned int Station::GetStationID() const { return GetHandle().GetDOID(); }

    unsigned int Station::ConvertDOHandleToID(DOHandle hStation) { return hStation.GetDOID(); }

    bool Station::TestAndSetFaultFlag() {
        CallRegister::GetInstanceRef()->QueueCancelCallToStation(GetHandle());
        PromotionReferee::ProcessLeavingStation(GetHandle());
        return m_oFaultFlag.TestAndSet() != 0;
    }

    int ProtectedFlag::TestAndSet() {
        m_csLock.Enter();
        int iOld = m_iValue;
        m_iValue = 1;
        m_csLock.Leave();
        return iOld;
    }

    unsigned int Station::GetProcessType() const { return m_oIdentification.m_uiProcessType; }

    bool Station::Send(Message *pMessage, unsigned int uiFlags) {
        qResult r = SendImpl(pMessage, uiFlags);
        if (TraceLog::GetInstance()->IsTraceEnabled(0x200)) {
            String strMessage;
            ObjDupProtocol::GetInstance()->ParseMessage(pMessage, false, &strMessage);
            if (r.Equals(false)) {
                r.Trace(0x200);
                SystemError::TraceLast(0x200);
            }
        }
        return r;
    }

    qResult Station::SendImpl(Message *pMessage, unsigned int uiFlags) {
        if (IsDeleted()) {
            return qResult(0x80010001);
        }
        if (IsLocal()) {
            return SendLocalMessage(pMessage);
        } else {
            return SendRemoteMessage(pMessage, uiFlags);
        }
        return qResult(0x10001);
    }

    qResult Station::SendLocalMessage(Message *pMessage) {
        ObjDupProtocol::GetInstance()->QueueMessageFromLocalStation(pMessage);
        return qResult(0x10001);
    }

    qResult Station::SendRemoteMessage(Message *pMessage, unsigned int uiFlags) {
        if (!IsConnected()) {
            return qResult(0x80010001);
        }
        if (BundlingPolicy::GetInstance()->IsEnabled()) {
            MessageBundle *pBundle = NULL;
            if (uiFlags & 1) {
                pBundle = &m_oReliableBundle;
            } else if (BundlingPolicy::GetInstance()->FlagIsSet(1)) {
                pBundle = &m_oReliableBundle;
            } else {
                pBundle = &m_oUnreliableBundle;
            }
            if (pBundle->MustFlushBefore(pMessage, uiFlags)) {
                FlushBundle(true);
            }
            if (uiFlags & 8) {
                return ObjDupProtocol::GetInstance()->Send(GetEndPoint(), pMessage, uiFlags);
            } else {
                pBundle->Add(pMessage, uiFlags);
                if (pBundle->MustFlushNow()) {
                    return FlushBundle(true);
                } else {
                    return qResult(0x10001);
                }
            }
        } else {
            return ObjDupProtocol::GetInstance()->Send(GetEndPoint(), pMessage, uiFlags);
        }
    }

    void Station::InitLocalStationInfo() {
        ObjDupProtocol::GetInstance()->AddLocalURLs(this);
        m_oStationInfo.InitMachineUniqueID();
    }

    unsigned int Station::GetMachineUniqueID() const {
        return m_oStationInfo.GetMachineUniqueID();
    }

    const char *Station::GetStationURL(int i) { return m_oConnectionInfo.GetURL(i); }

    void Station::GetStationURLs(qList<StationURL> *pList) {
        pList->clear();
        int i = 0;
        bool bDone = false;
        while (!bDone) {
            const char *szURL = GetStationURL(i);
            if (strlen(szURL) != 0) {
                pList->push_back(StationURL(szURL));
            } else {
                bDone = true;
            }
            i++;
        }
    }

    qResult Station::FlushBundle(bool bForce) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsConnected()) {
            qResult r = m_oUnreliableBundle.Send(GetEndPoint());
            if (r) {
                r = m_oReliableBundle.Send(GetEndPoint());
            }
            if (bForce && GetStreamSettingsForContext(1)->BundlingIsEnabled()) {
                GetStreamSettingsForContext(1)->GetBundling().Flush();
            }
            return r;
        } else {
            return qResult(0x80010001);
        }
    }

    void Station::FlushAllBundles() {
        SelectionIteratorTemplate<Station> it;
        while (!it.EndReached()) {
            it->FlushBundle(false);
            it.Next(false);
        }
        if (GetStreamSettingsForContext(1)->BundlingIsEnabled()) {
            GetStreamSettingsForContext(1)->GetBundling().Flush();
        }
    }

    bool Station::SignalFault(bool b) {
        if (m_pEndPoint == NULL) {
            return false;
        }
        m_pEndPoint->SignalFault(5, b);
        return true;
    }

    void Station::SignalAsFaulty(unsigned int ui) {
        if (m_pEndPoint != NULL) {
            m_pEndPoint->SignalFault(ui, false);
        }
    }

    void Station::InitiateFaultProcessingForStation(DOHandle hStation, unsigned int uiReason) {
        if (StationConnectionManager::GetInstance()->RoutingIsEnabled()) {
            Session *pSession = Session::GetInstance();
            if (pSession && pSession->GetMasterStation() == hStation) {
                GetTransport()->GetRouter()->EnableRouting(false);
            }
        }
        if (Session::IsCreated()) {
            if (Session::GetRole() == 1) {
                DORefTemplate<Station> ref(hStation);
                if (ref.IsValid()) {
                    RMCContext oContext(DOHandle(), true);
                    oContext.ClearFlag(0x20);
                    oContext.SetFlag(4);
                    oContext.SetFlag(0x1000);
                    _DOC_Station::CallSignalAsFaulty(ref.operator->(), &oContext, uiReason);
                }
            }
            if (Session::GetInstance()->GetMasterStation() == hStation) {
                SelectionIteratorTemplate<Station> it;
                while (!it.EndReached()) {
                    if (it->IsAPeer() && it->GetHandle() != hStation) {
                        it->SignalFault(false);
                    }
                    it.Next(false);
                }
            }
        }
        JobProcessFault *pJob = new (__FILE__, 0x32a) JobProcessFault(hStation);
        if (pJob->FaultProcessingShouldStop()) {
            pJob->ReleaseRef();
        } else {
            pJob->SetToWaiting(1500);
            Scheduler::GetInstance()->Queue(pJob, false);
        }
    }

    unsigned int GetLocalStationHandle() { return Station::GetLocalStation().mValue; }
}
