#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {
    class Message;

    class _DDL_BasicAccountInfo : public RootObject {
    public:
        _DDL_BasicAccountInfo() {}
        virtual ~_DDL_BasicAccountInfo() {}

        static void Extract(class Message *, class _DDL_BasicAccountInfo *);

        unsigned int m_uiPrincipalID; // 0x4
        String m_strName;             // 0x8
    };
}
