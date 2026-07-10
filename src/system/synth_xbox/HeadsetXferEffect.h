#pragma once

namespace ATG {

// Forward declarations
struct XAPO_REGISTRATION_PROPERTIES;

// Interface for XAPO parameters
class IXAPOParameters {
public:
    virtual ~IXAPOParameters() {}
    // The real ATG CSampleXAPOBase provides a concrete SetParameters; the stub
    // base makes it non-pure so derived XAPOs (HeadsetXferEffect) stay concrete.
    virtual void SetParameters(const void *, unsigned int) {}
};

// Base class for XAPO processing. First vtable at 0x0; instance data spans
// 0x04-0x1F so a subobject vtable placed after it lands at 0x20.
class CXAPOBase {
public:
    CXAPOBase();
    virtual ~CXAPOBase() {}

private:
    unsigned char mCXAPOBaseData[0x1c]; // 0x04-0x1F
};

// Base class with multiple inheritance. CXAPOBase at 0x0 (vtable + 0x1c
// data), IXAPOParameters subobject vtable at 0x20. Matches the real XDK
// CXAPOParametersBase, whose four-arg constructor CSampleXAPOBase calls
// directly (it is CSampleXAPOBase's immediate base, not CXAPOBase's).
class CXAPOParametersBase : public CXAPOBase, public IXAPOParameters {
public:
    CXAPOParametersBase(const void* pRegistrationProperties, void* pParameterBlocks, unsigned int uParameterBlockByteSize, unsigned char fProducer);
    virtual ~CXAPOParametersBase() {}
};

// Template base class for sample XAPOs. Fills 0x24-0x5F so derived effect data
// starts at 0x60 (matches the original 0x864 HeadsetXferEffect object size).
template <typename Derived, typename Params>
class CSampleXAPOBase : public CXAPOParametersBase {
protected:
    CSampleXAPOBase();
    virtual ~CSampleXAPOBase() {}

    static XAPO_REGISTRATION_PROPERTIES m_regProps;

protected:
    unsigned char mSampleBaseData[0x3c]; // 0x24-0x5F
    Params mParams;
};

}  // namespace ATG

// Empty parameter struct for HeadsetXferEffect XAPO (global namespace)
struct HeadsetXferEffectParams {};

// HeadsetXferEffect: Audio processing effect for headset voice transfer (global namespace)
// Layout: Base class data (0x00-0x5F), then effect-specific members
class HeadsetXferEffect : public ATG::CSampleXAPOBase<HeadsetXferEffect, HeadsetXferEffectParams> {
public:
    HeadsetXferEffect();

private:
    // Effect state at offset 0x60
    int mState;                    // 0x60
    // Audio buffer at offset 0x64 (0x800 bytes = 2048 bytes)
    unsigned char mBuffer[0x800];  // 0x64
};
