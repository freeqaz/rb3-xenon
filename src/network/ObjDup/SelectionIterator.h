#pragma once
#include "ObjDup/DORef.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class Selection;

    // 0x24 bytes (retail stack frames).
    class SelectionIterator : public RootObject {
    public:
        SelectionIterator(Selection *, bool);
        ~SelectionIterator();

        unsigned int unk0; // 0x0
        DORef m_refCurrent; // 0x4
        unsigned char unk10[0x14]; // 0x10
    };
}
