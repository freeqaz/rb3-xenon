#pragma once

namespace ATG {

// Forward declarations
struct XAPO_REGISTRATION_PROPERTIES;

struct EnvelopeGeneratorParams {
    char data[0x10];
};

// Base class for XAPO parameters
class CXAPOParametersBase {
public:
    CXAPOParametersBase(const void* pRegistrationProperties, void* pParameterBlocks, unsigned int uParameterBlockByteSize, unsigned char fProducer);
    virtual ~CXAPOParametersBase() {}
};

// Template base class for sample XAPOs
template <typename Derived, typename Params>
class CSampleXAPOBase : public CXAPOParametersBase {
protected:
    CSampleXAPOBase();
    virtual ~CSampleXAPOBase() {}

    static XAPO_REGISTRATION_PROPERTIES m_regProps;

protected:
    Params mParams;
};

class EnvelopeGenerator : public CSampleXAPOBase<EnvelopeGenerator, EnvelopeGeneratorParams> {
public:
    EnvelopeGenerator();
};

}  // namespace ATG
