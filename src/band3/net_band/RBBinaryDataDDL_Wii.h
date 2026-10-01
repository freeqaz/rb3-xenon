#pragma once
#include "Protocol/ProtocolCallContext.h"
#include "network/Protocol/ClientProtocol.h"
#include "Platform/RootObject.h"
#include "Plugins/Buffer.h"

namespace Quazal {

    class Message;

    class String;

    // Retail RTTI .?AV_DDL_RBBinaryBuffer@Quazal@@ (0x8207FA34) and
    // .?AVRBBinaryBuffer@Quazal@@ (0x8207FA9C); 0x18 bytes (ArtFileConverter
    // allocates that). The ctor (0x824F6828) stores the _DDL vtable, builds a
    // 0x400 Buffer at +4, then stores the RBBinaryBuffer vtable. Both vtables
    // share slot 0 (0x824F67C8), which restores only the _DDL vtable before
    // ~Buffer, so RBBinaryBuffer declares no destructor of its own.
    class _DDL_RBBinaryBuffer : public RootObject {
    public:
        _DDL_RBBinaryBuffer() {}
        virtual ~_DDL_RBBinaryBuffer() {}

        Buffer mBuffer; // 0x4
    };

    class RBBinaryBuffer : public _DDL_RBBinaryBuffer {
    public:
        RBBinaryBuffer() {}
    };

    class RBBinaryDataClient : public ClientProtocol {
    public:
        RBBinaryDataClient() : ClientProtocol(1) {}
        virtual ~RBBinaryDataClient() {}
        virtual void ExtractCallSpecificResults(Message *, ProtocolCallContext *);

        int CallSaveBinaryData(ProtocolCallContext *, const String &, const RBBinaryBuffer &, String *, signed char *);
        int CallGetBinaryData(ProtocolCallContext *, const String &, RBBinaryBuffer *, String *, signed char *);
    };
}
