#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DOHandle;
    class Message;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        Message *CreateStubMessage(const DOHandle &, unsigned short *);
    };
}
