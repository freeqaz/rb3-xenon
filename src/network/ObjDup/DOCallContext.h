#pragma once
#include "Core/CallContext.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    // 0xA8-byte DO call contexts; only what DuplicatedObject touches is declared.
    class DOCallContext : public CallContext {
    public:
        // Callers pass qResult-style codes (0x60001, 0x80010006, ...).
        enum _Outcome {
        };

        DOCallContext(DOHandle, bool);
        virtual ~DOCallContext();

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
