#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

// Lane-chosen method names: the retail callees are unnamed.
namespace Quazal {
    class Message;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        Message *CreateStubMessage(const DOHandle &, unsigned short *);
        Message *CreateDeleteMessage(DOHandle);
    };
}
