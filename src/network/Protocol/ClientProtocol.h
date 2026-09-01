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
        /** Slot 22, and `_purecall` in retail's own `??_7ClientProtocol@Quazal@@6B@`
            (`0x8207f944[22] == 0x828299b8`).  Retail declares this here, pure; the
            three RTTI-visible subclasses (`RBDataClient`, `RBBinaryDataClient`,
            `RBTestClient`) each carry a DISTINCT real body at that same slot
            (`0x8250a510` / `0x8250aaa8` / `0x8250af08`).  We used to declare it only
            on the subclasses, which left this base one slot short of retail. */
        virtual void ExtractCallSpecificResults(Message *, ProtocolCallContext *) = 0;

        static void SetCallError(qResult);
        int SendRMCMessage(ProtocolCallContext *, Message *);
    };

}