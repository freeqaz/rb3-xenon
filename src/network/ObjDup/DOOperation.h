#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/LogicalClockTmpl.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
        DOOperation(DOHandle, DOHandle);
        virtual ~DOOperation();

        DOHandle m_hTargetDO; // 0x14
        DuplicatedObject *m_pTargetDO; // 0x18
        unsigned int unk1c; // 0x1c
    };

    class UpdateDataSetOperation : public DOOperation {
    public:
        UpdateDataSetOperation(DOHandle, DuplicatedObject *, unsigned char, Message *);
        UpdateDataSetOperation(DOHandle, DuplicatedObject *, Message *);
        virtual ~UpdateDataSetOperation();

        bool UpdatesAllDataSets() const { return m_bAllDataSets; }
        unsigned char GetDataSetID() const { return m_byDataSetID; }
        Message *GetMessage() const { return m_pMessage; }

        bool m_bAllDataSets; // 0x20
        unsigned char m_byDataSetID; // 0x21
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

        unsigned char unk20[0x10]; // 0x20
    };
}
