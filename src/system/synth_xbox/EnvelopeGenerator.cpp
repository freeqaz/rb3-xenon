#include "EnvelopeGenerator.h"

// Forward declarations
extern "C" void XMemSet(void* dst, int val, int size);

namespace ATG {

// Define XAPO_REGISTRATION_PROPERTIES struct
struct XAPO_REGISTRATION_PROPERTIES {
    char data[0x58];  // Size from assembly analysis
};

// Define static member for the template instantiation
template <>
XAPO_REGISTRATION_PROPERTIES CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams>::m_regProps = {};

// Template constructor implementation
template <typename Derived, typename Params>
CSampleXAPOBase<Derived, Params>::CSampleXAPOBase()
    : CXAPOParametersBase(&m_regProps, &mParams, sizeof(Params), 0) {
    XMemSet(&mParams, 0, sizeof(Params));
}

// Explicit instantiation for EnvelopeGenerator
template class CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams>;

// EnvelopeGenerator constructor
EnvelopeGenerator::EnvelopeGenerator() : CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams>() {
}

}  // namespace ATG
