#pragma once
#include "ObjDup/DOHandle.h"
#include "ObjDup/DOFilter.h"
#include "ObjDup/Selection.h"
#include "Platform/RootObject.h"
#include "Platform/SystemError.h"

namespace Quazal {
    // Retail copies the cursor handle out of line (an /O1 COMDAT), so its copy
    // constructor is a non-inline declaration here.
    class SelectionCursor : public RootObject {
    public:
        SelectionCursor(unsigned int ui = 0) : m_uiValue(ui) {}
        SelectionCursor(const SelectionCursor &);

        unsigned int m_uiValue; // 0x0
    };

    inline bool operator==(SelectionCursor a, unsigned int b) { return a.m_uiValue == b; }

    class SelectionPosition : public RootObject {
    public:
        // Retail stores the 0 to its own slot and copy-constructs the cursor
        // straight into a second one (no temporary pointer), the named-local
        // shape below.
        bool EndReached() const {
            unsigned int uiEnd = 0;
            SelectionCursor oCursor(m_oCursor);
            return oCursor.m_uiValue == uiEnd;
        }

        unsigned int unk0; // 0x0
        SelectionCursor m_oCursor; // 0x4
    };

    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(Selection *, bool);
        SelectionIterator(bool, bool);
        ~SelectionIterator();
        void Next(bool);
        void GotoStart();
        void InitFilter();
        unsigned int Count();
        void SetFilter(DOFilter *);
        bool EndReached() const { return m_oPosition.EndReached(); }
        unsigned int GetCurrentHandle() const { return m_oPosition.m_oCursor.m_uiValue; }
        DOHandle operator*() const { return DOHandle(m_oPosition.m_oCursor.m_uiValue); }

        Selection *m_pSelection; // 0x0
        SelectionPosition m_oPosition; // 0x4
        unsigned char unkC[0x10];
        bool m_bFiltered; // 0x1c
        unsigned int unk20; // 0x20
    };

    // The template's members are out of line in retail (instantiated at the
    // end of the TU, 0x82A76800 / 0x82A76860).
    template <class T>
    class SelectionIteratorTemplate : public SelectionIterator {
    public:
        SelectionIteratorTemplate();
        SelectionIteratorTemplate(int iMode);
        // Retail 0x82A76970 (T = RootDO): never expanded, so callers keep its
        // frame.
        void InitFilter() {
            DOFilter *pFilter = new (__FILE__, 0x7b) IsAKindOfDOFilter(T::GetClassID());
            SetFilter(pFilter);
            pFilter->ReleaseRef();
        }
        // JobConnectStation::ConnectionFailed expands this one in place.
        SelectionIteratorTemplate(bool b1, bool b2) : SelectionIterator(b1, b2) {
            SetFilter();
            GotoStart();
        }
        void SetFilter();
        void GotoStart();
        T *GetDOPtr();
        T *operator->() { return GetDOPtr(); }
    };

    // Retail 0x82A79A90 (T = RootDO, in Session's TU): mode 0 and mode 1 pick
    // the base iterator's two flags, then the per-class filter and the first
    // position. Out of class, so /Ob1 never expands it.
    template <class T>
    SelectionIteratorTemplate<T>::SelectionIteratorTemplate(int iMode)
        : SelectionIterator(iMode == 1, iMode == 0) {
        InitFilter();
        GotoStart();
    }
}
