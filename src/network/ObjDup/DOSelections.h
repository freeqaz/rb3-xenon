#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;
    class Selection;

    class DOSelections : public RootObject {
    public:
        void RemoveFromAllSelections(DuplicatedObject *);

        static DOSelections *GetInstance();
        // Lane-chosen name for the unnamed retail accessor at 0x82ABD720.
        static Selection *GetDuplicatedObjects();
    };
}
