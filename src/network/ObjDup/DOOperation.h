#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORef.h"
#include "ObjDup/MasterStationRef.h"
#include "Platform/qStd.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
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

        bool IsADuplicaRemoval() const { return m_b21; }

        bool m_b20; // 0x20
        bool m_b21; // 0x21
    };

    class AddToStoreOperation : public DOOperation {
    public:
        AddToStoreOperation(DOHandle, DuplicatedObject *, bool, Message *);
        virtual ~AddToStoreOperation();

        Message *GetMessage() const { return m_pMessage; }
        bool IsADuplica() const { return !m_bMaster; }

        bool m_bMaster; // 0x20
        Message *m_pMessage; // 0x24
    };

    class FaultRecoveryOperation : public DOOperation {
    public:
        FaultRecoveryOperation(DuplicatedObject *, DOHandle, LogicalClockTmpl<unsigned char>);
        virtual ~FaultRecoveryOperation();

        MasterStationRef m_refNewMaster; // 0x20
    };

    // Lane-chosen name: the retail scope object at 0x82ABEB78/0x82ABEBD8.
    class OperationScope : public RootObject {
    public:
        OperationScope(int);
        ~OperationScope();

        int m_iType; // 0x0
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

    class ChangeMasterStationOperation : public DOOperation {
    public:
        int GetContext() const { return m_eContext; }
        const qList<DOHandle> *GetStationList() const { return m_pStationList; }

        DORef m_refStation; // 0x20
        MasterStationRef m_refNewMaster; // 0x2c
        const qList<DOHandle> *m_pStationList; // 0x3c
        int m_eContext; // 0x40
    };
}
