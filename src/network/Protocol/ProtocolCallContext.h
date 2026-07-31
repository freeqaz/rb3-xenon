#pragma once
#include "Core/CallContext.h"
#include "Platform/qStd.h"

namespace Quazal {

    class ProtocolCallContext : public CallContext {
    public:
        ProtocolCallContext();
        virtual ~ProtocolCallContext();
        virtual void BeginTransition(_State, qResult, bool);

        void *GetReturnValuePtr(unsigned int);
        void AddReturnValuePtr(void *);

        qVector<int> unk48;
        int unk50;
        int unk54;
        // Retail sizeof(ProtocolCallContext) is 0x68, 8 bytes larger than the
        // fields captured from the rb3-Wii RE (which end unpadded at 0x5c and
        // round to 0x60). The rb3-Wii header does not carry this field; type
        // and true offset unconfirmed, but an 8-byte tail (forcing 8-byte
        // class alignment via Time's 8-byte member) reproduces retail's
        // `li r3, 0x68` allocation size exactly.
        long long unk58;
    };

}