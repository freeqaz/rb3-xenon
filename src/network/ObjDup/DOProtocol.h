#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class Buffer;

    class DOProtocol : public RootObject {
    public:
        DOProtocol();
        ~DOProtocol();

        bool DecodeBuffer(Buffer *) const;
        void EncodeBuffer(Buffer *) const;
    };
}
