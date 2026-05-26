#pragma once

namespace DSP {
namespace Synapse {

class PitchCorrectedVoice {
public:
    PitchCorrectedVoice();
    ~PitchCorrectedVoice();
    float GetCorrection();
    void SetAmount(float);
    void SetProximityEffect(float);
    void SetProximityFocus(float);
    void SetTransposition(float);
    void SetAttackSmoothing(float);
    void SetReleaseSmoothing(float);

    float mFreq0;              // 0x00
    float mFreq1;              // 0x04
    float mSmoothedCorrection; // 0x08
    float mAttackSmoothing;    // 0x0C
    float mAttackCoeff;        // 0x10
    float mReleaseSmoothing;   // 0x14
    float mTransposition;      // 0x18
    float mAmount;             // 0x1C
    float mProximityEffect;    // 0x20
    float mProximityFocus;     // 0x24
    float mField_0x28;         // 0x28
    float mFreqCounter;        // 0x2C
    float mPrevFreq;           // 0x30
    float mAbsPitchDeviation;  // 0x34
};

} // namespace Synapse
} // namespace DSP

namespace TrueColor {

class ExposureRecipe {
public:
    void SetMinIntegrationTime(float);
    void SetGlobalGain(float);
    float GetLux();

    float mField_0x00;         // 0x00
    float mLux;                // 0x04
    float mField_0x08;         // 0x08
    float mField_0x0C;         // 0x0C
    float mField_0x10;         // 0x10
    float mMinIntegrationTime; // 0x14
};

} // namespace TrueColor
