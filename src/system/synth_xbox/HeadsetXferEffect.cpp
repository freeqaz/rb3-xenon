#include "HeadsetXferEffect.h"
#include "xdk/LIBCMT/string.h"

namespace ATG {

// XAPO registration properties structure (0x58 bytes)
// Contains metadata for Xbox Audio Processing Object registration
struct XAPO_REGISTRATION_PROPERTIES {
    char data[0x58];  // Opaque structure from XAudio2 SDK
};

// Static registration properties for HeadsetXferEffect XAPO
template <>
XAPO_REGISTRATION_PROPERTIES CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams>::m_regProps = {};

// Base template constructor - delegates to CXAPOParametersBase, wiring up
// the static registration properties and the derived Params block as the
// XAPO parameter storage (matches ATG::CSampleXAPOBase in the real XDK).
template <typename Derived, typename Params>
CSampleXAPOBase<Derived, Params>::CSampleXAPOBase()
    : CXAPOParametersBase(&m_regProps, &mParams, sizeof(Params), 0) {
}

// Explicit template instantiation for HeadsetXferEffect
template class CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams>;

}  // namespace ATG

// HeadsetXferEffect constructor (global namespace)
// Initializes effect state and audio buffer, then configures parameters
HeadsetXferEffect::HeadsetXferEffect() : ATG::CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams>() {
    // Initialize effect state
    mState = 0;

    // Clear audio buffer
    memset(mBuffer, 0, sizeof(mBuffer));

    // Configure initial parameters through IXAPOParameters interface (at offset 0x20)
    int initialParam = 0;
    ((ATG::IXAPOParameters*)((char*)this + 0x20))->SetParameters(&initialParam, sizeof(initialParam));
}
