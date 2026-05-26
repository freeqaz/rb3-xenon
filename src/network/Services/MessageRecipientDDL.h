#pragma once

#include "Platform/RootObject.h"

namespace Quazal {
    class Message;

    class _DDL_MessageRecipient : public RootObject {
    public:
        _DDL_MessageRecipient() {}
        virtual ~_DDL_MessageRecipient() {}

        static void Add(class Message *, const class _DDL_MessageRecipient &);

        unsigned int m_uiRecipientType; // 0x4
        unsigned int m_uiPrincipalID;   // 0x8
    };
}
