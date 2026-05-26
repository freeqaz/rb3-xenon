#pragma once

#include "Platform/RootObject.h"

namespace Quazal {
    class Message;

    class _DDL_ResultRange : public RootObject {
    public:
        _DDL_ResultRange() {}
        virtual ~_DDL_ResultRange() {}

        static void Add(class Message *, const class _DDL_ResultRange &);

        unsigned int m_uiOffset; // 0x4
        unsigned int m_uiSize;   // 0x8
    };
}
