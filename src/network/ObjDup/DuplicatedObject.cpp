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
#include "Platform/Message.h"
#include "Platform/Time.h"

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
        return ObjDupProtocol::GetInstance()->CreateStubMessage(m_dohMyself, puiSize);
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
        DOHandle hSource(pMessage->m_hSource.mValue);
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

    void DuplicatedObject::OperationBegin(DOOperation *) {}
    void DuplicatedObject::OperationEnd(DOOperation *) {}
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

    bool DuplicatedObject::IsInDuplicationSet(DOHandle h) const {
        unsigned int val = h.mValue;
        return m_setDuplicationSet.m_map.find(DOHandle(val)) != m_setDuplicationSet.m_map.end();
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
