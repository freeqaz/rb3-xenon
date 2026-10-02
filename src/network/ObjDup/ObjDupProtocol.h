#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/LogicalClock.h"
#include "Platform/qStd.h"

// Lane-chosen method names: the retail callees are unnamed.
namespace Quazal {
    class Message;
    class Buffer;
    class ProtocolCallContext;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        Message *CreateDeleteMessage(DOHandle);
        Message *CreateDuplicaMessage();

        static void BuildCreateDuplica(
            ProtocolCallContext *, Message *, DOHandle, DOHandle,
            LogicalClockTmpl<unsigned char>, Buffer *
        );
        static void BuildMigrateDuplica(
            ProtocolCallContext *, Message *, const unsigned short &, DOHandle, DOHandle,
            LogicalClockTmpl<unsigned char>, Buffer *, qList<DOHandle> *
        );
    };
}
