#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class Selection;

    class DOSelections : public RootObject {
    public:
        // Lane-chosen name for the unnamed retail accessor at 0x82ABD720.
        static Selection *GetDuplicatedObjects();
    };
}
