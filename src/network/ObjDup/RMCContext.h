#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class Message;

    class ActiveDOCallContext : public RootObject {
    public:
        Message *GetCallMessage();
        bool PerformCallAndWait();
    };

    // The context of one outgoing RMC call on a DO (0xe0 bytes).
    class RMCContext : public ActiveDOCallContext {
    public:
        RMCContext(DOHandle, bool);
        ~RMCContext();
        void ClearFlag(unsigned int);
        void SetFlag(unsigned int);
        bool PrepareCallMessage(DOHandle, unsigned short);
        Message *GetResponseMessage();

        unsigned short GetMethodID() const { return m_usMethodID; }

        unsigned char m_pad[0xa0];
        unsigned short m_usMethodID; // 0xa0
        unsigned char m_padA2[0x3e];
    };
}
