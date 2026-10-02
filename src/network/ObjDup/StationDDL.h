#pragma once
#include "RootDO.h"

namespace Quazal {
    class _DO_Station : public RootDO {
    public:
        _DO_Station();
        virtual ~_DO_Station() {}
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;
    };
}