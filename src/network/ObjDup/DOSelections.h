#pragma once
#include "ObjDup/DOHandle.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;
    class Selection;

    // Lane-chosen names: the singleton that keeps the global DO selections;
    // retail calls GetInstance out of line.
    class DOSelections : public RootObject {
    public:
        void RemoveFromAllSelections(DuplicatedObject *);

        static DOSelections *GetInstance();
        bool IsAvailable() const;
        bool Contains(DOHandle);
        // Lane-chosen name for the unnamed retail accessor at 0x82ABD720.
        static Selection *GetDuplicatedObjects();
    };
}
