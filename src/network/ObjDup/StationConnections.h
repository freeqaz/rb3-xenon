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

    // .\StationManager.cpp (0x82AB7EF0..0x82ABAB58).
    class StationManager : public RootObject {
    public:
        static StationManager *GetInstance();
        int ConnectStation(DOHandle);
        JobConnectStation *GetLatestConnectionJob(DOHandle) const;
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
