#pragma once
#include "ObjDup/DOHandle.h"
#include "ObjDup/Selection.h"
#include "Platform/RootObject.h"
#include "Core/InstanceControl.h"
#include "Core/InstanceTable.h"
#include "Core/PseudoSingleton.h"

namespace Quazal {
    class DuplicatedObject;
    class Selection;

    // Lane-chosen names and member names; offsets are retail's.
    class DuplicatedObjectSelections {
    public:
        void AddDO(DuplicatedObject *pDO) { m_selAll.Add(pDO); }
        Selection &GetAll() { return m_selAll; }
        Selection &GetMasters() { return m_selMasters; }
        Selection &GetDuplicas() { return m_selDuplicas; }

        Selection m_selAll; // 0x0
        Selection m_selMasters; // 0x24
        Selection m_selDuplicas; // 0x48
    };

    // Lane-chosen names: the singleton that keeps the global DO selections;
    // retail calls GetInstance out of line.
    class DOSelections : public RootObject {
    public:
        void RemoveFromAllSelections(DuplicatedObject *);

        static DOSelections *GetInstance();
        static DOSelections *GetCurrentInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            return (DOSelections *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        bool IsAvailable() const;
        bool Contains(DOHandle);
        // Lane-chosen name for the unnamed retail accessor at 0x82ABD720: it
        // returns three consecutive Selections (all DOs, masters, duplicas).
        static DuplicatedObjectSelections *GetDuplicatedObjects();
    };
}
