#include "network/ObjDup/DuplicatedObject.h"
#include "Core/Scheduler.h"
#include "Core/StateMachine.h"
#include "ObjDup/DOOperation.h"
#include "Platform/CriticalSection.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "ObjDup/DOClass.h"
#include "Platform/Time.h"
#include "ObjDup/WKHandle.h"
#include "ObjDup/Session.h"
#include "ObjDup/SelectionIterator.h"
#include "ObjDup/DOSelections.h"
#include "ObjDup/Station.h"

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
        pDO = CreateMasterImpl(Station::GetStationHandle(1), wk.GetDOClassID(), DOID(wk.GetID()));
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
        if (DOSelections::GetInstance()->IsAvailable()) {
            switch (pOp->GetType()) {
            case 5:
            case 6:
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return false;
            case 0xd:
                if (ChangeMasterStationOperation::DynamicCast(pOp)->m_dohNewMasterStation
                    == Station::GetLocalStation()) {
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
                it->ExecuteOperation(op);
            } else {
                it->RemoveFromCachedDuplicationSet(hStation);
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
        mCurrentState = reinterpret_cast<const StateFuncFactory &>(&DuplicatedObject::ValidState);
    }

    StateMachine::StateFuncFactory DuplicatedObject::InvalidState(const QEvent &e) {
        return reinterpret_cast<StateFuncFactory>(&StateMachine::TopState);
    }

    StateMachine::StateFuncFactory DuplicatedObject::ValidState(const QEvent &e) {
        if ((int)e.GetSignal() == 1) {
            mCurrentState =
                reinterpret_cast<const StateFuncFactory &>(&DuplicatedObject::ValidState);
            return 0;
        } else if (((unsigned int)((e.GetSignal() & 0xFFFF) - 4) >> 31) == 0) {
            static_cast<const Operation &>(e).Trace(1);
            Trace(1);
            static TransitionPath t_;
            StaticStateTransition(
                &t_, reinterpret_cast<const StateFuncFactory &>(&DuplicatedObject::InvalidState)
            );
            return 0;
        } else {
            return reinterpret_cast<StateFuncFactory>(&StateMachine::TopState);
        }
    }

    void DuplicatedObject::SetFlag(unsigned short f) { m_uiFlags = m_uiFlags | f; }
    void DuplicatedObject::ClearFlag(unsigned short f) { m_uiFlags = m_uiFlags & (f ^ 0xFFFF); }

}
