#pragma once
#include "Core/Operation.h"
#include "Core/StateMachine.h"
#include "DOHandle.h"
#include "ObjDup/MasterStationRef.h"
#include "ObjDup/DOOperation.h"
#include "ObjDup/DORefTemplate.h"
#include "Platform/CriticalSection.h"
#include "Platform/ScopedCS.h"
#include "Selection.h"
#include "ObjDup/DOID.h"
#include "Platform/SystemError.h"

namespace Quazal {
    class DataSet;
    class Time;
    class Message;
    class DOClass;
    class Station;
    class WKHandle;
    class OperationManager;
    class RemoveFromStoreOperation;
    class AddToStoreOperation;
    class ChangeMasterStationOperation;
    class ChangeDupSetOperation;
    class CallMethodOperation;
    class UpdateDataSetOperation;
    class FetchContext;
    class MigrationContext;
    template <class T>
    class qList;
    template <class T>
    class LogicalClockTmpl;

    class DuplicatedObject : public StateMachine {
    public:
        DuplicatedObject();
        virtual ~DuplicatedObject();
        virtual bool HasGlobalDOProperty() const;
        virtual bool HasForcedNonGlobalProperty() const;
        virtual bool ApproveFaultRecovery();
        virtual bool ApproveEmigration(unsigned int);
        virtual void InitDO();
        virtual float ComputeDistance(DuplicatedObject *);
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool ValidOperation(DOOperation *);
        virtual void OperationBegin(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual float GetWeight() { return 0; }
        virtual void Trace(unsigned int) const;
        virtual void CallOperationEndOnAdapters(DOOperation *);
        virtual void TestInvariants();
        virtual bool IsACoreDO() const = 0;
        virtual bool IsABootstrapDO() const { return false; }
        virtual void UpdateCellStats(int, int, int) {}

        // Retail source order (0x82A6FC78..0x82A76B58).
        bool IsAKindOf(unsigned int) const;
        void SetMasterStation(const MasterStationRef &);
        bool UpdateImpl(DataSet *, const Time &);
        bool RefreshImpl(DataSet *, const Time &);
        bool SpecificExtractADataset(Message *, unsigned char);
        bool SpecificRefresh(DataSet *, const Time &);
        bool SpecificUpdate(DataSet *, Time);
        bool CallApproveFaultRecovery();
        bool CallApproveEmigration(unsigned int);
        static DOClass *GetDOClass(unsigned int);
        Message *CreateStubMessage(unsigned short *);
        bool SendStubMessage(bool, Message *);
        bool RemoveFromStore(DOHandle, bool, bool);
        bool AddToStoreAsDuplica(DOHandle, Message *);
        bool AddToStoreAsMaster();
        bool UndeleteMainRef();
        bool ChangeMasterStation(
            DOHandle, DOHandle, const MasterStationRef &, const qList<DOHandle> *, unsigned int
        );
        static void UpdateDatasets(Message *, DOHandle, unsigned char);
        static DOOperation *GetCurrentOperation();
        static OperationManager *GetOperationManager();
        bool ExecuteOperation(DOOperation &);
        bool PerformOperation(DOOperation *);
        void ExecRemoveFromStore(const RemoveFromStoreOperation &);
        void ExecAddToStore(const AddToStoreOperation &);
        void ExecChangeMasterStation(const ChangeMasterStationOperation &);
        void ExecChangeDupSet(const ChangeDupSetOperation &);
        void ForgetDuplicaOn(DOHandle);
        bool FaultRecoveryImpl(DOOperation *);
        bool PerformFaultRecovery(DOHandle, LogicalClockTmpl<unsigned char>);
        void DispatchRMCCall(const CallMethodOperation &);
        void ExecUpdateDataSet(const UpdateDataSetOperation &);
        static bool SendConnectOrphanRequest(FetchContext *, DOHandle);
        void SendToAllDuplicas(Message *, unsigned int);
        void SendToSomeDuplicas(Selection *, Message *, unsigned int);
        bool IsGlobal() const;
        bool EmigrateTo(MigrationContext *, DOHandle);
        bool MigrationInProgress() const;
        bool AttemptEmigration(DOHandle);
        void PrepareToLeave();
        DOHandle SelectNewLocation(unsigned int);
        bool IsADuplica() const;
        bool IsADuplicationMaster() const;
        bool IsAWellKnownDO() const;
        unsigned int GetMasterID() const;
        void ReleaseMainReference();
        void CompleteDecreaseRefCount();
        bool Refresh();
        void SetFlag(unsigned short);
        void ClearFlag(unsigned short);
        bool DeleteMainRef();
        bool DeleteDuplicaMainRef();
        bool DeleteMainRefImpl();
        bool ConnectOrphanDuplica();
        bool Publish(unsigned int);
        void FillDuplicaStationsList(qList<DOHandle> *);
        static DuplicatedObject *CreateWellKnown(WKHandle &);
        static DuplicatedObject *Create(unsigned int, unsigned int);
        static DuplicatedObject *Create(unsigned int, DOID);
        static DuplicatedObject *CreateMasterImpl(DOHandle, unsigned int, DOID);
        static DuplicatedObject *CreateDuplica(DOHandle, const MasterStationRef &);
        void SetStationSpecialRelevance();
        void ReleaseReferenceToMaster();
        void AcquireReferenceToMaster();
        bool IsDuplicatedOn(DOHandle);
        bool IsInCachedDuplicationSet(DOHandle) const;
        void AddToCachedDuplicationSet(const Station *);
        bool RemoveFromCachedDuplicationSet(DOHandle);
        bool IsInDuplicationSet(DOHandle) const;
        void AddToDuplicationSet(DuplicatedObject *);
        bool RemoveFromDuplicationSet(DOHandle);
        static void RemoveAllDuplicasOnLeavingStation(DOHandle);
        bool IsASettledMaster() const;

        void SetInitialState(const QEvent &);
        StateFuncFactory ValidState(const QEvent &);
        StateFuncFactory InvalidState(const QEvent &);
        StateFuncFactory InitialState(const QEvent &);
        StateFuncFactory DuplicationMasterState(const QEvent &);
        StateFuncFactory UnpublishedMasterState(const QEvent &);
        StateFuncFactory UnidentifiedMasterState(const QEvent &);
        StateFuncFactory InStoreMasterState(const QEvent &);
        StateFuncFactory DeletedMasterState(const QEvent &);
        StateFuncFactory DuplicaState(const QEvent &);
        StateFuncFactory InStoreDuplicaState(const QEvent &);
        StateFuncFactory OrphanDuplicaState(const QEvent &);
        StateFuncFactory ConnectedDuplicaState(const QEvent &);
        StateFuncFactory DeletedDuplicaState(const QEvent &);

        DOHandle GetHandle() const {
            DOID oID = m_dohMyself.GetID();
            if (oID.m_uiValue == 0) {
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return DOHandle(0);
            } else {
                return m_dohMyself;
            }
        }

        // The root DO class id (retail's DuplicatedObject check at 0x82A76568
        // passes 1).
        static unsigned int GetClassID() { return 1; }

        bool FlagIsSet(unsigned short f) const { return (m_uiFlags & f) == f; }
        bool IsDeleted() const { return !FlagIsSet(1); }
        void SetDOID(DOID oID) { m_dohMyself.SetDOID(oID); }
        void SetDOClassID(unsigned int ui) { m_dohMyself.SetDOClassID(ui); }
        DOClass *GetDOClass() const { return GetDOClass(m_dohMyself.GetDOClassID()); }

        void ReleaseReference(bool bRelevance) {
            bool bKeep = true;
            {
                volatile ScopedCS cs(s_csRefCount);
                if (bRelevance) {
                    m_uiRelevanceCount--;
                }
                m_uiRefCount--;
                if (m_uiRefCount == 0) {
                    m_uiRefCount++;
                    bKeep = false;
                }
            }
            if (!bKeep) {
                ReleaseMainReference();
            }
        }

        void DecreaseRefCount() {
            bool bComplete = false;
            {
                volatile ScopedCS cs(s_csRefCount);
                m_uiRefCount--;
                if (m_uiRefCount == 0) {
                    bComplete = true;
                }
            }
            if (bComplete) {
                CompleteDecreaseRefCount();
            }
        }

        void AcquireMainReference() {
            volatile ScopedCS cs(s_csRefCount);
            m_uiRefCount++;
        }

        static CriticalSection s_csRefCount;

        unsigned short m_uiRefCount; // 0xc
        unsigned short m_uiRelevanceCount; // 0xe
        MasterStationRef m_refMasterStation; // 0x10
        unsigned short m_uiFlags; // 0x20
        Selection m_setDuplicationSet; // 0x24
        DOHandle m_dohMyself; // 0x48
        Selection m_setCachedDuplicationSet; // 0x4c
    };

}
