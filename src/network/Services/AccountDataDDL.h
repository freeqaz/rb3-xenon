#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"
#include "Platform/DateTime.h"

namespace Quazal {
    class Message;

    class _DDL_AccountData : public RootObject {
    public:
        _DDL_AccountData() {}
        virtual ~_DDL_AccountData() {}

        static void Extract(class Message *, class _DDL_AccountData *);

        unsigned int m_uiPrincipalID;   // 0x4
        String m_strName;               // 0x8
        unsigned int m_uiNGSVersion;    // 0xc
        String m_strSomething;          // 0x10
        DateTime m_dtCreated;           // 0x18
        DateTime m_dtUpdated;           // 0x20
        String m_strEmail;              // 0x28
        DateTime m_dtSomeTime;          // 0x30
        String m_strLastString;         // 0x38
    };
}
