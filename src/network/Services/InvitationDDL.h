#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {
    class Message;

    class _DDL_Invitation : public RootObject {
    public:
        _DDL_Invitation() {}
        virtual ~_DDL_Invitation() {}

        static void Extract(class Message *, class _DDL_Invitation *);

        unsigned int m_uiRecipientID; // 0x4
        unsigned int m_uiSenderID;    // 0x8
        String m_strMessage;          // 0xc
    };
}
