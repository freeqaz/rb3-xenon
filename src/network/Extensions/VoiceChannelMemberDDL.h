#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class Message;

    class _DDL_VoiceChannelMember : public RootObject {
    public:
        _DDL_VoiceChannelMember() {}
        virtual ~_DDL_VoiceChannelMember() {}

        static void Add(class Message *, const class _DDL_VoiceChannelMember &);
        static void Extract(class Message *, class _DDL_VoiceChannelMember *);

        DOHandle m_hChannel; // 0x4
        DOHandle m_hMember;  // 0x8
    };
}
