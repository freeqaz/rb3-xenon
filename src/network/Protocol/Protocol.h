#pragma once
#include "network/Core/SystemComponent.h"

namespace Quazal {

    class EndPoint;
    class Message;
    class ProtocolCallContext;

    class Protocol : public SystemComponent {
    public:
        enum _Type {
            T0 = 0,
            T1 = 1
        };

        Protocol(unsigned int);
        virtual ~Protocol(); // 0x8
        virtual const char *GetType() const; // 0x14
        virtual bool IsAKindOf(const char *str) const; // 0x18
        virtual void EnforceDeclareSysComponentMacro(); // 0x1C
        virtual void TraceImpl(uint) const; // 0x20
        virtual bool BeginInitialization(); // 0x30
        virtual int GetProtocolType() const = 0; // fix ret type
        virtual void EndPointDisconnected(EndPoint *);
        virtual void FaultDetected(EndPoint *, unsigned int);
        virtual int Clone() const;

        bool FlagIsSet(unsigned int) const;
        static unsigned int ExtractMethodID(Message *);
        static void AddMethodID(Message *, unsigned int);
        static int RegisterCallContext(Message *, ProtocolCallContext *);

        bool unk18;
        int unk1c;
        int unk20;
        unsigned int unk24;
        int unk28;
        bool unk2c;
        int unk30;
        int unk34;
    };

    class ProtocolRequestBroker {
    public:
        static void InitMessage(Message *, unsigned char, Protocol::_Type);
    };
}
