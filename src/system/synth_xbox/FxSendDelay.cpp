#include "FxSendDelay.h"
#include "FxSend.h"
#include "synth/DelayEffect.h"

// Forward declarations
extern "C" void* memset(void* dst, int val, size_t size);
extern "C" void* memcpy(void* dst, const void* src, size_t size);

namespace ATG {

// XAPO registration properties structure
// Size: 0x430 bytes (highest accessed offset is 0x428)
struct XAPO_REGISTRATION_PROPERTIES {
    char data[0x430];
};

// Copyright string for XAPO registration
// Copied to offset 0x210 with size 0x50 bytes (including null terminator padding)
static const wchar_t kCopyrightString[] = L"Copyright (C) 2008 Microsoft Corp";

// Template base class for XAPO effects
// Note: Multiple variants of this template exist across different namespaces
template <typename Derived, typename Params>
class CSampleXAPOBase {
public:
    static XAPO_REGISTRATION_PROPERTIES m_regProps;
};

// Static member definition for DelayEffect specialization
template <>
XAPO_REGISTRATION_PROPERTIES CSampleXAPOBase<DelayEffect, DelayEffect::Params>::m_regProps;

// Static initializer for XAPO registration properties
// Runs before main() to initialize the m_regProps structure
// Symbol: ??__E?m_regProps@?$CSampleXAPOBase@VDelayEffect@@UParams@1@@ATG@@0UXAPO_REGISTRATION_PROPERTIES@@A@@YAXXZ
static struct StaticInit {
    StaticInit() {
        char* base = (char*)&CSampleXAPOBase<DelayEffect, DelayEffect::Params>::m_regProps;

        // Zero-initialize first range: offset 0x24, size 0x1ec
        memset(base + 0x24, 0, 0x1ec);

        // Copy copyright string: offset 0x210, size 0x50
        memcpy(base + 0x210, kCopyrightString, 0x50);

        // Zero-initialize second range: offset 0x260, size 0x1b0
        memset(base + 0x260, 0, 0x1b0);

        // Initialize XAPO property fields (likely flags and limits)
        *(int*)(base + 0x414) = 0;      // Flags or min channels
        *(int*)(base + 0x418) = 0x3f;   // Max value (63 decimal)
        *(int*)(base + 0x410) = 1;      // Min value or boolean flag
        *(int*)(base + 0x41c) = 1;      // Boolean flag
        *(int*)(base + 0x420) = 1;      // Boolean flag
        *(int*)(base + 0x424) = 1;      // Boolean flag
        *(int*)(base + 0x428) = 1;      // Boolean flag
    }
} s_staticInit;

}  // namespace ATG

FxSendDelay360::FxSendDelay360() : FxSend360(this) {}

FxSendDelay360::~FxSendDelay360() {}

void FxSendDelay360::OnParametersChanged() { FxSend360::SyncEffectParams(); }

void FxSendDelay360::Recreate(std::vector<FxSend *> &sends) { FxSend360::Refresh(sends); }

void FxSendDelay360::UpdateMix() { FxSend360::UpdateVolumes(); }
