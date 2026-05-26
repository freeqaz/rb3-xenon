#pragma once
#include "Protocol/ProtocolCallContext.h"
#include "network/Protocol/ClientProtocol.h"

namespace Quazal {

    class Message;

    class String;
    class RBBinaryBuffer;

    class RBBinaryDataClient : public ClientProtocol {
    public:
        RBBinaryDataClient() : ClientProtocol(1) {}
        virtual ~RBBinaryDataClient() {}
        virtual void ExtractCallSpecificResults(Message *, ProtocolCallContext *);

        int CallSaveBinaryData(ProtocolCallContext *, const String &, const RBBinaryBuffer &, String *, signed char *);
        int CallGetBinaryData(ProtocolCallContext *, const String &, RBBinaryBuffer *, String *, signed char *);
    };
}
