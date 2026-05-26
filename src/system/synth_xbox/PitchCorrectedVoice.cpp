// Decompiled from assembly
#include "PitchCorrectedVoice.h"

void DSP::Synapse::PitchCorrectedVoice::SetAmount(float amount) {
    mAmount = amount;
}

void DSP::Synapse::PitchCorrectedVoice::SetProximityEffect(float effect) {
    mProximityEffect = effect;
}

void DSP::Synapse::PitchCorrectedVoice::SetProximityFocus(float focus) {
    mProximityFocus = focus;
}

void DSP::Synapse::PitchCorrectedVoice::SetAttackSmoothing(float smoothing) {
    mAttackSmoothing = smoothing;
    mAttackCoeff = smoothing;
}

void TrueColor::ExposureRecipe::SetMinIntegrationTime(float time) {
    mMinIntegrationTime = time;
}
