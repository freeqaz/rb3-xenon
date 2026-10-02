#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

// Lane-chosen names: the retail callees these stand for are unnamed.
namespace Quazal {
    class DOOperation;

    class JobConnectStation : public RootObject {
    public:
        void QueueOperation(DOOperation *);
    };

    class StationConnections : public RootObject {
    public:
        static StationConnections *GetInstance();
        int GetConnectionState(DOHandle);
        JobConnectStation *GetConnectionJob(DOHandle);
    };

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
