#pragma once

#include "Platform/RootObject.h"

namespace Quazal {
    class Message;

    class _DDL_DeletionEntry : public RootObject {
    public:
        _DDL_DeletionEntry() {}
        virtual ~_DDL_DeletionEntry() {}

        static void Extract(class Message *, class _DDL_DeletionEntry *);

        unsigned int m_uiID;       // 0x4
        unsigned int m_uiCategory; // 0x8
        unsigned int m_uiReason;   // 0xc
    };
}
