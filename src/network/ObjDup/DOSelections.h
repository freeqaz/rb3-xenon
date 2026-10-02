#pragma once
#include "ObjDup/Selection.h"

namespace Quazal {
    // Member names are lane-chosen; offsets are retail's.
    class DOSelections {
    public:
        static DOSelections *GetInstance();
        void RemoveFromAllSelections(DuplicatedObject *);

        Selection &GetMasters() { return m_selMasters; }
        Selection &GetPublished() { return m_selPublished; }
        Selection &GetDuplicas() { return m_selDuplicas; }

        Selection m_selMasters; // 0x0
        Selection m_selPublished; // 0x24
        Selection m_selDuplicas; // 0x48
    };
}
