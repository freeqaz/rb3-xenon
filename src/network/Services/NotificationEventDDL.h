#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {
    class Message;

    class _DDL_NotificationEvent : public RootObject {
    public:
        _DDL_NotificationEvent() {}
        virtual ~_DDL_NotificationEvent() {}

        static void Extract(class Message *, class _DDL_NotificationEvent *);

        unsigned int m_uiPIDSource;       // 0x4
        unsigned int m_uiType;            // 0x8
        unsigned int m_uiParam1;          // 0xc
        unsigned int m_uiParam2;          // 0x10
        String m_strParam;                // 0x14
    };
}
