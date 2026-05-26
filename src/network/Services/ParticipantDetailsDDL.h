#pragma once

#include "Platform/RootObject.h"
#include "Platform/String.h"

namespace Quazal {
    class Message;

    class _DDL_ParticipantDetails : public RootObject {
    public:
        _DDL_ParticipantDetails() {}
        virtual ~_DDL_ParticipantDetails() {}

        static void Extract(class Message *, class _DDL_ParticipantDetails *);

        unsigned int m_uiPrincipalID; // 0x4
        String m_strName;             // 0x8
        String m_strMessage;          // 0xc
    };
}
