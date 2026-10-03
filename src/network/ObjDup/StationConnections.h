#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/StationManager.h"

// Lane-chosen names: the retail callees these stand for are unnamed.
namespace Quazal {
    class DOOperation;

    class OperationValidator : public RootObject {
    public:
        static OperationValidator *GetInstance();
        bool Validate(DOOperation *);
    };

    class OperationErrorNotifier : public RootObject {
    public:
        static OperationErrorNotifier *GetInstance();
        void NotifyError(DOHandle, unsigned int);
    };
}
