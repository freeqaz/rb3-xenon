// Quazal NetZ - .\RootDODDL.cpp
//
// The DDL-generated code for the root duplicated object class: the class
// descriptor _DOC_RootDO and the generated base _DO_RootDO that RootDO (and,
// through it, every NetZ DO class) derives from.
//
// The retail TU is 0x82A99368..0x82A99C48. It starts at _DOC_RootDO::Create
// (the eight VirtualRootObject allocation operators before it are called from
// many earlier TUs, so they are that class's own definitions, not COMDATs of
// this file) and ends where DOClass::DOClass begins: that constructor stores
// the vtable at 0x8217EFE8, which sits directly before the ".\DOClass.cpp"
// string in .rdata.
//
// Built /Od /Ob1 with EH off, like the other Quazal TUs: the helpers the
// classes below define in the class body are expanded in place, the ones
// defined out of line are called. Several _DOC_RootDO overrides (Delete,
// ApproveFaultRecovery, ApproveEmigration, Trace, DataSetsOperation) and the
// DOClassTemplate forwarders are byte-identical to the same function in every
// other DDL TU, so the linker folded them and retail's vtable points at a copy
// elsewhere; they are still defined here, in source order.
//
// The NetZ classes this file needs are declared here only as far as it uses
// them; DuplicatedObject comes from its own header.

#include "ObjDup/DuplicatedObject.h"
#include "Platform/String.h"
#include "Platform/Time.h"
#include "Plugins/Message.h"

namespace Quazal {

    class Station;
    class Adapter;
    class Variable;
    class CallMethodOperation;
    class RMCContext;

    class DupSpace {
    public:
        enum _Role {
        };
    };

    // Hands out the ushort ids the DDL uses for its actions.
    class MethodIDGenerator {
    public:
        static unsigned short AssignID(String);
        static unsigned short GetID(String);
    };

    ByteStream &operator>>(ByteStream &, DOHandle &);

    // Writes a value through a const reference. A DOHandle is written as its
    // raw value: the value is returned into one temp and the reference binds a
    // second, which is the pair of stack temps retail stores before each Append.
    template <class T>
    inline ByteStream &operator<<(ByteStream &s, const T &t) {
        s.Append((const unsigned char *)&t, sizeof(T), 1);
        return s;
    }

    inline unsigned int DOHandleValue(const DOHandle &h) { return h.mValue; }

    inline ByteStream &operator<<(ByteStream &s, const DOHandle &h) { return s << DOHandleValue(h); }

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

        static void InitDOClass(unsigned int);
        void SpecificAddDSToDiscoveryMessage(Message *, Station *);
        bool SpecificExtractDSFromDiscoveryMessage(Message *);
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        bool SpecificExtractADataset(Message *, unsigned char);

        bool AddDuplicaLocation_OnMaster(DOHandle, DOHandle, bool, DOHandle);
        void CalleeAddDuplicaLocationStub(Message *);
        bool DeleteDuplica_OnMaster(DOHandle);
        void CalleeDeleteDuplicaStub(Message *);
        bool RemoveFromCachedDuplicationSet_OnDuplicas(DOHandle);
        void CalleeRemoveFromCachedDuplicationSetStub(Message *);

        static unsigned int s_uiClassID;
    };

    class RootDO : public _DO_RootDO {
    public:
        RootDO();
        virtual ~RootDO();
        bool AddDuplicaLocation(DOHandle, DOHandle, bool, DOHandle);
        bool DeleteDuplica(DOHandle);
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

    // _DOC_RootDO

    DuplicatedObject *_DOC_RootDO::Create() { return new (__FILE__, 11) RootDO(); }

    void _DOC_RootDO::Delete(DuplicatedObject *pDO) { delete pDO; }

    const char *_DOC_RootDO::GetClassNameString() const { return "RootDO"; }

    bool _DOC_RootDO::ApproveFaultRecovery(DuplicatedObject *pDO) {
        return pDO->ApproveFaultRecovery();
    }

    bool _DOC_RootDO::ApproveEmigration(DuplicatedObject *pDO, unsigned int uiReason) {
        return pDO->ApproveEmigration(uiReason);
    }

    void _DOC_RootDO::Trace(DuplicatedObject *pDO, unsigned int uiFlags) { pDO->Trace(uiFlags); }

    _DOC_RootDO::_DOC_RootDO(unsigned int uiClassID)
        : DOClassTemplate<_DO_RootDO, DOClass>(uiClassID) {
        m_usAddDuplicaLocationID = MethodIDGenerator::AssignID(String("AddDuplicaLocation"));
        m_usDeleteDuplicaID = MethodIDGenerator::AssignID(String("DeleteDuplica"));
        m_usRemoveFromCachedDuplicationSetID =
            MethodIDGenerator::AssignID(String("RemoveFromCachedDuplicationSet"));
    }

    // The root class is a kind of itself and of DuplicatedObject (class id 1).
    inline bool IsAKindOfDuplicatedObject(unsigned int uiClassID) {
        if (uiClassID == 1)
            return true;
        else
            return false;
    }

    bool _DOC_RootDO::IsAKindOf(unsigned int uiClassID) {
        if (_DO_RootDO::s_uiClassID == uiClassID)
            return true;
        return IsAKindOfDuplicatedObject(uiClassID);
    }

    void _DOC_RootDO::DataSetsOperation(unsigned int uiOperation) {
        DOClass::DataSetsOperation(uiOperation);
    }

    bool _DOC_RootDO::FormatVariableValue(
        const DuplicatedObject *, Variable *, Variable *, String *
    ) const {
        return false;
    }

    bool _DOC_RootDO::DispatchAction(DuplicatedObject *pDO, unsigned short usActionID, Message *pMsg) {
        if (usActionID == m_usAddDuplicaLocationID) {
            static_cast<_DO_RootDO *>(pDO)->CalleeAddDuplicaLocationStub(pMsg);
            return true;
        }
        if (usActionID == m_usDeleteDuplicaID) {
            static_cast<_DO_RootDO *>(pDO)->CalleeDeleteDuplicaStub(pMsg);
            return true;
        }
        if (usActionID == m_usRemoveFromCachedDuplicationSetID) {
            static_cast<_DO_RootDO *>(pDO)->CalleeRemoveFromCachedDuplicationSetStub(pMsg);
            return true;
        }
        return false;
    }

    void _DOC_RootDO::DispatchRMCCall(const CallMethodOperation &oOperation) {
        DOClass::DispatchRMCCall(oOperation);
    }

    // bool: DOClass's (0x82AB2278) returns false, and _DOC_Station's override
    // returns 1 or this function's value.
    bool _DOC_RootDO::DispatchRMCResult(RMCContext *pContext) { return DOClass::DispatchRMCResult(pContext); }

    void _DOC_RootDO::FillDupSpacesInfo(DupSpace::_Role eRole, unsigned int *puiA, unsigned int *puiB) {
        DOClass::FillDupSpacesInfo(eRole, puiA, puiB);
    }

    const char *_DOC_RootDO::GetDatasetNameString(unsigned char) const {
        return "Invalid dataset index";
    }

    // _DO_RootDO

    unsigned int _DO_RootDO::s_uiClassID;

    _DO_RootDO::_DO_RootDO() {
        SetDOClassID(s_uiClassID);
        TestInvariants();
    }

    void _DO_RootDO::InitDOClass(unsigned int uiClassID) {
        s_uiClassID = uiClassID;
        DOClass *pDOClass = new (__FILE__, 60) _DOC_RootDO(uiClassID);
        pDOClass->CompleteInitialisation();
    }

    void _DO_RootDO::SpecificAddDSToDiscoveryMessage(Message *, Station *) {}

    bool _DO_RootDO::SpecificExtractDSFromDiscoveryMessage(Message *) { return true; }

    void _DO_RootDO::CallOperationOnDatasets(DOOperation *pOperation, Operation::_Event eEvent) {
        DuplicatedObject::CallOperationOnDatasets(pOperation, eEvent);
    }

    bool _DO_RootDO::SpecificUpdate(DataSet *pDataSet, const Time &tTime) {
        return DuplicatedObject::SpecificUpdate(pDataSet, tTime);
    }

    bool _DO_RootDO::SpecificRefresh(DataSet *pDataSet, const Time &tTime) {
        return DuplicatedObject::SpecificRefresh(pDataSet, tTime);
    }

    bool _DO_RootDO::SpecificExtractADataset(Message *pMsg, unsigned char ucIndex) {
        return DuplicatedObject::SpecificExtractADataset(pMsg, ucIndex);
    }

    bool _DO_RootDO::AddDuplicaLocation_OnMaster(
        DOHandle hDuplicaStation, DOHandle hDuplicaLocation, bool bAddToCache, DOHandle hOrigin
    ) {
        unsigned short usMethodID = MethodIDGenerator::GetID(String("AddDuplicaLocation"));
        Message *pMessage = CreateStubMessage(&usMethodID);
        *pMessage << hDuplicaStation;
        *pMessage << hDuplicaLocation;
        *pMessage << bAddToCache;
        *pMessage << hOrigin;
        return SendStubMessage(false, pMessage);
    }

    void _DO_RootDO::CalleeAddDuplicaLocationStub(Message *pMsg) {
        DOHandle hStation;
        *pMsg >> hStation;
        DOHandle hLocation;
        *pMsg >> hLocation;
        bool bAddToCache;
        *pMsg >> bAddToCache;
        DOHandle hOrigin;
        *pMsg >> hOrigin;
        static_cast<RootDO *>(this)->AddDuplicaLocation(hStation, hLocation, bAddToCache, hOrigin);
    }

    bool _DO_RootDO::DeleteDuplica_OnMaster(DOHandle hDuplica) {
        unsigned short usMethodID = MethodIDGenerator::GetID(String("DeleteDuplica"));
        Message *pMessage = CreateStubMessage(&usMethodID);
        *pMessage << hDuplica;
        return SendStubMessage(false, pMessage);
    }

    void _DO_RootDO::CalleeDeleteDuplicaStub(Message *pMsg) {
        DOHandle hDuplica;
        *pMsg >> hDuplica;
        static_cast<RootDO *>(this)->DeleteDuplica(hDuplica);
    }

    bool _DO_RootDO::RemoveFromCachedDuplicationSet_OnDuplicas(DOHandle hStation) {
        unsigned short usMethodID = MethodIDGenerator::GetID(String("RemoveFromCachedDuplicationSet"));
        Message *pMessage = CreateStubMessage(&usMethodID);
        *pMessage << hStation;
        return SendStubMessage(true, pMessage);
    }

    void _DO_RootDO::CalleeRemoveFromCachedDuplicationSetStub(Message *pMsg) {
        DOHandle hStation;
        *pMsg >> hStation;
        RemoveFromCachedDuplicationSet(hStation);
    }

}
