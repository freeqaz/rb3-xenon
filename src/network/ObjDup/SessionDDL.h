#pragma once
#include "RootDO.h"

namespace Quazal {
    class _DO_Session : public RootDO {
    public:
        _DO_Session();
        virtual ~_DO_Session() {}
        virtual bool HasGlobalDOProperty() const;
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;
    };
}