#pragma once
#include "Core/Operation.h"
#include "Platform/Result.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DOID.h"
#include "ObjDup/DORef.h"
#include "ObjDup/MasterStationRef.h"
#include "Platform/LogicalClock.h"
#include "Platform/qStd.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;
    class Station;

    // Layout from the retail ctor 0x82AF2128: Operation's 0x14 bytes, then the
    // target DORef.
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
        bool IsAMaster() const { return m_bIsAMaster; }
        bool IsADuplica() const { return !m_bIsAMaster; }

        bool m_bIsAMaster; // 0x20
        Message *m_pMessage; // 0x24
    };

    // Layout from the retail ctor 0x82AB4570.
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
        int GetContext() const { return m_eContext; }
        DOHandle GetStation() const { return m_refStation.m_hReferencedDO; }
        DOHandle GetNewMasterStation() const { return m_refNewMaster.m_hReferencedDO; }
        const qList<DOHandle> *GetStationList() const { return m_pStationList; }

        DORef m_refStation; // 0x20
        MasterStationRef m_refNewMaster; // 0x2c
        const qList<DOHandle> *m_pStationList; // 0x3c
        Context m_eContext; // 0x40
    };

    // Layout from the retail ctor 0x82AB48D0.
    // Signal 7; only cast to here (its members live in the RMC code).
    class CallMethodOperation : public DOOperation {
    public:
        Message *GetCallMessage() const;
        Message *PrepareSuccessMessage() const;
        unsigned short GetMethodID() const { return m_usMethodID; }
        // Taken by value: retail copies the result into a temp, then assigns.
        void SetReturnValue(qResult oResult) const { m_oResult = oResult; }

        unsigned char m_pad20[0x12];
        unsigned short m_usMethodID; // 0x32
        unsigned char m_pad34[0xc];
        mutable qResult m_oResult; // 0x40
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

        MasterStationRef m_refNewMaster; // 0x20
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

        DOID GetDOID() const { return m_oDOID; }

        MasterStationRef m_refMasterStation; // 0x20
        DOID m_oDOID; // 0x30
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
        virtual DOHandle GetImplicitStationConnection() const;
        virtual DOOperation *Clone() const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        bool IsARemoval() const { return !m_bAdd; }
        unsigned short GetMigrationContext() const { return m_uiMigrationContext; }
        Context GetContext() const { return m_eContext; }
        unsigned char GetFlags() const { return m_ucFlags; }

        DORef m_refStation; // 0x20
        bool m_bAdd; // 0x2c
        unsigned short m_uiMigrationContext; // 0x2e
        Context m_eContext; // 0x30
        unsigned char m_ucFlags; // 0x34
    };

    // Lane-chosen name: the retail scope object at 0x82ABEB78/0x82ABEBD8.
    class OperationScope : public RootObject {
    public:
        OperationScope(int);
        ~OperationScope();

        int m_iType; // 0x0
    };
}
