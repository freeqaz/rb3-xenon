#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORef.h"

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

        bool m_b20; // 0x20
        bool m_b21; // 0x21
    };

    class AddToStoreOperation : public DOOperation {
    public:
        AddToStoreOperation(DOHandle, DuplicatedObject *, bool, Message *);
        virtual ~AddToStoreOperation();

        bool m_bMaster; // 0x20
        Message *m_pMessage; // 0x24
    };
}
