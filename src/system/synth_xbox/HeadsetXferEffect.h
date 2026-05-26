#pragma once

namespace ATG {

// Forward declarations
struct XAPO_REGISTRATION_PROPERTIES;

// Empty parameter struct for HeadsetXferEffect XAPO
struct HeadsetXferEffectParams {};

// Interface for XAPO parameters
class IXAPOParameters {
public:
    virtual ~IXAPOParameters() {}
    virtual void SetParameters(const void *, unsigned int) = 0;
};

// Base class for XAPO parameters
class CXAPOParametersBase {
public:
    CXAPOParametersBase(const void* pRegistrationProperties, void* pParameterBlocks, unsigned int uParameterBlockByteSize, unsigned char fProducer);
    virtual ~CXAPOParametersBase() {}
};

// Base class with multiple inheritance
class CXAPOBase : public CXAPOParametersBase, public IXAPOParameters {
public:
    CXAPOBase();
    virtual ~CXAPOBase() {}
};

// Template base class for sample XAPOs
template <typename Derived, typename Params>
class CSampleXAPOBase : public CXAPOBase {
protected:
    CSampleXAPOBase();
    virtual ~CSampleXAPOBase() {}

    static XAPO_REGISTRATION_PROPERTIES m_regProps;

protected:
    Params mParams;
};

// HeadsetXferEffect: Audio processing effect for headset voice transfer
// Layout: Base class data (0x00-0x5F), then effect-specific members
class HeadsetXferEffect : public CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams> {
public:
    HeadsetXferEffect();

private:
    // Effect state at offset 0x60
    int mState;                    // 0x60
    // Audio buffer at offset 0x64 (0x800 bytes = 2048 bytes)
    unsigned char mBuffer[0x800];  // 0x64
};

}  // namespace ATG
