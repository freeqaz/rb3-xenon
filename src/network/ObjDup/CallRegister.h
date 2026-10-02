#pragma once
#include "ObjDup/DOCallContext.h"
#include "ObjDup/DOHandle.h"
#include "Platform/RootObject.h"

namespace Quazal {
    // Retail's CallRegister (0x28 bytes) derives from
    // ItemRegister<DOCallContext>; its methods are in the CallRegister TU.
    // Only the accessor and the queries other TUs call are declared here.
    class CallRegister : public RootObject {
    public:
        static CallRegister &GetInstanceRef();
        void SignalRelevantFetchContextes(DOHandle, DOCallContext::_Outcome);
        bool MigrationInProgress(DOHandle, DOHandle);
    };
}
