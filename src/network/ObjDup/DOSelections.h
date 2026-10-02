#pragma once
#include "ObjDup/DOHandle.h"
#include "Platform/RootObject.h"
#include "Core/InstanceControl.h"
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    // Lane-chosen names: the instance-table singleton (type 4) that keeps the
    // global DO selections; retail calls GetInstance out of line.
    class DOSelections : public RootObject {
    public:
        static DOSelections *GetInstance();
        static DOSelections *GetCurrentInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (DOSelections *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        bool IsAvailable() const;
        bool Contains(DOHandle);
    };
}
