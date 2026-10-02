#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DOID.h"
#include "ObjDup/DORef.h"
#include "ObjDup/MasterStationRef.h"
#include "Platform/LogicalClock.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;
    template <class T>
    class qList;

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
        DOOperation(DOHandle, DOHandle);
        virtual ~DOOperation();
        virtual DOHandle GetImplicitStationConnection() const;
        virtual DOOperation *Clone() const;
        virtual bool CallsBackOnDataSet() = 0;
        virtual bool CallsBackOnDataSet(unsigned char) = 0;

        DORef m_refTargetObject; // 0x14
    };

    class RemoveFromStoreOperation : public DOOperation {
    public:
        RemoveFromStoreOperation(DOHandle, DuplicatedObject *, bool, bool);
        virtual ~RemoveFromStoreOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        bool IsADuplicaRemoval() const { return m_bRemoveDuplicas; }

        bool m_bDeleteObject; // 0x20
        bool m_bRemoveDuplicas; // 0x21
    };

    class AddToStoreOperation : public DOOperation {
    public:
        AddToStoreOperation(DOHandle, DuplicatedObject *, bool, Message *);
        virtual ~AddToStoreOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        Message *GetMessage() const { return m_pMessage; }
        bool IsADuplica() const { return !m_bIsAMaster; }

        bool m_bIsAMaster; // 0x20
        Message *m_pMessage; // 0x24
    };

    class ChangeMasterStationOperation : public DOOperation {
    public:
        enum Context {
        };
        ChangeMasterStationOperation(
            DOHandle, DuplicatedObject *, DOHandle, const MasterStationRef &,
            const qList<DOHandle> *, Context
        );
        virtual ~ChangeMasterStationOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        static ChangeMasterStationOperation *DynamicCast(DOOperation *pOp) {
            if (pOp == 0 || pOp->GetType() != 0xd)
                return 0;
            return static_cast<ChangeMasterStationOperation *>(pOp);
        }

        DORef m_refNewMasterStation; // 0x20
        MasterStationRef m_refMasterStation; // 0x2c
        const qList<DOHandle> *m_plstDuplicaStations; // 0x3c
        Context m_eContext; // 0x40
    };

    class UpdateDataSetOperation : public DOOperation {
    public:
        UpdateDataSetOperation(DOHandle, DuplicatedObject *, unsigned char, Message *);
        UpdateDataSetOperation(DOHandle, DuplicatedObject *, Message *);
        virtual ~UpdateDataSetOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        bool UpdatesAllDataSets() const { return m_bAllDataSets; }
        unsigned char GetDataSetID() const { return m_ucDataSetID; }
        Message *GetMessage() const { return m_pMessage; }

        bool m_bAllDataSets; // 0x20
        unsigned char m_ucDataSetID; // 0x21
        Message *m_pMessage; // 0x24
    };

    class FaultRecoveryOperation : public DOOperation {
    public:
        FaultRecoveryOperation(DuplicatedObject *, DOHandle, LogicalClockTmpl<unsigned char>);
        virtual ~FaultRecoveryOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        unsigned char unk20[0x10]; // 0x20
    };

    class CreateMasterOperation : public DOOperation {
    public:
        CreateMasterOperation(DuplicatedObject *, DOHandle, DOID);
        virtual ~CreateMasterOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        unsigned char unk20[0x14];
    };

    class ChangeDupSetOperation : public DOOperation {
    public:
        enum Context {
        };
        ChangeDupSetOperation(DOHandle, DuplicatedObject *, DOHandle, bool, Context);
        virtual ~ChangeDupSetOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        unsigned char unk20[0x18];
    };
}
