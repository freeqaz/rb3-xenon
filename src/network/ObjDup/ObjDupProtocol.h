#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"
#include "Platform/LogicalClock.h"
#include "Platform/qStd.h"
#include "Platform/Result.h"

// Lane-chosen method names: the retail callees are unnamed.
namespace Quazal {
    class Message;
    class Buffer;
    class ProtocolCallContext;
    class EndPoint;
    class JobJoinSession;

    class ObjDupProtocol : public RootObject {
    public:
        static ObjDupProtocol *GetInstance();
        Message *CreateGetParticipantsRequest();
        Message *CreateJoinRequest();
        qResult Send(EndPoint *, Message *, unsigned int);
        void StopToListen();
        bool ListenOnWellKnown();
        bool StartToListen();
        bool IsListening(unsigned short *) const;
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

        // The join job in progress registers here (retail ObjDupProtocol +0x30).
        void SetJoinSession(JobJoinSession *pJoinSession) { m_pJoinSession = pJoinSession; }

        char m_unk0[0x30];
        JobJoinSession *m_pJoinSession; // 0x30
    };
}
