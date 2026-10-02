#pragma once
#include "Platform/RefCountedObject.h"
#include "Platform/Result.h"
#include "Platform/Time.h"
#include "Platform/qStd.h"

namespace Quazal {
    class CallbackRoot;

    class CallContext : public RefCountedObject {
    public:
        enum _State {
            CallInit = 0,
            CallPending = 1,
            CallSuccess = 2,
            CallError = 3,
            CallCancelled = 4,
        };
        CallContext();
        virtual ~CallContext();
        virtual bool FlagsAreValid() const;
        virtual void BeginTransition(_State, qResult, bool);
        virtual void ProcessCallCompletion();

        void RegisterCompletionCallback(CallbackRoot *, bool, bool);
        void SetFlag(unsigned int);
        void ClearFlag(unsigned int);
        void Reset();
        void Trace(unsigned int);
        void SetStateImpl(_State, qResult, bool);
        // Retail 0x82A8C168 (the first function of the CallContext.cpp pin),
        // called with a timeout of -1 before a pending context is torn down.
        bool Wait(unsigned int);
        // Retail 0x82A8B9E0: moves the context to CallPending; false if it
        // cannot start a call. The service clients call it before queueing a job.
        bool InitiateCall();
        // The value the service clients pass as the first argument of the
        // jobs they queue (TicketManager::Login/AcquireTicket).
        unsigned int GetID() const { return unk30; }
        // By value: retail copies the argument into its own temporary before
        // the assignment (TicketManager::AcquireTicket).
        void SetTimeout(Time tTimeout) { unk40 = tTimeout; }

        _State GetState() const { return unkc; }

        // Offsets read off retail's ctor (0x82A8AF30, the base ctor
        // ProtocolCallContext's 0x82A8A4A0 calls): it constructs the qResult
        // at +0x28, zeroes +0x34, +0x38, +0x40 and 8 bytes at +0x48, and
        // MakeSessionJob::IsFinished allocates the object at 0x50 bytes.
        unsigned int unk8; // 0x8
        _State unkc; // 0xc
        qList<int> unk10; // 0x10
        qVector<int> unk18; // 0x18
        int unk24; // 0x24 (not initialised by the ctor)
        qResult unk20; // 0x28
        int unk30; // 0x34
        int unk34; // 0x38
        int unk38; // 0x3c
        int unk3c; // 0x40
        Time unk40; // 0x48
    };
}
