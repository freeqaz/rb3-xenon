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

        // Retail's ctor (0x82A8A4A0) builds its own members from +0x50 and
        // stores +0x60 and +0x64; sizeof is 0x68 (`li r3, 0x68` before
        // ContextWrapper::SetCallbackObject's `new`).
        qVector<int> unk48; // 0x50
        int unk50; // 0x5c
        int unk54; // 0x60
        int unk58; // 0x64
    };

}