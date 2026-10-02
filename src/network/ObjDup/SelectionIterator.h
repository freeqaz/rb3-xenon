#pragma once
#include "ObjDup/DOHandle.h"
#include "ObjDup/Selection.h"
#include "Platform/RootObject.h"

namespace Quazal {
    // Retail copies the cursor handle out of line (an /O1 COMDAT), so its copy
    // constructor is a non-inline declaration here.
    class SelectionCursor : public RootObject {
    public:
        SelectionCursor(unsigned int ui = 0) : m_uiValue(ui) {}
        SelectionCursor(const SelectionCursor &);

        unsigned int m_uiValue; // 0x0
    };

    inline bool operator==(SelectionCursor a, SelectionCursor b) { return a.m_uiValue == b.m_uiValue; }

    class SelectionPosition : public RootObject {
    public:
        bool EndReached() const { return m_oCursor == SelectionCursor(0); }

        unsigned int unk0; // 0x0
        SelectionCursor m_oCursor; // 0x4
    };

    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(Selection *, bool);
        ~SelectionIterator();
        void Next(bool);
        void GotoStart();
        bool EndReached() const { return m_oPosition.EndReached(); }
        DOHandle operator*() const { return DOHandle(m_oPosition.m_oCursor.m_uiValue); }

        Selection *m_pSelection; // 0x0
        SelectionPosition m_oPosition; // 0x4
        unsigned char unkC[0x10];
        bool m_bFiltered; // 0x1c
        unsigned int unk20; // 0x20
    };

    template <class T>
    class SelectionIteratorTemplate : public SelectionIterator {
    public:
        SelectionIteratorTemplate();
        T *operator->();
    };
}
