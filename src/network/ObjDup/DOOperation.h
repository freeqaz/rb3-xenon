#pragma once
#include "Core/Operation.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DOID.h"
#include "ObjDup/DORef.h"
#include "ObjDup/MasterStationRef.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
        virtual ~DOOperation();

        DORef m_refTargetObject; // 0x14
    };

    class RemoveFromStoreOperation : public DOOperation {
    public:
        bool unk20; // 0x20
        bool unk21; // 0x21
    };

    class AddToStoreOperation : public DOOperation {
    public:
        bool IsMaster() const { return m_bMaster; }
        bool IsDuplica() const { return !m_bMaster; }

        bool m_bMaster; // 0x20
        Message *m_pMessage; // 0x24
    };

    class CallMethodOperation : public DOOperation {};
    class UpdateDatasetsOperation : public DOOperation {};
    class ChangeDupSetOperation : public DOOperation {};

    class ChangeMasterStationOperation : public DOOperation {
    public:
        enum Context {
        };

        Context GetContext() const { return m_eContext; }
        DOHandle GetOldMasterStation() const { return m_dohOldMasterStation; }
        DOHandle GetNewMasterStation() const { return m_dohNewMasterStation; }

        int unk20; // 0x20
        DOHandle m_dohOldMasterStation; // 0x24
        int unk28[2]; // 0x28
        DOHandle m_dohNewMasterStation; // 0x30
        int unk34[3]; // 0x34
        Context m_eContext; // 0x40
    };

    class CreateMasterOperation : public DOOperation {
    public:
        MasterStationRef m_refMasterStation; // 0x20
        DOID GetDOID() const { return m_oDOID; }

        DOID m_oDOID; // 0x30
    };
}
