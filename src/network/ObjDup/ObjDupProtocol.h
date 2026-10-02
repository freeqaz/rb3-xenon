#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"
#include "Platform/LogicalClock.h"
#include "Platform/qStd.h"

// Lane-chosen method names: the retail callees are unnamed.
namespace Quazal {
    class Message;
    class Buffer;
    class ProtocolCallContext;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        Message *CreateActionMessage(DOHandle *, unsigned short *);
        Message *CreateDeleteMessage(DOHandle);
        Message *CreateDOProtocolMessage();

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
