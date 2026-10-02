#pragma once

#include "DuplicatedObject.h"
#include "ObjDup/DOClass.h"
namespace Quazal {
    class _DO_RootDO : public DuplicatedObject {
    public:
        _DO_RootDO();
        virtual ~_DO_RootDO() {}
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;

        // Sends the RemoveFromCachedDuplicationSet action to the duplicas
        // (.\RootDODDL.cpp); DuplicatedObject::ExecChangeDupSet calls it.
        bool RemoveFromCachedDuplicationSet_OnDuplicas(DOHandle);
        // The dataset dispatchers each DDL-generated DO extends (the root DO
        // has no datasets of its own).
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        void AddDSToDiscoveryMessage(Message *, Station *);
        bool ExtractDSFromDiscoveryMessage(Message *);
        bool ExtractADataset(Message *, unsigned char);
        static unsigned int GetClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;
    };

    class _DOC_RootDO : public DOClassTemplate<_DO_RootDO, DOClass> {
    public:
        _DOC_RootDO(unsigned int);
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create();
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool
        FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        unsigned short m_usMethodIDs[3]; // 0x28
    };
}