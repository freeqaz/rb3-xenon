#pragma once
#include "Core/CallContext.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    // 0xA8-byte DO call contexts; only what DuplicatedObject touches is declared.
    class DOCallContext : public CallContext {
    public:
        // Callers pass qResult-style codes (0x60001, 0x80010006, ...).
        enum _Outcome {
            CallCancelled = 0x80060003
        };

        DOCallContext(DOHandle, bool);
        virtual ~DOCallContext();
        // Retail 0x82AA1410 (JobConnectStation::ConnectionFailed passes 4).
        bool Cancel(unsigned int);

        unsigned char unk50[0x58]; // 0x50
    };

    class MigrationContext : public DOCallContext {
    public:
        MigrationContext(bool);
        bool MigrateObject(DOHandle, DOHandle);
    };

    class FetchContext : public DOCallContext {
    public:
        FetchContext(DOHandle, bool);
        void SetOrphanRecovery();
        bool ConnectOrphan(DOHandle);
    };
}
