#include "network/ObjDup/DuplicatedObject.h"
#include "Core/Scheduler.h"
#include "Core/StateMachine.h"
#include "ObjDup/DOOperation.h"
#include "Platform/CriticalSection.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/ObjDupProtocol.h"
#include "ObjDup/Station.h"
#include "Core/NetZ.h"
#include "Core/OperationManager.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/StationConnections.h"
#include "ObjDup/DOSelections.h"
#include "ObjDup/Session.h"
#include "Plugins/Message.h"
#include "Protocol/ProtocolCallContext.h"
#include "Platform/Time.h"
#include "ObjDup/WKHandle.h"
#include "ObjDup/Session.h"
#include "ObjDup/SelectionIterator.h"
#include "ObjDup/DOSelections.h"
#include "ObjDup/Station.h"
#include "ObjDup/DOSelections.h"
#include "ObjDup/CallRegister.h"
#include "ObjDup/DOCallContext.h"
#include "ObjDup/BundlingPolicy.h"
#include "ObjDup/SelectionIterator.h"
#include "ObjDup/Session.h"

namespace Quazal {

    CriticalSection DuplicatedObject::s_csRefCount(0x40000000);

    DuplicatedObject::DuplicatedObject()
        : StateMachine(static_cast<StateFunc>(&DuplicatedObject::SetInitialState)),
          m_setDuplicationSet(3), m_setCachedDuplicationSet(0) {
        m_uiRefCount = 0;
        m_uiRelevanceCount = 0;
        m_uiFlags = 0;
        {
            ScopedCS cs(Scheduler::GetInstance()->unk38);
            AcquireMainReference();
            SetFlag(1);
        }
        InitialTransition();
    }

    DuplicatedObject::~DuplicatedObject() {}

    void DuplicatedObject::SetStationSpecialRelevance() {
        m_refMasterStation.SetSoft();
        m_setDuplicationSet.SetFlags(1);
    }

    bool DuplicatedObject::IsAKindOf(unsigned int id) const {
        if (m_dohMyself.IsA(id)) {
            return true;
        }
        return GetDOClass(m_dohMyself.GetDOClassID())->IsAKindOf(id);
    }

    void DuplicatedObject::SetMasterStation(const MasterStationRef &refMaster) {
        m_refMasterStation = refMaster;
    }

    bool DuplicatedObject::UpdateImpl(DataSet *pDataSet, const Time &t) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (!IsADuplicationMaster()) {
            SystemError::SignalError(0, 0, 0xE0030000, 0);
            return false;
        }
        if (!GetDOClass(m_dohMyself.GetDOClassID())->SpecificUpdate(this, pDataSet, t)) {
            SystemError::SignalError(0, 0, 0xE0000016, 0);
            return false;
        }
        return true;
    }

    bool DuplicatedObject::RefreshImpl(DataSet *pDataSet, const Time &t) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsADuplicationMaster()) {
            SystemError::SignalError(0, 0, 0xE0030001, 0);
            return false;
        }
        return GetDOClass(m_dohMyself.GetDOClassID())->SpecificRefresh(this, pDataSet, t);
    }

    bool DuplicatedObject::SpecificExtractADataset(Message *, unsigned char) { return false; }

    bool DuplicatedObject::SpecificRefresh(DataSet *pDataSet, const Time &) {
        if (!pDataSet) {
            return true;
        } else {
            SystemError::SignalError(0, 0, 0xE0000016, 0);
            return false;
        }
    }

    bool DuplicatedObject::SpecificUpdate(DataSet *pDataSet, Time) {
        if (!pDataSet) {
            return true;
        } else {
            SystemError::SignalError(0, 0, 0xE0000016, 0);
            return false;
        }
    }

    bool DuplicatedObject::CallApproveFaultRecovery() {
        if (IsAWellKnownDO()) {
            return true;
        }
        return ApproveFaultRecovery();
    }

    bool DuplicatedObject::CallApproveEmigration(unsigned int ui) {
        if (ui == 0 && IsAWellKnownDO()) {
            return true;
        }
        return ApproveEmigration(ui);
    }

    DOClass *DuplicatedObject::GetDOClass(unsigned int id) { return DOClass::FindDOClass(id); }

    Message *DuplicatedObject::CreateStubMessage(unsigned short *puiSize) {
        return ObjDupProtocol::GetInstance()->CreateActionMessage(&m_dohMyself, puiSize);
    }

    bool DuplicatedObject::SendStubMessage(bool bToDuplicas, Message *pMessage) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (bToDuplicas) {
            if (IsADuplica()) {
                delete pMessage;
                SystemError::SignalError(0, 0, 0xE0030000, 0);
                return false;
            } else {
                SendToAllDuplicas(pMessage, 1);
                delete pMessage;
            }
        } else {
            if (IsADuplicationMaster()) {
                delete pMessage;
                SystemError::SignalError(0, 0, 0xE0030001, 0);
                return false;
            } else {
                DOHandle hMaster(m_refMasterStation.m_hReferencedDO.mValue);
                {
                    DORefTemplate<Station> refMaster(hMaster);
                    refMaster.Get()->Send(pMessage, 1);
                    delete pMessage;
                }
            }
        }
        return true;
    }

    bool DuplicatedObject::RemoveFromStore(DOHandle hStation, bool bDelete, bool bRemoveDuplicas) {
        RemoveFromStoreOperation oOperation(hStation, this, bDelete, bRemoveDuplicas);
        return ExecuteOperation(oOperation);
    }

    bool DuplicatedObject::AddToStoreAsDuplica(DOHandle hMaster, Message *pMessage) {
        AddToStoreOperation oOperation(hMaster, this, false, pMessage);
        return ExecuteOperation(oOperation);
    }

    bool DuplicatedObject::AddToStoreAsMaster() {
        AddToStoreOperation oOperation(Station::GetLocalStation(), this, true, NULL);
        return ExecuteOperation(oOperation);
    }

    bool DuplicatedObject::UndeleteMainRef() {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (!IsDeleted()) {
            return false;
        }
        if (FlagIsSet(0x20)) {
            if (IsADuplicationMaster()) {
                return AddToStoreAsMaster();
            } else {
                return AddToStoreAsDuplica(Station::GetLocalStation(), NULL);
            }
        } else {
            AcquireMainReference();
            SetFlag(1);
            return true;
        }
    }

    bool DuplicatedObject::ChangeMasterStation(
        DOHandle hTarget, DOHandle hNewMaster, const MasterStationRef &refMaster,
        const qList<DOHandle> *plstDuplicas, unsigned int uiContext
    ) {
        if (m_refMasterStation.GetHandle() == refMaster.GetHandle()) {
            if (refMaster.m_lcVersion > m_refMasterStation.m_lcVersion) {
                m_refMasterStation.m_lcVersion = refMaster.m_lcVersion;
            }
            return true;
        }
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsDeleted()) {
            return false;
        }
        ChangeMasterStationOperation oOperation(
            hTarget, this, hNewMaster, refMaster, plstDuplicas,
            (ChangeMasterStationOperation::Context)uiContext
        );
        return ExecuteOperation(oOperation);
    }

    void DuplicatedObject::UpdateDatasets(Message *pMessage, DOHandle hObject, unsigned char ucDataSet) {
        DORefTemplate<DuplicatedObject> refObject(hObject);
        if (!refObject.IsValid()) {
            return;
        }
        if (refObject.Get()->IsADuplicationMaster()) {
            return;
        }
        DOHandle hSource(pMessage->unk24);
        UpdateDataSetOperation oOperation(hSource, refObject.Get(), ucDataSet, pMessage);
        refObject.Get()->ExecuteOperation(oOperation);
    }

    DOOperation *DuplicatedObject::GetCurrentOperation() {
        return NetZ::GetInstance()->GetOperationManager()->GetCurrentOperation();
    }

    OperationManager *DuplicatedObject::GetOperationManager() {
        if (NetZ::GetInstance()) {
            return NetZ::GetInstance()->GetOperationManager();
        } else {
            return NULL;
        }
    }

    bool DuplicatedObject::ExecuteOperation(DOOperation &op) {
        bool bResult;
        bool bValid;
        TestInvariants();
        bValid = true;
        if (!OperationValidator::GetInstance()->Validate(&op)) {
            SystemError::SignalError(0, 0, 0xE000001B, 0);
            op.Trace(0x10u);
            bValid = false;
        }
        if (!ValidOperation(&op)) {
            bValid = false;
        }
        if (!bValid) {
            if (op.GetType() == 6) {
                OperationErrorNotifier::GetInstance()->NotifyError(GetHandle(), 0x80010006);
            }
            return false;
        }
        DOHandle hStation = op.GetImplicitStationConnection();
        if (hStation != DOHandle()) {
            int iState = StationConnections::GetInstance()->GetConnectionState(hStation);
            if (iState == 2) {
                JobConnectStation *pJob =
                    StationConnections::GetInstance()->GetConnectionJob(hStation);
                DOOperation *pClone = op.Clone();
                pJob->QueueOperation(pClone);
                return true;
            }
            if (iState == 1) {
                return false;
            }
        }
        GetOperationManager()->OperationBegins(&op);
        GetOperationManager()->InvokeCallbacks(-0x400, -0x201, &op);
        if (op.CallsBackOnDataSet()) {
            CallOperationOnDatasets(&op, (Operation::_Event)0);
        }
        OperationBegin(&op);
        op.Trace((Operation::_Event)0);
        GetOperationManager()->InvokeCallbacks(-0x1ff, -1, &op);
        bResult = PerformOperation(&op);
        GetOperationManager()->InvokeCallbacks(1, 0x1ff, &op);
        if (op.CallsBackOnDataSet()) {
            CallOperationOnDatasets(&op, (Operation::_Event)1);
        }
        CallOperationEndOnAdapters(&op);
        OperationEnd(&op);
        op.Trace((Operation::_Event)1);
        GetOperationManager()->InvokeCallbacks(0x201, 0x400, &op);
        GetOperationManager()->PopOperation(&op);
        TestInvariants();
        return bResult;
    }

    bool DuplicatedObject::PerformOperation(DOOperation *pOp) {
        bool bResult = true;
        switch (pOp->GetType()) {
        case 5:
            DispatchEvent(*pOp);
            bResult = FaultRecoveryImpl(pOp);
            break;
        case 6:
        case 7:
        case 8:
        case 9:
        case 13:
        case 14:
        case 18:
            DispatchEvent(*pOp);
            break;
        }
        return bResult;
    }

    void DuplicatedObject::ExecChangeMasterStation(const ChangeMasterStationOperation &op) {
        DOHandle hStation(op.m_refStation.m_hReferencedDO);
        DOHandle hNewMaster(op.m_refNewMaster.m_hReferencedDO);
        LogicalClockTmpl<unsigned char> lcVersion(op.m_refNewMaster.m_lcVersion);
        if (m_refMasterStation.m_lcVersion >= lcVersion) {
            if (op.GetContext() == 0) {
                lcVersion = LogicalClockTmpl<unsigned char>(
                    m_refMasterStation.m_lcVersion.m_value + (unsigned char)1
                );
            } else {
                return;
            }
        }
        if (m_refMasterStation.GetHandle() == hNewMaster) {
            if (m_refMasterStation.m_lcVersion != LogicalClockTmpl<unsigned char>(1)) {
                Trace(1);
            }
            m_refMasterStation.m_lcVersion = lcVersion;
            return;
        }
        DOHandle hLocal = Station::GetLocalStation();
        if (hLocal == hStation) {
            if (!IsADuplicationMaster()) {
                Trace(1);
            }
            m_setDuplicationSet.Clear();
            SetMasterStation(MasterStationRef(hNewMaster, lcVersion));
        } else if (hLocal == hNewMaster) {
            bool bReconnect = !m_refMasterStation.GetDO()->IsDeleted();
            if (op.GetContext() == 0) {
                bReconnect = false;
            }
            SetMasterStation(MasterStationRef(hNewMaster, lcVersion));
            if (bReconnect) {
                DORef refOldMaster((DOHandle(hStation)));
                AddToDuplicationSet(refOldMaster.Get<Station>());
            }
            if (op.GetStationList()) {
                qList<DOHandle>::const_iterator it;
                for (it = op.GetStationList()->begin(); it != op.GetStationList()->end(); ++it) {
                    if (*it != Station::GetLocalStation()) {
                        DOHandle hDuplica = *it;
                        int iState =
                            StationConnections::GetInstance()->GetConnectionState(hDuplica);
                        switch (iState) {
                        case 0: {
                            DORef refDuplica((DOHandle(hDuplica)));
                            AddToDuplicationSet(refDuplica.Get<Station>());
                            break;
                        }
                        case 2: {
                            JobConnectStation *pJob =
                                StationConnections::GetInstance()->GetConnectionJob(hDuplica);
                            pJob->QueueOperation(new (".\\DuplicatedObject.cpp", 0x31d)
                                                     ChangeDupSetOperation(
                                                         Station::GetLocalStation(), this,
                                                         hDuplica, true,
                                                         (ChangeDupSetOperation::Context)1
                                                     ));
                            break;
                        }
                        }
                    }
                }
            }
            m_setCachedDuplicationSet.Clear();
        } else {
            SetMasterStation(MasterStationRef(hNewMaster, lcVersion));
        }
        RemoveFromDuplicationSet(hNewMaster);
    }

    void DuplicatedObject::ExecChangeDupSet(const ChangeDupSetOperation &op) {
        (void)op.GetFlags();
        DOHandle hStation(op.m_refStation.m_hReferencedDO);
        if (op.IsARemoval()) {
            RemoveFromDuplicationSet(hStation);
            if (op.GetContext() != 0) {
                ForgetDuplicaOn(hStation);
            }
            return;
        }
        DORef refStation((DOHandle(hStation)));
        Message msgDataSets;
        GetDOClass(m_dohMyself.GetDOClassID())
            ->SpecificAddDSToDiscoveryMessage(this, refStation.Get<Station>(), &msgDataSets);
        Message *pMessage = ObjDupProtocol::GetInstance()->CreateDOProtocolMessage();
        ProtocolCallContext oContext;
        if (op.GetMigrationContext() == 0) {
            ObjDupProtocol::BuildCreateDuplica(
                &oContext, pMessage, GetHandle(), m_refMasterStation.GetHandle(),
                m_refMasterStation.m_lcVersion, msgDataSets.GetBuffer()
            );
        } else {
            qList<DOHandle> lstStations;
            FillDuplicaStationsList(&lstStations);
            ObjDupProtocol::BuildMigrateDuplica(
                &oContext, pMessage, op.GetMigrationContext(), GetHandle(),
                DOHandle(m_refMasterStation.GetReferencedHandle()),
                m_refMasterStation.m_lcVersion, msgDataSets.GetBuffer(), &lstStations
            );
        }
        refStation.Get<Station>()->Send(pMessage, true);
        delete pMessage;
        AddToDuplicationSet(refStation.Get<Station>());
    }

    bool DuplicatedObject::FaultRecoveryImpl(DOOperation *pOp) {
        FaultRecoveryOperation *pFRO;
        if (!pOp || pOp->GetType() != 5) {
            pFRO = NULL;
        } else {
            pFRO = (FaultRecoveryOperation *)pOp;
        }
        DOHandle hNewMaster(pFRO->m_refNewMaster.m_hReferencedDO);
        if (m_refMasterStation.GetHandle() == hNewMaster) {
            if (pFRO->m_refNewMaster.m_lcVersion > m_refMasterStation.m_lcVersion) {
                m_refMasterStation.m_lcVersion = pFRO->m_refNewMaster.m_lcVersion;
            }
            return true;
        }
        if (!ChangeMasterStation(
                DOHandle(), m_refMasterStation.GetHandle(),
                MasterStationRef(hNewMaster, pFRO->m_refNewMaster.m_lcVersion), NULL, 0
            )) {
            {
                OperationScope scope(2);
                pFRO->Trace(0x20u);
            }
            return false;
        }
        if (hNewMaster == Station::GetLocalStation()) {
            if (m_refMasterStation.GetHandle() != hNewMaster) {
                Trace(1);
            }
            if (!CallApproveFaultRecovery()) {
                DeleteMainRef();
            }
        }
        return true;
    }

    bool DuplicatedObject::Refresh() { return RefreshImpl(NULL, Time::GetSessionTime()); }

    void DuplicatedObject::ExecRemoveFromStore(const RemoveFromStoreOperation &op) {
        if (op.IsADuplicaRemoval()) {
            DORef refSession(Session::GetWKHandle());
            if (refSession.IsA<Session>()) {
                if (refSession.Get<Session>()->GetSessionState() != 3) {
                    Message *pMsg = ObjDupProtocol::GetInstance()->CreateDeleteMessage(GetHandle());
                    SendToAllDuplicas(pMsg, 1);
                    delete pMsg;
                }
            }
            m_setDuplicationSet.Clear();
        }
        ClearFlag(1);
        ReleaseReference(false);
        if (m_dohMyself.IsA(_DO_Station::GetStaticClassID())) {
            ((Station *)this)->ReleaseOwnReference();
        }
    }

    void DuplicatedObject::ExecAddToStore(const AddToStoreOperation &op) {
        if (op.GetMessage()) {
            GetDOClass(m_dohMyself.GetDOClassID())
                ->SpecificExtractDSFromDiscoveryMessage(this, op.GetMessage());
        }
        if (op.IsADuplica()) {
            Refresh();
        }
        if (IsDeleted()) {
            AcquireMainReference();
            SetFlag(1);
        } else {
            DOSelections::GetDuplicatedObjects()->AddDO(this);
            InitDO();
        }
        if (GetHandle() == m_refMasterStation.GetHandle()) {
            Station::DynamicCast(this)->AcquireOwnReference();
        }
        if (op.IsADuplica()) {
            OperationErrorNotifier::GetInstance()->NotifyError(GetHandle(), 0x60001);
        }
    }

    void DuplicatedObject::OperationBegin(DOOperation *) {}
    void DuplicatedObject::OperationEnd(DOOperation *) {}
    bool DuplicatedObject::Publish(unsigned int ui) {
        unsigned int uiID = m_dohMyself.GetDOID();
        if (uiID == 0) {
            if (!GetDOClass()->GenerateObjectID(&uiID, ui)) {
                SystemError::SignalError(0, 0, 0xE000000C, 0);
                return false;
            }
        }
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsDeleted()) {
            SystemError::SignalError(0, 0, 0xE000000E, 0);
            return false;
        }
        if (!IsAWellKnownDO() && !Session::IsActive()) {
            SystemError::SignalError(0, 0, 0xE0030015, 0);
            return false;
        }
        if (!FlagIsSet(4)) {
            SystemError::SignalError(0, 0, 0xE0030007, 0);
            return false;
        }
        if (FlagIsSet(0x20)) {
            SystemError::SignalError(0, 0, 0xE0030008, 0);
            return false;
        }
        if (m_dohMyself.GetDOID() == 0) {
            SetDOID(DOID(uiID));
            SetMasterStation(
                MasterStationRef(Station::GetLocalStation(), LogicalClockTmpl<unsigned char>(2))
            );
        }
        return AddToStoreAsMaster();
    }

    void DuplicatedObject::FillDuplicaStationsList(qList<DOHandle> *pList) {
        SelectionIterator it(&m_setDuplicationSet, false);
        while (!it.EndReached()) {
            pList->push_back(*it);
            it.Next(false);
        }
    }

    DuplicatedObject *DuplicatedObject::CreateWellKnown(WKHandle &wk) {
        DuplicatedObject *pDO;
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (wk.IsCreated()) {
            SystemError::SignalError(0, 0, 0xE003000D, 0);
            return 0;
        }
        pDO = CreateMasterImpl(Station::ConvertIDToDOHandle(1), wk.GetDOClassID(), DOID(wk.GetID()));
        wk.m_bCreated = true;
        return pDO;
    }

    DuplicatedObject *DuplicatedObject::Create(unsigned int uiClassID, unsigned int uiValue) {
        if (!Session::IsActive()) {
            return CreateMasterImpl(DOHandle(0), uiClassID, DOID(0));
        } else {
            unsigned int uiID = 0;
            if (!DOClass::FindDOClass(uiClassID)->GenerateObjectID(&uiID, uiValue)) {
                SystemError::SignalError(0, 0, 0xE000000C, 0);
                return 0;
            }
            return CreateMasterImpl(Station::GetLocalStation(), uiClassID, uiID);
        }
    }

    DuplicatedObject *DuplicatedObject::Create(unsigned int uiClassID, DOID oID) {
        return CreateMasterImpl(Station::GetLocalStation(), uiClassID, oID);
    }

    DuplicatedObject *
    DuplicatedObject::CreateMasterImpl(DOHandle hMaster, unsigned int uiClassID, DOID oID) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        DOHandle oHandle(0);
        oHandle.SetDOClassID(uiClassID);
        oHandle.SetDOID(oID);
        if (DOSelections::GetInstance()->Contains(oHandle)) {
            return 0;
        }
        DuplicatedObject *pObject = GetDOClass(uiClassID)->Create();
        pObject->m_dohMyself.SetDOClassID(uiClassID);
        pObject->SetFlag(4);
        CreateMasterOperation op(pObject, hMaster, oID);
        pObject->ExecuteOperation(op);
        return pObject;
    }

    DuplicatedObject *
    DuplicatedObject::CreateDuplica(DOHandle h, const MasterStationRef &refMaster) {
        DuplicatedObject *pDO = GetDOClass(h.GetDOClassID())->Create();
        pDO->SetDOClassID(h.GetDOClassID());
        pDO->SetFlag(4);
        pDO->SetDOID(h.GetDOID());
        pDO->SetMasterStation(refMaster);
        return pDO;
    }

    bool DuplicatedObject::ValidOperation(DOOperation *pOp) {
        if (DOSelections::GetCurrentInstance()->IsAvailable()) {
            switch (pOp->GetType()) {
            case 5:
            case 6:
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return false;
            case 0xd:
                if (Station::IsLocal(
                        ChangeMasterStationOperation::DynamicCast(pOp)->m_refNewMaster.m_hReferencedDO.mValue
                    )) {
                    SystemError::SignalError(0, 0, 0xE000000E, 0);
                    return false;
                }
                break;
            }
        }
        return true;
    }

    float DuplicatedObject::ComputeDistance(DuplicatedObject *) { return -1; }
    void DuplicatedObject::ReleaseReferenceToMaster() { m_refMasterStation.Release(); }

    void DuplicatedObject::AcquireReferenceToMaster() {
        DORef *ref;
        DuplicatedObject *referencedDO;
        ref = &m_refMasterStation;
        if (!ref->m_poReferencedDO) {
            ref->Acquire();
        }
        referencedDO = ref->m_poReferencedDO;
    }

    bool DuplicatedObject::IsDuplicatedOn(DOHandle h) {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsADuplicationMaster()) {
            return m_setDuplicationSet.find(h) != m_setDuplicationSet.end();
        } else {
            return IsInCachedDuplicationSet(h);
        }
    }

    bool DuplicatedObject::IsInCachedDuplicationSet(DOHandle h) const {
        return m_setCachedDuplicationSet.find(DOHandle(h)) != m_setCachedDuplicationSet.end();
    }

    void DuplicatedObject::AddToCachedDuplicationSet(const Station *pStation) {
        m_setCachedDuplicationSet.Add(pStation->GetHandle());
    }

    bool DuplicatedObject::RemoveFromCachedDuplicationSet(DOHandle h) {
        return m_setCachedDuplicationSet.Remove(h);
    }

    bool DuplicatedObject::IsInDuplicationSet(DOHandle h) const {
        return m_setDuplicationSet.find(DOHandle(h)) != m_setDuplicationSet.end();
    }

    void DuplicatedObject::AddToDuplicationSet(DuplicatedObject *pDO) { m_setDuplicationSet.Add(pDO); }

    bool DuplicatedObject::RemoveFromDuplicationSet(DOHandle h) {
        return m_setDuplicationSet.Remove(h);
    }

    void DuplicatedObject::RemoveAllDuplicasOnLeavingStation(DOHandle hStation) {
        SelectionIteratorTemplate<DuplicatedObject> it;
        while (!it.EndReached()) {
            if (it->IsADuplicationMaster() && it->IsASettledMaster()) {
                ChangeDupSetOperation op(
                    hStation, it.operator->(), hStation, false, (ChangeDupSetOperation::Context)0
                );
                it.GetDOPtr()->ExecuteOperation(op);
            } else {
                it.GetDOPtr()->RemoveFromCachedDuplicationSet(hStation);
            }
            it.Next(false);
        }
    }

    bool DuplicatedObject::IsASettledMaster() const {
        return IsADuplicationMaster() && !MigrationInProgress();
    }

    // MSVC X360 makes any StateMachine-derived class use the 8-byte
    // multiple_inheritance pmf representation, so &DuplicatedObject::ValidState
    // is 8 bytes while the StateFuncFactory field mCurrentState is the 4-byte
    // single_inheritance StateMachine::* form. The DuplicatedObject sub-object is
    // at offset 0 of StateMachine (no this-adjust), so word 0 (the raw code
    // address) is the value the retail /Od TU stores as a single word at this+4.
    // A first-word reference-reinterpret of the pmf literal reproduces it without
    // a stack temp. (&StateMachine::TopState is already a 4-byte SI pmf.)
    // mCurrentState assignment note (root cause of the SetInitialState/ValidState
    // near-miss): every StateMachine-derived class is forced by MSVC X360 to the
    // 8-byte multiple_inheritance pmf representation, while the StateFuncFactory
    // field is the 4-byte single_inheritance StateMachine::* form. The retail /Od
    // TU stores just the code-address word (one 4-byte store at this+4) — i.e. it
    // truncated the 8-byte DuplicatedObject pmf to its first word with no stack
    // temp. We cannot reproduce that exact frameless single-word store from the
    // available source: any standard cast either materializes
    // the full 8-byte pmf literal (extra `li 0; stw` of the this-adjust word) or
    // forces a frame. &StateMachine::TopState is a 4-byte SI pmf, so InvalidState
    // and the ValidState `else` branch match exactly.

    void DuplicatedObject::SetInitialState(const QEvent &) {
        mCurrentState = (StateFuncFactory)&DuplicatedObject::ValidState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::InvalidState(const QEvent &e) {
        return reinterpret_cast<StateFuncFactory>(&StateMachine::TopState);
    }

    StateMachine::StateFuncFactory DuplicatedObject::ValidState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 1:
            mCurrentState = (StateFuncFactory)&DuplicatedObject::InitialState;
            return 0;
        }
        if (!e.IsSystemEvent()) {
            const Operation &op = static_cast<const Operation &>(e);
            op.Trace(1);
            Trace(1);
            static TransitionPath t_;
            StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::InvalidState);
            return 0;
        }
        return (StateFuncFactory)&StateMachine::TopState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::InitialState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 0x12: {
            const CreateMasterOperation &op = static_cast<const CreateMasterOperation &>(e);
            if (op.m_oDOID.IsNull()) {
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::UnidentifiedMasterState);
                return 0;
            } else {
                m_dohMyself.SetDOID(DOID(op.GetDOID()));
                SetMasterStation(op.m_refMasterStation);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::UnpublishedMasterState);
                return 0;
            }
            break;
        }
        case 6: {
            const AddToStoreOperation &op = static_cast<const AddToStoreOperation &>(e);
            if (op.IsADuplica()) {
                if (op.GetOrigin() == Station::GetLocalStation()) {
                    ExecAddToStore(op);
                    static TransitionPath t_;
                    StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::OrphanDuplicaState);
                    return 0;
                } else {
                    ExecAddToStore(op);
                    static TransitionPath t_;
                    StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::InStoreDuplicaState);
                    return 0;
                }
            }
            break;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::ValidState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::DuplicationMasterState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            if (m_dohMyself.GetDOID() == 0) {
            } else {
                if (!FlagIsSet(0x20)) {
                    DOSelections::GetDuplicatedObjects()->GetAll().Add(this);
                }
                DOSelections::GetDuplicatedObjects()->GetMasters().Add(this);
            }
            return 0;
            break;
        case 3:
            DOSelections::GetDuplicatedObjects()->GetMasters().Remove(this);
            return 0;
            break;
        case 7: {
            const CallMethodOperation &op = static_cast<const CallMethodOperation &>(e);
            DispatchRMCCall(op);
            return 0;
        }
            break;
        case 14: {
            const ChangeDupSetOperation &op = static_cast<const ChangeDupSetOperation &>(e);
            ExecChangeDupSet(op);
            return 0;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::ValidState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::UnpublishedMasterState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            return 0;
        case 3:
            return 0;
        case 6: {
            const AddToStoreOperation &op = static_cast<const AddToStoreOperation &>(e);
            if (op.IsAMaster()) {
                ExecAddToStore(op);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::InStoreMasterState);
                return 0;
            }
            break;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::DuplicationMasterState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::UnidentifiedMasterState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            return 0;
            break;
        case 3:
            DOSelections::GetDuplicatedObjects()->GetAll().Add(this);
            DOSelections::GetDuplicatedObjects()->GetMasters().Add(this);
            return 0;
        }
        return (StateFuncFactory)&DuplicatedObject::UnpublishedMasterState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::InStoreMasterState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            SetFlag(0x20);
            return 0;
            break;
        case 3:
            return 0;
            break;
        case 9: {
            const RemoveFromStoreOperation &op = static_cast<const RemoveFromStoreOperation &>(e);
            ExecRemoveFromStore(op);
            static TransitionPath t_;
            StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::DeletedMasterState);
            return 0;
            break;
        }
        case 13: {
            const ChangeMasterStationOperation &op =
                static_cast<const ChangeMasterStationOperation &>(e);
            if (op.GetStation() == Station::GetLocalStation()) {
                ExecChangeMasterStation(op);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::ConnectedDuplicaState);
                return 0;
            }
            if (op.GetNewMasterStation() == Station::GetLocalStation()) {
                ExecChangeMasterStation(op);
                return 0;
            }
            ExecChangeMasterStation(op);
            return 0;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::DuplicationMasterState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::DeletedMasterState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            return 0;
            break;
        case 3:
            return 0;
        }
        return (StateFuncFactory)&DuplicatedObject::DuplicationMasterState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::DuplicaState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            DOSelections::GetDuplicatedObjects()->GetDuplicas().Add(this);
            return 0;
            break;
        case 3:
            DOSelections::GetDuplicatedObjects()->GetDuplicas().Remove(this);
            return 0;
            break;
        case 7: {
            const CallMethodOperation &op = static_cast<const CallMethodOperation &>(e);
            DispatchRMCCall(op);
            return 0;
        }
        case 14: {
            const ChangeDupSetOperation &op = static_cast<const ChangeDupSetOperation &>(e);
            ExecChangeDupSet(op);
            return 0;
            break;
        }
        case 8: {
            const UpdateDataSetOperation &op = static_cast<const UpdateDataSetOperation &>(e);
            ExecUpdateDataSet(op);
            return 0;
            break;
        }
        case 5:
            return 0;
        }
        return (StateFuncFactory)&DuplicatedObject::ValidState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::InStoreDuplicaState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            SetFlag(0x20);
            return 0;
            break;
        case 3:
            return 0;
            break;
        case 9: {
            const RemoveFromStoreOperation &op = static_cast<const RemoveFromStoreOperation &>(e);
            ExecRemoveFromStore(op);
            static TransitionPath t_;
            StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::DeletedDuplicaState);
            return 0;
        }
        case 13: {
            const ChangeMasterStationOperation &op =
                static_cast<const ChangeMasterStationOperation &>(e);
            if (op.GetNewMasterStation() == Station::GetLocalStation()) {
                ExecChangeMasterStation(op);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::InStoreMasterState);
                return 0;
            } else if (op.GetContext() == 0) {
                ExecChangeMasterStation(op);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::OrphanDuplicaState);
                return 0;
            } else {
                ExecChangeMasterStation(op);
                return 0;
            }
            break;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::DuplicaState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::OrphanDuplicaState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            SetFlag(0x10);
            ConnectOrphanDuplica();
            return 0;
            break;
        case 3:
            ClearFlag(0x10);
            return 0;
            break;
        case 6: {
            const AddToStoreOperation &op = static_cast<const AddToStoreOperation &>(e);
            if (op.IsADuplica()) {
                ExecAddToStore(op);
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::ConnectedDuplicaState);
                return 0;
            }
            break;
        }
        case 7:
        case 8:
        case 13:
        case 14: {
            const Operation &op = static_cast<const Operation &>(e);
            if (op.GetOrigin() != Station::GetLocalStation()) {
                const_cast<QEvent &>(e).m_bRepeatEvent = true;
                static TransitionPath t_;
                StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::ConnectedDuplicaState);
                return 0;
            }
            break;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::InStoreDuplicaState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::ConnectedDuplicaState(const QEvent &) {
        return (StateFuncFactory)&DuplicatedObject::InStoreDuplicaState;
    }

    StateMachine::StateFuncFactory DuplicatedObject::DeletedDuplicaState(const QEvent &e) {
        switch (e.GetSignal()) {
        case 2:
            return 0;
            break;
        case 3:
            return 0;
            break;
        case 6: {
            const AddToStoreOperation &op = static_cast<const AddToStoreOperation &>(e);
            ExecAddToStore(op);
            static TransitionPath t_;
            StaticStateTransition(&t_, (StateFuncFactory)&DuplicatedObject::OrphanDuplicaState);
            return 0;
        }
        }
        return (StateFuncFactory)&DuplicatedObject::DuplicaState;
    }

    // ---- 0x82A72D58..0x82A74220 ----

    void DuplicatedObject::DispatchRMCCall(const CallMethodOperation &op) {
        GetDOClass(m_dohMyself.GetDOClassID())->DispatchRMCCall(op);
    }

    void DuplicatedObject::ExecUpdateDataSet(const UpdateDataSetOperation &op) {
        if (op.UpdatesAllDataSets()) {
            GetDOClass(m_dohMyself.GetDOClassID())
                ->SpecificExtractDSFromDiscoveryMessage(this, op.GetMessage());
        } else {
            GetDOClass(m_dohMyself.GetDOClassID())
                ->SpecificExtractADataset(this, op.GetMessage(), op.GetDataSetID());
        }
    }

    bool DuplicatedObject::SendConnectOrphanRequest(FetchContext *pContext, DOHandle hDO) {
        return pContext->ConnectOrphan(hDO);
    }

    bool DuplicatedObject::PerformFaultRecovery(
        DOHandle hFaultyStation, LogicalClockTmpl<unsigned char> clock
    ) {
        if (IsDeleted()) {
            return false;
        }
        FaultRecoveryOperation op(this, hFaultyStation, clock);
        return ExecuteOperation(op);
    }

    void DuplicatedObject::SendToAllDuplicas(Message *pMessage, unsigned int ui) {
        SendToSomeDuplicas(&m_setDuplicationSet, pMessage, ui);
    }

    void DuplicatedObject::SendToSomeDuplicas(
        Selection *pSelection, Message *pMessage, unsigned int ui
    ) {
        if (BundlingPolicy::GetInstance()) {
            SelectionIterator it(pSelection, false);
            BundlingPolicy::GetInstance()->SendToSelection(pMessage, &it, this, ui);
        } else {
            qMap<DOHandle, DuplicatedObject *>::const_iterator i = pSelection->begin();
            while (i != pSelection->end()) {
                static_cast<Station *>(i->second)->Send(pMessage, ui);
                ++i;
            }
        }
    }

    bool DuplicatedObject::IsGlobal() const {
        if (IsAWellKnownDO()) {
            return true;
        }
        if (HasGlobalDOProperty() && !HasForcedNonGlobalProperty()) {
            return true;
        }
        return false;
    }

    bool DuplicatedObject::EmigrateTo(MigrationContext *pContext, DOHandle hNewMaster) {
        return pContext->MigrateObject(GetHandle(), hNewMaster);
    }

    bool DuplicatedObject::MigrationInProgress() const {
        return CallRegister::GetInstance()->MigrationInProgress(GetHandle(), DOHandle());
    }

    bool DuplicatedObject::AttemptEmigration(DOHandle hNewMaster) {
        MigrationContext *pContext = new (__FILE__, 0x450) MigrationContext(false);
        pContext->SetFlag(2);
        return EmigrateTo(pContext, hNewMaster);
    }

    void DuplicatedObject::PrepareToLeave() {
        bool bOK = false;
        DORefTemplate<Session> ref(Session::s_hSession);
        if (ref.IsValid() && ref->GetSessionState() != 3) {
            bOK = CallApproveEmigration(0);
        } else {
            bOK = false;
        }
        bool bMigrating = MigrationInProgress();
        if (bOK && !bMigrating) {
            DOHandle hNewLocation = SelectNewLocation(0);
            if (hNewLocation != DOHandle()) {
                bMigrating = AttemptEmigration(hNewLocation);
            }
        }
        if (!bMigrating) {
            DeleteMainRef();
        }
    }

    DOHandle DuplicatedObject::SelectNewLocation(unsigned int) {
        SelectionIteratorTemplate<Station> it(1);
        while (!it.EndReached()) {
            if (it->IsAPeer() && it->IsConnected() && it->GetState() == 3
                && it->GetProcessType() != 4) {
                return it->GetHandle();
            }
            it.Next(false);
        }
        SystemError::SignalError(0, 0, 0xE0030006, 0);
        return DOHandle();
    }

    bool DuplicatedObject::IsADuplica() const {
        if (DOHandle(m_refMasterStation.m_hReferencedDO.mValue) == DOHandle()) {
            return false;
        }
        return !IsADuplicationMaster();
    }

    bool DuplicatedObject::IsADuplicationMaster() const {
        if (DOHandle(m_refMasterStation.m_hReferencedDO.mValue) == DOHandle()) {
            return false;
        }
        if (Station::GetLocalStation() == DOHandle()) {
            return true;
        }
        return DOHandle(m_refMasterStation.m_hReferencedDO.mValue) == Station::GetLocalStation();
    }

    bool DuplicatedObject::IsAWellKnownDO() const { return m_dohMyself.IsAWKHandle(); }

    unsigned int DuplicatedObject::GetMasterID() const {
        return Station::ConvertDOHandleToID(m_refMasterStation.m_hReferencedDO.mValue);
    }

    void DuplicatedObject::ReleaseMainReference() {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        DecreaseRefCount();
    }

    void DuplicatedObject::CompleteDecreaseRefCount() {
        DOSelections::GetInstance()->RemoveFromAllSelections(this);
        SetFlag(8);
        GetDOClass(m_dohMyself.GetDOClassID())->Delete(this);
    }

    void DuplicatedObject::SetFlag(unsigned short f) { m_uiFlags = m_uiFlags | f; }
    void DuplicatedObject::ClearFlag(unsigned short f) { m_uiFlags = m_uiFlags & (f ^ 0xFFFF); }

    bool DuplicatedObject::DeleteMainRef() {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        if (IsADuplica()) {
            SystemError::SignalError(0, 0, 0xE0030000, 0);
            return false;
        }
        return DeleteMainRefImpl();
    }

    bool DuplicatedObject::DeleteDuplicaMainRef() {
        ScopedCS cs(Scheduler::GetInstance()->unk38);
        return DeleteMainRefImpl();
    }

    bool DuplicatedObject::DeleteMainRefImpl() {
        DORef ref(this);
        if (FlagIsSet(0x20)) {
            RemoveFromStore(DOHandle(), true, true);
        } else {
            DORef refSelf(this);
            ClearFlag(1);
            ReleaseReference(false);
            if (m_dohMyself.IsA(Station::GetClassID())) {
                static_cast<Station *>(this)->ReleaseOwnReference();
            }
        }
        return true;
    }

    bool DuplicatedObject::ConnectOrphanDuplica() {
        {
            DORefTemplate<Station> refMaster(m_refMasterStation.m_hReferencedDO.mValue);
            DORefTemplate<Station> refLocal(Station::GetLocalStation());
            if (refMaster.IsValid() && refMaster->GetState() == 4) {
                return false;
            }
            if (refLocal.IsValid() && refLocal->GetState() == 4) {
                return false;
            }
        }
        FetchContext *pContext =
            new (__FILE__, 0x517) FetchContext(m_refMasterStation.m_hReferencedDO.mValue, false);
        pContext->unk40 = Time::FromMilliseconds(30000);
        pContext->SetFlag(2);
        if (!IsAWellKnownDO()) {
            pContext->SetOrphanRecovery();
        }
        if (SendConnectOrphanRequest(pContext, GetHandle())) {
            return true;
        } else {
            return false;
        }
    }

    // ---- end 0x82A72D58..0x82A74220 ----

}
