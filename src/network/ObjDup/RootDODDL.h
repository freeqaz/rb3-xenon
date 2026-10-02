#pragma once

#include "DuplicatedObject.h"
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
        static unsigned int GetClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;
    };
}