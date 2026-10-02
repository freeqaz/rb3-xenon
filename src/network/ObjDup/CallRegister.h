#pragma once
#include "ObjDup/DOHandle.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class CallRegister : public RootObject {
    public:
        bool MigrationInProgress(DOHandle, DOHandle);

        static CallRegister *GetInstance();
    };
}
