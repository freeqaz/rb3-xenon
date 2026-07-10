// Decompiled from assembly
#include "PitchCorrectedVoice.h"
#include <math.h>

// Synapse-internal natural-log helper (defined in another TU; referenced as
// ??$Log@M@Util@@YAMABM@Z).
namespace Util {
template <class T> T Log(const T &);
}

DSP::Synapse::PitchCorrectedVoice::PitchCorrectedVoice()
    : mFreq0(0.0f), mFreq1(0.0f), mSmoothedCorrection(0.0f), mAttackSmoothing(0.0f),
      mAttackCoeff(0.0f), mReleaseSmoothing(0.0f), mTransposition(0.0f), mAmount(1.0f),
      mProximityEffect(0.0f), mProximityFocus(0.5f), mField_0x28(0.0f), mFreqCounter(0.0f),
      mPrevFreq(0.0f), mAbsPitchDeviation(0.0f) {}

float DSP::Synapse::PitchCorrectedVoice::GetCorrection() {
    // Interval between the two target frequencies, in semitones.
    float interval = Util::Log<float>(mFreq1) - Util::Log<float>(mFreq0);
    float amount = mAmount;
    float semitones = interval * 17.312339782714844f;

    // Wrap the interval into (-6, 6] semitones (octave-fold), at most 3 steps.
    int wrap = 3;
    while (semitones > 6.0f) {
        if (wrap <= 0) break;
        semitones -= 12.0f;
        wrap -= 1;
    }
    wrap = 3;
    while (semitones < -6.0f) {
        if (wrap <= 0) break;
        semitones += 12.0f;
        wrap -= 1;
    }

    // Back to a log-frequency-ratio deviation.
    float deviation = semitones * 0.0577622652053833f;
    float absDev = fabsf(deviation);
    mAbsPitchDeviation = absDev;

    if (mProximityEffect != 0.0f) {
        // Proximity window half-width (ln-ratio units).
        float halfWidth = (12.0f - mProximityEffect * 11.5f) * 0.0577622652053833f;
        if (absDev < halfWidth) {
            float norm = absDev / halfWidth;
            float k = mProximityFocus * 2.0f - 1.0f;
            k = (k * 2.0f) / (1.0f - k);
            float shaped = (k + 1.0f) * norm / (fabsf(norm) * k + 1.0f);
            float win = (float)cos((double)(shaped * 3.1415927410125732f));
            float scale = (win + 1.0f) * 0.5f;
            amount = scale * amount;
        } else {
            amount = 0.0f;
        }
    }

    // Pick attack vs release coefficient by whether the deviation continues
    // in the same direction as the current smoothed correction.
    float coeff = mReleaseSmoothing;
    if (mSmoothedCorrection * deviation < 0.0f) {
        coeff = mAttackCoeff;
    }

    // Suppress correction until the detector has been confident for a while.
    if (mFreqCounter < 20.0f) {
        amount = 0.0f;
    }

    // Penalize sudden jumps in the confidence counter.
    float ratio = mFreqCounter / (mPrevFreq + 1.0f);
    if (ratio > 1.1) {
        float fade = (2.0f - ratio) * 2.0f;
        if (fade < 0.0f) fade = 0.0f;
        amount = fade * amount;
    }
    mPrevFreq = mFreqCounter;

    // Smooth toward the target correction.
    mSmoothedCorrection += (amount * deviation - mSmoothedCorrection) * coeff;
    float mag = mSmoothedCorrection;
    if (mag < 0.0f) {
        mag = -mag;
    }
    if (mag < 9.999999974752427e-07f) {
        mSmoothedCorrection = 0.0f;
    }

    return (float)exp((double)(mTransposition + mSmoothedCorrection));
}

void DSP::Synapse::PitchCorrectedVoice::SetTransposition(float value) {
    float two = 2.0f;
    mTransposition = Util::Log<float>(two) * value * 0.0833333358168602f;
}

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
