#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;

    // Size 0x60 per asm bss layout.
    class SessionSpace : public RootObject {
    public:
        SessionSpace();
        virtual ~SessionSpace();
        bool PSMatch(DuplicatedObject *, DuplicatedObject *, bool);

    private:
        unsigned char m_pad[0x60 - 4];
    };
}
