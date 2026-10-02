#pragma once
#include "ObjDup/DORef.h"
#include "Platform/RootObject.h"
#include "Platform/SystemError.h"

namespace Quazal {
    class Selection;

    // 0x24 bytes (retail stack frames).
    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(Selection *, bool);
        SelectionIterator(bool, bool);
        ~SelectionIterator();

        void GotoNext(bool);
        void GotoStart();
        void InitFilter();

        unsigned int unk0; // 0x0
        DORef m_refCurrent; // 0x4
        unsigned char unk10[0x14]; // 0x10
    };

    template <class T>
    class SelectionIteratorTemplate : public SelectionIterator {
    public:
        SelectionIteratorTemplate(int iMode) : SelectionIterator(iMode == 1, iMode == 0) {
            InitFilter();
            GotoStart();
        }
        ~SelectionIteratorTemplate() {}

        bool EndReached() const { return m_refCurrent.GetHandle() == DOHandle(); }
        T *operator->() {
            if (m_refCurrent.GetDOPtr() == 0) {
                SystemError::SignalError(0, 0, 0xA0000007, 0);
                return 0;
            }
            return (T *)m_refCurrent.m_poReferencedDO;
        }
    };
}
