#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {
    class Message;

    class _DDL_FriendData : public RootObject {
    public:
        _DDL_FriendData() {}
        virtual ~_DDL_FriendData() {}

        static void Extract(class Message *, class _DDL_FriendData *);

        unsigned int m_uiPrincipalID;  // 0x4
        String m_strName;              // 0x8
        unsigned char m_byStatus;      // 0xc (1 byte)
        unsigned int m_uiUpdateTime;   // 0x10
        String m_strComment;           // 0x14
    };
}
