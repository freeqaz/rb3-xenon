#pragma once
#include "Plugins/CompressionAlgorithm.h"
#include "Platform/RootObject.h"
#include "system/zlib/zlib.h"

namespace Quazal {
    struct ZLibStreams : public RootObject {
        z_stream inflate_stream; // 0x0
        z_stream deflate_stream; // 0x38
    };

    class ZLibCompression : public CompressionAlgorithm {
    public:
        ZLibCompression();
        virtual ~ZLibCompression();
        virtual bool CompressImpl(const Buffer &, Buffer *);
        virtual bool DecompressImpl(const Buffer &, Buffer *);

        ZLibStreams *mStreams; // 0xb8
    };
}