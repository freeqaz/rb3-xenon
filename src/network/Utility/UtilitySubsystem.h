#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class UtilitySubsystem : public RootObject {
    public:
        UtilitySubsystem();
        ~UtilitySubsystem();

        static UtilitySubsystem *_Instance;
    };
}
