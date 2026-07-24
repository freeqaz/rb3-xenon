// Faithful port from the rb3-Wii oracle (../rb3/src/system/synth/VoiceBeat.cpp).
// Contains VoiceBeat (the talky/spam-syllable DSP), EventTracker (reference-event
// hit/miss bookkeeping), and TalkyMatcher (the per-frame unpitched-note matcher
// Singer drives via ProcessTalkyData). X360-inert: not in objects.json, header
// unchanged, so it cannot perturb retail preprocessed output. The single deviation
// from the oracle is the profiling-only START_AUTO_TIMER, gated out under HX_NATIVE
// (a no-op that never touches scoring state) to keep the native link surface tight.
#include "synth/VoiceBeat.h"
#include "math/Utl.h"
#ifndef HX_NATIVE
#include "os/Timer.h"
#endif
#include <algorithm>
#include <string.h>
#include <math.h>

VoiceBeat::VoiceBeat() {
    mEnabled = true;
    Reset();
}

void VoiceBeat::SetEnable(bool enable) {
    if (enable && !mEnabled)
        Reset();
    mEnabled = enable;
}

void VoiceBeat::Analyze(
    float *samples, int numSamples, bool useWindow, bool storeEvents, float ms
) {
#ifndef HX_NATIVE
    START_AUTO_TIMER("voice_beat");
#endif
    if (!mEnabled) return;

    if (ms != -1.0f) {
        float mCountScaled = (float)mCount * 0.0625f;
        mRate = ((double)numSamples
                 + (ms - ((float)(numSamples / 16) + mCountScaled)) / 5.0)
            / (double)numSamples;
    }

    double k_thrEnergy = 0.3;
    double k_c0 = 1.178584698;
    double k_floorRise = 0.001;
    double k_c5 = 3.6717290892;
    double k_c4 = -5.0679983867;
    double k_b0 = 143.5132541;
    double k_a0 = 2666.171709;
    double k_c1 = 6.0;
    double k_c2 = -0.7199103273;
    double k_b2 = 1.7041197124;
    double k_thrSpam = 0.35;
    double k_envFast = 0.03;
    double k_a2 = 1.9444776578;
    double k_a1 = -0.9459779362;
    double k_msPerSample = 0.0625f;
    double k_envSyl = 0.08;

    for (int i = 0.0f; i < numSamples; i++) {
        if (useWindow) {
            *samples *= (float)sin(3.1415927410125732 * ((double)i / (double)numSamples));
        }

        double oldXV1 = mXVVoice[1];
        double oldXV3 = mXVVoice[3];
        mXVVoice[0] = oldXV1;
        mXVVoice[1] = mXVVoice[2];
        double oldYV1 = mYVVoice[1];
        mXVVoice[2] = oldXV3;
        double oldYV2 = mYVVoice[2];
        mXVVoice[3] = mXVVoice[4];
        double oldYV3 = mYVVoice[3];
        double oldYV4 = mYVVoice[4];
        double newXV = (double)*samples / 6.349260768;
        mYVVoice[0] = oldYV1;
        double oldVE = mVoiceEnergy;
        mYVVoice[3] = oldYV4;
        double oldFBE = mFullBandEnergy;
        mYVVoice[1] = oldYV2;
        mXVVoice[4] = newXV;
        mYVVoice[2] = oldYV3;
        double oldAA_X1 = mXVEnvAntiAlias[1];
        double oldAA_X2 = mXVEnvAntiAlias[2];
        double oldAA_Y1 = mYVEnvAntiAlias[1];
        double oldAA_Y2 = mYVEnvAntiAlias[2];
        double newYV = 2.4013168963 * oldYV4
            + (-2.0287939898 * oldYV3
               + (0.7561945957 * oldYV2
                  + (-0.1330748863 * oldYV1 + (2.0 * oldXV3 - (oldXV1 + newXV)))));
        newYV = -newYV;
        mYVVoice[4] = newYV;

        double absYV = fabs(newYV);
        mCount += mRate;
        float absSample = fabs(*samples);
        double newVoiceEnergy = 0.02 * (absYV - oldVE) + oldVE;
        double newFullBandEnergy
            = 0.02 * ((double)absSample - oldFBE) + oldFBE;
        double envInput = absYV / k_a0;
        mXVEnvAntiAlias[0] = oldAA_X1;
        mXVEnvAntiAlias[1] = oldAA_X2;
        mXVEnvAntiAlias[2] = envInput;
        mYVEnvAntiAlias[0] = oldAA_Y1;
        mYVEnvAntiAlias[1] = oldAA_Y2;
        double aaAcc = 2.0 * oldAA_X2 + (oldAA_X1 + envInput);
        double newAA_Y2 = k_a2 * oldAA_Y2 + (k_a1 * oldAA_Y1 + aaAcc);
        mVoiceEnergy = newVoiceEnergy;
        mFullBandEnergy = newFullBandEnergy;
        double ratio = newVoiceEnergy / newFullBandEnergy;
        mYVEnvAntiAlias[2] = newAA_Y2;

        if ((i % 40) == 0) {
            double oldSylX1 = mXVSyllables[1];
            double sylInput = newAA_Y2 / k_b0;
            double oldSpamX1 = mXVSpamSyllables[1];
            double oldSpamX2 = mXVSpamSyllables[2];
            double oldSpamX4 = mXVSpamSyllables[4];
            double oldSpamX3 = mXVSpamSyllables[3];
            double spamInput = newAA_Y2 / k_c0;
            double oldSylX2 = mXVSyllables[2];
            double oldSylY1 = mYVSyllables[1];
            double oldSylY2 = mYVSyllables[2];
            double oldSpamY1 = mYVSpamSyllables[1];
            double oldSpamY2 = mYVSpamSyllables[2];
            double sylAcc0 = oldSylX1 + sylInput;
            mXVSyllables[0] = oldSylX1;
            double oldSpamY3 = mYVSpamSyllables[3];
            double spamSum24 = oldSpamX2 + oldSpamX4;
            double spamAcc0 = oldSpamX1 + spamInput;
            mXVSpamSyllables[0] = oldSpamX1;
            double sylAcc1 = 2.0 * oldSylX2 + sylAcc0;
            double oldSpamY4 = mYVSpamSyllables[4];
            double spamAcc1 = -(4.0 * spamSum24 - spamAcc0);
            mXVSyllables[1] = oldSylX2;
            double oldSylEnvSigma = mSylEnvSigma;
            double sylAcc2 = (-0.7319917025) * oldSylY1 + sylAcc1;
            double k_c1_l = k_c1;
            mYVSyllables[1] = oldSylY2;
            double spamAcc2 = k_c1_l * oldSpamX3 + spamAcc1;
            double k_b2_l = k_b2;
            double k_envSyl_l = k_envSyl;
            double newSylY2 = k_b2_l * oldSylY2 + sylAcc2;
            double k_c2_l = k_c2;
            double oldFloorSigma = mFloorSigma;
            double spamAcc3 = k_c2_l * oldSpamY1 + spamAcc2;
            double k_c3_l = 3.1159669252;
            mXVSpamSyllables[1] = oldSpamX2;
            double sylDelta = newSylY2 - oldSylEnvSigma;
            mXVSpamSyllables[3] = oldSpamX4;
            float sylY2_f = (float)newSylY2;
            double spamAcc4 = k_c3_l * oldSpamY2 + spamAcc3;
            double oldSpamAvg = mSpamAvg;
            mYVSpamSyllables[1] = oldSpamY2;
            double newSylEnvSigma = k_envSyl_l * sylDelta + oldSylEnvSigma;
            double k_c4_l = k_c4;
            mXVSyllables[2] = sylInput;
            double spamAcc5 = k_c4_l * oldSpamY3 + spamAcc4;
            double k_c5_l = k_c5;
            mYVSyllables[0] = oldSylY1;
            mYVSyllables[2] = newSylY2;
            double newSpamY4 = k_c5_l * oldSpamY4 + spamAcc5;
            mXVSpamSyllables[2] = oldSpamX3;
            mXVSpamSyllables[4] = spamInput;
            double absSpam = fabs(newSpamY4);
            mYVSpamSyllables[0] = oldSpamY1;
            double spamDelta = absSpam - oldSpamAvg;
            mYVSpamSyllables[2] = oldSpamY3;
            double newSpamAvg = k_envFast * spamDelta + oldSpamAvg;
            mYVSpamSyllables[3] = oldSpamY4;
            mYVSpamSyllables[4] = newSpamY4;
            mSpamAvg = newSpamAvg;
            unk4 = sylY2_f;
            mSylEnvSigma = newSylEnvSigma;

            if (newSylY2 < oldFloorSigma) {
                mFloorSigma = newSylY2;
            } else {
                mFloorSigma = k_floorRise * (newSylY2 - oldFloorSigma) + oldFloorSigma;
            }

            ms = (float)mCount * (float)k_msPerSample;
            unk0 = newSpamAvg > k_thrSpam;
            unk1 = ratio > k_thrEnergy;

            if (sylDelta < 0.0 && mSylDeltaPrev >= 0.0) {
                static double k_thrFloor = 0.15;
                double *floorPtr
                    = (k_thrFloor >= mFloorSigma) ? &k_thrFloor : &mFloorSigma;
                if (newSylY2 > 4.0 * *floorPtr && unk1 && unk0) {
                    if (storeEvents) {
                        mPeaks.push_back((float)newSylY2);
                        mTimes.push_back(ms);
                    }
                    mTriggered = true;
                }
            }
            mSylDeltaPrev = sylDelta;
        }
        samples++;
    }
}

void VoiceBeat::Reset() {
    memset(mXVVoice, 0, sizeof(mXVVoice));
    memset(mYVVoice, 0, sizeof(mYVVoice));
    memset(mXVEnvAntiAlias, 0, sizeof(mXVEnvAntiAlias));
    memset(mYVEnvAntiAlias, 0, sizeof(mYVEnvAntiAlias));
    memset(mXVSyllables, 0, sizeof(mXVSyllables));
    memset(mYVSyllables, 0, sizeof(mYVSyllables));
    memset(mXVSpamSyllables, 0, sizeof(mXVSpamSyllables));
    memset(mYVSpamSyllables, 0, sizeof(mYVSpamSyllables));
    unk0 = false;
    unk1 = false;
    unk4 = 0;
    mSpamAvg = 0;
    mSylDeltaPrev = 0;
    mSylEnvSigma = 0;
    mFloorSigma = 0;
    mCount = 0;
    mRate = 1;
    mPeaks.clear();
    mTimes.clear();
    mTriggered = false;
}

void VoiceBeat::ClearTrigger() { mTriggered = false; }

void VoiceBeat::ClearEventList() {
    mPeaks.clear();
    mTimes.clear();
}

EventTracker::EventTracker() : mSelFrom(-1), mSelTo(-1), mAvgHitTime(0) {}

void EventTracker::invalidate() {
    mSelFrom = -1;
    mSelTo = -1;
}

int EventTracker::findEarliest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int last = n - 1;
    MaxEq(start, 0);
    if (start > last) start = last;
    while (start >= 0 && mTimes[start] >= t) {
        start--;
    }
    if (start < 0) return 0;
    while (start < n && mTimes[start] < t) {
        start++;
    }
    return start;
}

int EventTracker::findLatest(float t, int start) {
    int n = mTimes.size();
    if (n == 0) return -1;
    int idx = start;
    if (idx > n) idx = n - 1;
    if (idx < 0) idx = 0;
    while (idx < n && mTimes[idx] < t) {
        idx++;
    }
    if (idx >= n) return n - 1;
    while (idx >= 0 && mTimes[idx] >= t) {
        idx--;
    }
    return idx;
}

void EventTracker::Reset() {
    mMisses.clear();
    mMisses.resize(mTimes.size(), false);
    mHits.clear();
    mHits.resize(mTimes.size(), false);
    mSwings.clear();
    mSwings.resize(mTimes.size(), 0);
    mAvgHitTime = 0;
    invalidate();
}

bool EventTracker::Hit(float msFrom, float msUpTo, float msNow) {
    mSelFrom = findEarliest(msFrom, mSelFrom);
    mSelTo = findLatest(msUpTo, mSelTo);
    float tAccum = 0.0f;
    for (int i = mSelFrom; i <= mSelTo; i++) {
        static float k_zero = 0.0f;
        float diff = 0.2f - mPeaks[i];
        float *p = (k_zero >= diff) ? &k_zero : &diff;
        float tolHalf = 1000.0f * (*p) + 60.0f;
        if (mTimes[i] - tolHalf <= msNow && msNow <= mTimes[i] + tolHalf) {
            tAccum += mTimes[i];
            mHits[i] = true;
        }
    }
    mSelFrom = findEarliest(msNow - 150.0f, mSelFrom);
    mSelTo = findLatest(150.0f + msNow, mSelTo);
    for (int i = mSelFrom; i <= mSelTo; i++) {
        mSwings[i]++;
    }
    if (tAccum != 0.0f) {
        int n = mSelFrom - mSelTo + 1;
        mAvgHitTime = 0.1f * (tAccum / (float)n - (msFrom + msUpTo) * 0.5f - mAvgHitTime)
            + mAvgHitTime;
    }
    return 0.0f != tAccum;
}

bool EventTracker::Miss(float msFrom, float msUpTo) {
    mSelFrom = findEarliest(msFrom, mSelFrom);
    mSelTo = findLatest(msUpTo, mSelTo);
    bool result = false;
    for (int i = mSelFrom; i <= mSelTo; i++) {
        if (!mHits[i]) {
            mMisses[i] = true;
            result = true;
        }
    }
    return result;
}

TalkyMatcher::TalkyMatcher() { memset(mBuffer, 0, sizeof(mBuffer)); }

void TalkyMatcher::updateScoring(float f) {
    if (mVoiceBeat.mTriggered) {
        mRefEvents.Hit(f - 180.0f, f + 180.0f, f);
        mVoiceBeat.ClearEventList();
    }
    std::vector<double> unused;
    mRefEvents.Miss(f - 120.0f, f - 60.0f);
    mVoiceBeat.ClearTrigger();
}

void TalkyMatcher::LoadEvents(
    const std::vector<float> &times, const std::vector<float> &peaks
) {
    mRefEvents.mTimes = times;
    mRefEvents.mPeaks = peaks;
    mRefEvents.Reset();
}

void TalkyMatcher::Reset() { mVoiceBeat.Reset(); }

void TalkyMatcher::Analyze(const short *samples, int numSamples, float ms) {
    if (numSamples > 0x3000) numSamples = 0x3000;
    int n3 = numSamples / 3;
    for (int i = 0; i < n3; i++) {
        mBuffer[i] = (float)samples[i * 3] / 32767.0f;
    }
    mVoiceBeat.Analyze(mBuffer, n3, false, true, ms + 6.0f);
    if (!mRefEvents.mTimes.empty()) {
        updateScoring(ms);
    }
}

void TalkyMatcher::SetEnableTalkyMatcher(bool enable) { mVoiceBeat.SetEnable(enable); }
