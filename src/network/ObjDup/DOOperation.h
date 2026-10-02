#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DOID.h"
#include "ObjDup/DORef.h"

namespace Quazal {
    class DuplicatedObject;
    class Station;

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
        virtual ~DOOperation();

        DORef m_refTargetObject; // 0x14
    };

    class CreateMasterOperation : public DOOperation {
    public:
        CreateMasterOperation(DuplicatedObject *, DOHandle, DOID);
        virtual ~CreateMasterOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;

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

        unsigned char unk20[0x18];
    };

    class ChangeMasterStationOperation : public DOOperation {
    public:
        virtual ~ChangeMasterStationOperation();
        virtual int GetType() const;
        virtual const char *GetClassNameString() const;
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;

        static ChangeMasterStationOperation *DynamicCast(DOOperation *pOp) {
            if (pOp == 0 || pOp->GetType() != 0xd)
                return 0;
            return static_cast<ChangeMasterStationOperation *>(pOp);
        }

        unsigned char unk20[0x10];
        DOHandle m_dohNewMasterStation; // 0x30
    };
}
