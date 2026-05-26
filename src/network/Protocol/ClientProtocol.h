#pragma once
#include "Platform/Result.h"
#include "Protocol/Protocol.h"

namespace Quazal {

    class Message;
    class ProtocolCallContext;

    class ClientProtocol : public Protocol {
    public:
        ClientProtocol(unsigned int ui) : Protocol(ui) {}
        virtual ~ClientProtocol() {}
        virtual const char *GetType() const; // 0x14
        virtual bool IsAKindOf(const char *str) const; // 0x18
        virtual void EnforceDeclareSysComponentMacro(); // 0x1C
        virtual int GetProtocolType() const;

        static void SetCallError(qResult);
        int SendRMCMessage(ProtocolCallContext *, Message *);
    };

}