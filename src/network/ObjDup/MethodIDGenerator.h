#pragma once
#include "Platform/String.h"

namespace Quazal {
    // Maps an RMC method name to the id its DO class dispatches on. The name
    // is taken by value (the callee destroys it).
    class MethodIDGenerator {
    public:
        static unsigned short AssignID(String);
        static unsigned short GetID(String);
    };
}
