#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORef.h"
#include "ObjDup/MasterStationRef.h"

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

        DORef m_refNewMasterStation; // 0x20
        MasterStationRef m_refMasterStation; // 0x2c
        const qList<DOHandle> *m_plstDuplicaStations; // 0x3c
        Context m_eContext; // 0x40
    };
}
