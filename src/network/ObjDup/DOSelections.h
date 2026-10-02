#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;

    class DOSelections : public RootObject {
    public:
        void RemoveFromAllSelections(DuplicatedObject *);

        static DOSelections *GetInstance();
    };
}
