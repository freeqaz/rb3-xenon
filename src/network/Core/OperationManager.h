#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DOOperation;

    class OperationManager : public RootObject {
    public:
        DOOperation *GetCurrentOperation() const;
    };
}
