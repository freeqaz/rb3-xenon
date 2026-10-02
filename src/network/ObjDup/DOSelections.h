#pragma once
#include "ObjDup/DOHandle.h"
#include "Platform/RootObject.h"

namespace Quazal {
    // Lane-chosen names: the instance-table singleton (type 4) that keeps the
    // global DO selections; retail calls GetInstance out of line.
    class DOSelections : public RootObject {
    public:
        static DOSelections *GetInstance();
        bool IsAvailable() const;
        bool Contains(DOHandle);
    };
}
