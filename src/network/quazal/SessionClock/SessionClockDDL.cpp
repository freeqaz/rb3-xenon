// Quazal NetZ - .\SessionClock\SessionClockDDL.cpp
//
// The DDL-generated code for the SessionClock duplicated object: the class
// descriptor _DOC_SessionClock and the generated base _DO_SessionClock that
// SessionClock derives from. SessionClock has three RMC methods, AdjustTime,
// SyncRequest and SyncResponse, and no actions or datasets of its own.
//
// The retail TU is 0x82AD2548..0x82AD30D0. It starts at _DOC_SessionClock::
// Create (the code before it belongs to the class whose vtable precedes this
// TU's file string in .rdata) and ends after the DOClassTemplate COMDAT at
// 0x82AD3090. 0x82AD30D0 and 0x82AD3130 are LocalClock's constructor and
// destructor (only LocalClock's code calls them), so LocalClock's TU starts
// there.
//
// Built /Od /Ob1 with EH off, like RootDODDL. _DOC_SessionClock derives from
// _DOC_RootDO through DOClassTemplate (its constructor calls _DOC_RootDO's), and
// the overrides that are identical in every DDL TU were folded by the linker,
// so retail's vtable points at a copy elsewhere; they are still defined here,
// in source order. The same holds for the void result stubs of the three
// methods (the surviving copy is 0x82AD2CB8) and for the caller stub of
// AdjustTime, which nothing references.
//
// The NetZ classes this file needs are declared here only as far as it uses
// them; DuplicatedObject comes from its own header.

#include "ObjDup/DuplicatedObject.h"
#include "Platform/String.h"
#include "Platform/Time.h"
#include "Plugins/Message.h"
#include "Platform/Result.h"

namespace Quazal {

    class Station;
    class Adapter;
    class Variable;

    class DupSpace {
    public:
        enum _Role {
        };
    };

    // Hands out the ushort ids the DDL uses for its methods.
    class MethodIDGenerator {
    public:
        static unsigned short AssignID(String);
        static unsigned short GetID(String);
    };

    // The shared header declares CallMethodOperation without its members (they
    // live in the RMC code); this view adds the ones the stubs below read.
    class RMCOperation : public CallMethodOperation {
    public:
        DuplicatedObject *GetTargetObject() const { return m_refTargetObject.m_poReferencedDO; }
        unsigned short GetMethodID() const { return m_usMethodID; }
        Message *GetParameters() const;
        unsigned int SignalSuccess() const;
        void SetOutcome(qResult oOutcome) const { m_oOutcome = oOutcome; }

        char m_pad20[0x12];
        unsigned short m_usMethodID; // 0x32
        char m_pad34[0xC];
        mutable qResult m_oOutcome; // 0x40
    };


    class RMCContext {
    public:
        bool PrepareCall(DOHandle, unsigned short);
        Message *GetMessage();
        bool CallMethod();
        Message *GetResponse();
        unsigned short GetMethodID() const { return m_usMethodID; }

        char m_pad0[0xA0];
        unsigned short m_usMethodID; // 0xa0
    };

    class WKHandle : public DOHandle {
    public:
        bool IsAKindOf(unsigned int uiClassID) const { return IsOfClass(uiClassID); }
        bool IsOfClass(unsigned int) const;
    };

    class DOClass : public RootObject {
    public:
        DOClass(unsigned int);
        virtual ~DOClass();
        virtual unsigned int GetCategory();
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create() = 0;
        virtual void Delete(DuplicatedObject *) = 0;
        virtual bool ApproveFaultRecovery(DuplicatedObject *) = 0;
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int) = 0;
        virtual void Trace(DuplicatedObject *, unsigned int) = 0;
        virtual Adapter *CreateAdapter(DuplicatedObject *, unsigned int);
        virtual void DeleteAdapter(DuplicatedObject *, unsigned int, Adapter *);
        virtual void SpecificAddDSToDiscoveryMessage(DuplicatedObject *, Station *, Message *) = 0;
        virtual bool SpecificExtractDSFromDiscoveryMessage(DuplicatedObject *, Message *) = 0;
        virtual bool SpecificExtractADataset(DuplicatedObject *, Message *, unsigned char) = 0;
        virtual bool SpecificUpdate(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool SpecificRefresh(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const = 0;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual bool ValidCastTowards(unsigned int);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        void CompleteInitialisation();

        unsigned int m_uiClassID; // 0x4
        char m_pad8[0x20]; // 0x8
    };

    template <class T, class Base>
    class DOClassTemplate : public Base {
    public:
        DOClassTemplate(unsigned int uiClassID) : Base(uiClassID) {}
        virtual ~DOClassTemplate() {}
        virtual void SpecificAddDSToDiscoveryMessage(DuplicatedObject *pDO, Station *pStation, Message *pMsg) {
            static_cast<T *>(pDO)->SpecificAddDSToDiscoveryMessage(pMsg, pStation);
        }
        virtual bool SpecificExtractDSFromDiscoveryMessage(DuplicatedObject *pDO, Message *pMsg) {
            return static_cast<T *>(pDO)->SpecificExtractDSFromDiscoveryMessage(pMsg);
        }
        virtual bool SpecificExtractADataset(DuplicatedObject *pDO, Message *pMsg, unsigned char ucIndex) {
            return static_cast<T *>(pDO)->SpecificExtractADataset(pMsg, ucIndex);
        }
        virtual bool SpecificUpdate(DuplicatedObject *pDO, DataSet *pDataSet, const Time &tTime) {
            return static_cast<T *>(pDO)->SpecificUpdate(pDataSet, tTime);
        }
        virtual bool SpecificRefresh(DuplicatedObject *pDO, DataSet *pDataSet, const Time &tTime) {
            return static_cast<T *>(pDO)->SpecificRefresh(pDataSet, tTime);
        }
        virtual bool ValidCastTowards(unsigned int uiClassID) { return IsAKindOf(uiClassID); }
    };

    class _DO_RootDO : public DuplicatedObject {
    public:
        _DO_RootDO();
        virtual ~_DO_RootDO() {}
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const { return true; }

        void SpecificAddDSToDiscoveryMessage(Message *, Station *);
        bool SpecificExtractDSFromDiscoveryMessage(Message *);
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        bool SpecificExtractADataset(Message *, unsigned char);
    };

    class RootDO : public _DO_RootDO {
    public:
        RootDO();
        virtual ~RootDO();
    };

    class _DOC_RootDO : public DOClassTemplate<_DO_RootDO, DOClass> {
    public:
        _DOC_RootDO(unsigned int);
        virtual DuplicatedObject *Create();
        virtual void Delete(DuplicatedObject *);
        virtual const char *GetClassNameString() const;
        virtual bool ApproveFaultRecovery(DuplicatedObject *);
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int);
        virtual void Trace(DuplicatedObject *, unsigned int);
        virtual bool IsAKindOf(unsigned int);
        virtual void DataSetsOperation(unsigned int);
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);
        virtual const char *GetDatasetNameString(unsigned char) const;

        unsigned short m_usAddDuplicaLocationID; // 0x28
        unsigned short m_usDeleteDuplicaID; // 0x2a
        unsigned short m_usRemoveFromCachedDuplicationSetID; // 0x2c
    };

    class SessionClock;

    class _DO_SessionClock : public RootDO {
    public:
        _DO_SessionClock();
        virtual ~_DO_SessionClock() {}
        virtual bool HasGlobalDOProperty() const;
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static void InitDOClass(unsigned int);
        static SessionClock *CreateWellKnown(WKHandle &);
        void SpecificAddDSToDiscoveryMessage(Message *, Station *);
        bool SpecificExtractDSFromDiscoveryMessage(Message *);
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        bool SpecificExtractADataset(Message *, unsigned char);

        bool CallAdjustTime(RMCContext *, const long long &);
        void CalleeAdjustTimeStub(const RMCOperation &);
        static void ExtractAdjustTimeResult(RMCContext *);
        bool CallSyncRequest(RMCContext *, const unsigned long long &);
        void CalleeSyncRequestStub(const RMCOperation &);
        static void ExtractSyncRequestResult(RMCContext *);
        bool CallSyncResponse(
            RMCContext *, const unsigned long long &, const unsigned long long &, const unsigned int &
        );
        void CalleeSyncResponseStub(const RMCOperation &);
        static void ExtractSyncResponseResult(RMCContext *);

        static unsigned int s_uiClassID;
    };

    class SessionClock : public _DO_SessionClock {
    public:
        SessionClock();
        void AdjustTime(long long);
        void SyncRequest(Time);
        void SyncResponse(Time, Time, unsigned int);

        char m_padSC[0x40];
    };

    class _DOC_SessionClock : public DOClassTemplate<_DO_SessionClock, _DOC_RootDO> {
    public:
        _DOC_SessionClock(unsigned int);
        virtual DuplicatedObject *Create();
        virtual void Delete(DuplicatedObject *);
        virtual const char *GetClassNameString() const;
        virtual bool ApproveFaultRecovery(DuplicatedObject *);
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int);
        virtual void Trace(DuplicatedObject *, unsigned int);
        virtual bool IsAKindOf(unsigned int);
        virtual void DataSetsOperation(unsigned int);
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);
        virtual const char *GetDatasetNameString(unsigned char) const;

        unsigned short m_usAdjustTimeID; // 0x30
        unsigned short m_usSyncRequestID; // 0x32
        unsigned short m_usSyncResponseID; // 0x34
    };

    // _DOC_SessionClock

    DuplicatedObject *_DOC_SessionClock::Create() { return new (__FILE__, 11) SessionClock(); }

    void _DOC_SessionClock::Delete(DuplicatedObject *pDO) { delete pDO; }

    const char *_DOC_SessionClock::GetClassNameString() const { return "SessionClock"; }

    bool _DOC_SessionClock::ApproveFaultRecovery(DuplicatedObject *pDO) {
        return pDO->ApproveFaultRecovery();
    }

    bool _DOC_SessionClock::ApproveEmigration(DuplicatedObject *pDO, unsigned int uiReason) {
        return pDO->ApproveEmigration(uiReason);
    }

    void _DOC_SessionClock::Trace(DuplicatedObject *pDO, unsigned int uiFlags) { pDO->Trace(uiFlags); }

    _DOC_SessionClock::_DOC_SessionClock(unsigned int uiClassID)
        : DOClassTemplate<_DO_SessionClock, _DOC_RootDO>(uiClassID) {
        m_usAdjustTimeID = MethodIDGenerator::AssignID(String("AdjustTime"));
        m_usSyncRequestID = MethodIDGenerator::AssignID(String("SyncRequest"));
        m_usSyncResponseID = MethodIDGenerator::AssignID(String("SyncResponse"));
    }

    bool _DOC_SessionClock::IsAKindOf(unsigned int uiClassID) {
        if (_DO_SessionClock::s_uiClassID == uiClassID)
            return true;
        return _DOC_RootDO::IsAKindOf(uiClassID);
    }

    void _DOC_SessionClock::DataSetsOperation(unsigned int uiOperation) {
        _DOC_RootDO::DataSetsOperation(uiOperation);
    }

    bool _DOC_SessionClock::FormatVariableValue(
        const DuplicatedObject *pDO, Variable *pVar, Variable *pSubVar, String *pString
    ) const {
        return _DOC_RootDO::FormatVariableValue(pDO, pVar, pSubVar, pString);
    }

    bool _DOC_SessionClock::DispatchAction(DuplicatedObject *pDO, unsigned short usActionID, Message *pMsg) {
        return _DOC_RootDO::DispatchAction(pDO, usActionID, pMsg);
    }

    #define RMC_OP static_cast<const RMCOperation &>(oOperation)

    void _DOC_SessionClock::DispatchRMCCall(const CallMethodOperation &oOperation) {
        if (RMC_OP.GetMethodID() == m_usAdjustTimeID) {
            static_cast<_DO_SessionClock *>(RMC_OP.GetTargetObject())->CalleeAdjustTimeStub(RMC_OP);
            RMC_OP.SetOutcome(qResult(0x60001));
        } else if (RMC_OP.GetMethodID() == m_usSyncRequestID) {
            static_cast<_DO_SessionClock *>(RMC_OP.GetTargetObject())->CalleeSyncRequestStub(RMC_OP);
            RMC_OP.SetOutcome(qResult(0x60001));
        } else if (RMC_OP.GetMethodID() == m_usSyncResponseID) {
            static_cast<_DO_SessionClock *>(RMC_OP.GetTargetObject())->CalleeSyncResponseStub(RMC_OP);
            RMC_OP.SetOutcome(qResult(0x60001));
        } else {
            _DOC_RootDO::DispatchRMCCall(oOperation);
        }
    }

#undef RMC_OP

    bool _DOC_SessionClock::DispatchRMCResult(RMCContext *pContext) {
        if (pContext->GetMethodID() == m_usAdjustTimeID) {
            _DO_SessionClock::ExtractAdjustTimeResult(pContext);
            return true;
        }
        if (pContext->GetMethodID() == m_usSyncRequestID) {
            _DO_SessionClock::ExtractSyncRequestResult(pContext);
            return true;
        }
        if (pContext->GetMethodID() == m_usSyncResponseID) {
            _DO_SessionClock::ExtractSyncResponseResult(pContext);
            return true;
        }
        return _DOC_RootDO::DispatchRMCResult(pContext);
    }

    void _DOC_SessionClock::FillDupSpacesInfo(DupSpace::_Role eRole, unsigned int *puiA, unsigned int *puiB) {
        _DOC_RootDO::FillDupSpacesInfo(eRole, puiA, puiB);
    }

    const char *_DOC_SessionClock::GetDatasetNameString(unsigned char ucIndex) const {
        return _DOC_RootDO::GetDatasetNameString(ucIndex);
    }

    // _DO_SessionClock

    unsigned int _DO_SessionClock::s_uiClassID;

    _DO_SessionClock::_DO_SessionClock() {
        SetDOClassID(s_uiClassID);
        TestInvariants();
    }

    void _DO_SessionClock::InitDOClass(unsigned int uiClassID) {
        s_uiClassID = uiClassID;
        DOClass *pDOClass = new (__FILE__, 65) _DOC_SessionClock(uiClassID);
        pDOClass->CompleteInitialisation();
    }

    SessionClock *_DO_SessionClock::CreateWellKnown(WKHandle &hWK) {
        if (!hWK.IsAKindOf(s_uiClassID)) {
            SystemError::SignalError(0, 0, 0xE0030007, 0);
            return 0;
        }
        return static_cast<SessionClock *>(DuplicatedObject::CreateWellKnown(hWK));
    }

    bool _DO_SessionClock::HasGlobalDOProperty() const { return true; }

    void _DO_SessionClock::SpecificAddDSToDiscoveryMessage(Message *pMsg, Station *pStation) {
        RootDO::SpecificAddDSToDiscoveryMessage(pMsg, pStation);
    }

    bool _DO_SessionClock::SpecificExtractDSFromDiscoveryMessage(Message *pMsg) {
        return RootDO::SpecificExtractDSFromDiscoveryMessage(pMsg);
    }

    void _DO_SessionClock::CallOperationOnDatasets(DOOperation *pOperation, Operation::_Event eEvent) {
        RootDO::CallOperationOnDatasets(pOperation, eEvent);
    }

    bool _DO_SessionClock::SpecificUpdate(DataSet *pDataSet, const Time &tTime) {
        return RootDO::SpecificUpdate(pDataSet, tTime);
    }

    bool _DO_SessionClock::SpecificRefresh(DataSet *pDataSet, const Time &tTime) {
        return RootDO::SpecificRefresh(pDataSet, tTime);
    }

    bool _DO_SessionClock::SpecificExtractADataset(Message *pMsg, unsigned char ucIndex) {
        return RootDO::SpecificExtractADataset(pMsg, ucIndex);
    }

    bool _DO_SessionClock::IsACoreDO() const { return true; }

    bool _DO_SessionClock::IsABootstrapDO() const { return true; }

    // AdjustTime

    bool _DO_SessionClock::CallAdjustTime(RMCContext *pContext, const long long &i64Offset) {
        if (!pContext->PrepareCall(GetHandle(), MethodIDGenerator::GetID(String("AdjustTime"))))
            return false;
        Message *pMsg = pContext->GetMessage();
        pMsg->Append((const unsigned char *)&i64Offset, 8, 1);
        return pContext->CallMethod();
    }

    void _DO_SessionClock::CalleeAdjustTimeStub(const RMCOperation &oOperation) {
        const RMCOperation *pOperation = &oOperation;
        Message *pMsg = oOperation.GetParameters();
        long long i64Offset;
        pMsg->Extract((unsigned char *)&i64Offset, 8, 1);
        static_cast<SessionClock *>(this)->AdjustTime(i64Offset);
        pMsg = (Message *)oOperation.SignalSuccess();
    }

    void _DO_SessionClock::ExtractAdjustTimeResult(RMCContext *pContext) {
        Message *pMsg = pContext->GetResponse();
        unsigned int uiUnused = 0;
    }

    // SyncRequest

    bool _DO_SessionClock::CallSyncRequest(RMCContext *pContext, const unsigned long long &ui64RequestTime) {
        if (!pContext->PrepareCall(GetHandle(), MethodIDGenerator::GetID(String("SyncRequest"))))
            return false;
        Message *pMsg = pContext->GetMessage();
        pMsg->Append((const unsigned char *)&ui64RequestTime, 8, 1);
        return pContext->CallMethod();
    }

    void _DO_SessionClock::CalleeSyncRequestStub(const RMCOperation &oOperation) {
        const RMCOperation *pOperation = &oOperation;
        Message *pMsg = oOperation.GetParameters();
        unsigned long long ui64RequestTime;
        pMsg->Extract((unsigned char *)&ui64RequestTime, 8, 1);
        static_cast<SessionClock *>(this)->SyncRequest(ui64RequestTime);
        pMsg = (Message *)oOperation.SignalSuccess();
    }

    void _DO_SessionClock::ExtractSyncRequestResult(RMCContext *pContext) {
        Message *pMsg = pContext->GetResponse();
        unsigned int uiUnused = 0;
    }

    // SyncResponse

    bool _DO_SessionClock::CallSyncResponse(
        RMCContext *pContext,
        const unsigned long long &ui64RequestTime,
        const unsigned long long &ui64ServerTime,
        const unsigned int &uiDelay
    ) {
        if (!pContext->PrepareCall(GetHandle(), MethodIDGenerator::GetID(String("SyncResponse"))))
            return false;
        Message *pMsg = pContext->GetMessage();
        pMsg->Append((const unsigned char *)&ui64RequestTime, 8, 1);
        pMsg->Append((const unsigned char *)&ui64ServerTime, 8, 1);
        pMsg->Append((const unsigned char *)&uiDelay, 4, 1);
        return pContext->CallMethod();
    }

    void _DO_SessionClock::CalleeSyncResponseStub(const RMCOperation &oOperation) {
        const RMCOperation *pOperation = &oOperation;
        Message *pMsg = oOperation.GetParameters();
        unsigned long long ui64RequestTime;
        pMsg->Extract((unsigned char *)&ui64RequestTime, 8, 1);
        unsigned long long ui64ServerTime;
        pMsg->Extract((unsigned char *)&ui64ServerTime, 8, 1);
        unsigned int uiDelay;
        pMsg->Extract((unsigned char *)&uiDelay, 4, 1);
        static_cast<SessionClock *>(this)->SyncResponse(ui64RequestTime, ui64ServerTime, uiDelay);
        pMsg = (Message *)oOperation.SignalSuccess();
    }

    void _DO_SessionClock::ExtractSyncResponseResult(RMCContext *pContext) {
        Message *pMsg = pContext->GetResponse();
        unsigned int uiUnused = 0;
    }

}
